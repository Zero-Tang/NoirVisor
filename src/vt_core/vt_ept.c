/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the basic driver of Intel EPT.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_ept.c
*/

#include <nvdef.h>
#include <nvstatus.h>
#include <nvbdk.h>
#include <vt_intrin.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include <ci.h>
#include "vt_vmcs.h"
#include "vt_ept.h"
#include "vt_def.h"

#if !defined(_hv_type1)
bool nvc_ept_insert_pte(noir_ept_manager_p eptm,noir_hook_page_p nhp)
{
	for(u32 h=0;h<noir_hook_pages_count;h++)
	{
		noir_ept_pte_descriptor_p cur_d=eptm->pte.head;
		ia32_addr_translator addr;
		addr.value=nhp[h].orig.phys;
		while(cur_d)
		{
			// Find if the desired 2MiB page has already been described.
			if(nhp[h].orig.phys>=cur_d->gpa_start && nhp[h].orig.phys<cur_d->gpa_start+page_2mb_size)
				break;
			cur_d=cur_d->next;
		}
		if(cur_d==null)
		{
			// The desired page is not described.
			// Describe the page, then perform hooking.
			u32 index=(u32)((addr.pdpte_offset<<9)+addr.pde_offset);
			ia32_ept_pde_p pde_p=(ia32_ept_pde_p)&eptm->pde.virt[index];
			u32 i=0;
			cur_d=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
			if(cur_d==null)return false;
			cur_d->virt=noir_alloc_contd_memory(page_size);
			if(cur_d->virt==null)return false;
			cur_d->phys=noir_get_physical_address(cur_d->virt);
			cur_d->gpa_start=(addr.pdpte_offset<<18)+(addr.pde_offset<<9);
			// Setup identity map.
			for(;i<512;i++)
			{
				cur_d->virt[i].value=0;
				cur_d->virt[i].read=1;
				cur_d->virt[i].write=1;
				cur_d->virt[i].execute=1;
				cur_d->virt[i].memory_type=ia32_write_back;
				cur_d->virt[i].page_offset=cur_d->gpa_start+i;
			}
			cur_d->gpa_start<<=12;
			// Insert to descriptor linked-list.
			if(eptm->pte.head)
			{
				eptm->pte.tail->next=cur_d;
				eptm->pte.tail=cur_d;
			}
			else
			{
				eptm->pte.head=cur_d;
				eptm->pte.tail=cur_d;
			}
			// Reset the Page-Directory Entry
			pde_p->reserved0=0;
			pde_p->large_pde=0;
			pde_p->pte_offset=cur_d->phys>>12;
		}
		// At this moment, we assume the desired page is described.
		nhp[h].pte_descriptor=(void*)&cur_d->virt[addr.pte_offset];
		// Reset the page translation.
		cur_d->virt[addr.pte_offset].page_offset=nhp[h].hook.phys>>12;
		// Clear R/W but Reserve X.
		cur_d->virt[addr.pte_offset].read=0;
		cur_d->virt[addr.pte_offset].write=0;
	}
	return true;
}
#endif

void nvc_ept_cleanup(noir_ept_manager_p eptm)
{
	if(eptm)
	{
		if(eptm->eptp.virt)
			noir_free_contd_memory(eptm->eptp.virt,page_size);
		if(eptm->pdpt.virt)
			noir_free_contd_memory(eptm->pdpt.virt,page_size);
		if(eptm->pde.virt)
			noir_free_2mb_page(eptm->pde.virt);
		if(eptm->pte.head)
		{
			noir_ept_pte_descriptor_p cur=eptm->pte.head;
			do
			{
				noir_ept_pte_descriptor_p next=cur->next;
				noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}while(cur);
		}
		if(eptm->blank_page.virt)
			noir_free_contd_memory(eptm->blank_page.virt,page_size);
		noir_free_nonpg_memory(eptm);
	}
}

/*
	According to the rule of variable MTRR precedence:

	1. If one of the overlapped region is UC, then the memory type will be UC.
	2. If two or more overlapped region are WT and WB, then the memory type will be WT.
	3. If the overlapped regions do not match rule 1 and 2, then behavior of processor is undefined.

	Therefore, we may simply compare the value of memory type.
	The final value of the memory type will be the one that have smallest value.
*/
bool nvc_ept_update_pde_memory_type(noir_ept_manager_p eptm,u64 hpa,u8 memory_type,bool regardless)
{
	ia32_addr_translator trans;
	trans.value=hpa;
	if(trans.pml4e_offset==0)
	{
		u32 index=(u32)((trans.pdpte_offset<<9)+trans.pde_offset);
		// Note that for standard PDEs, the memory type field is reserved.
		// If set, EPT Misconfiguration would occur.
		if(eptm->pde.virt[index].large_pde)
		{
			if(regardless || eptm->pde.virt[index].ignored || memory_type<eptm->pde.virt[index].memory_type)
				eptm->pde.virt[index].memory_type=memory_type;
			eptm->pde.virt[index].ignored0=true;				// Use the ignored bit to indicate that this page is marked by MTRR.
		}
		else
		{
			// For standard PDEs (i.e: which describe PTEs instead of large pages),
			// set the memory types for all 512 corresponding PTEs.
			noir_ept_pte_descriptor_p cur=eptm->pte.head;
			while(cur)
			{
				if(hpa>=cur->gpa_start && hpa<cur->gpa_start+page_2mb_size)
				{
					for(u32 i=0;i<512;i++)
					{
						if(regardless || cur->virt[index].ignored1 || memory_type<cur->virt[index].memory_type)
							cur->virt[index].memory_type=memory_type;
						cur->virt[index].ignored1=true;			// Use the ignored bit to indicate that this page is marked by MTRR.
					}
				}
				cur=cur->next;
			}
		}
		return true;
	}
	return false;
}

bool nvc_ept_update_pde(noir_ept_manager_p eptm,u64 hpa,bool r,bool w,bool x)
{
	ia32_addr_translator trans;
	trans.value=hpa;
	if(trans.pml4e_offset==0)
	{
		u32 index=(u32)((trans.pdpte_offset<<9)+trans.pde_offset);
		eptm->pde.virt[index].read=r;
		eptm->pde.virt[index].write=w;
		eptm->pde.virt[index].execute=x;
		return true;
	}
	return false;
}

bool nvc_ept_update_pte_memory_type(noir_ept_manager_p eptm,u64 hpa,u8 memory_type)
{
	ia32_addr_translator trans;
	noir_ept_pte_descriptor_p pte_p=eptm->pte.head;
	trans.value=hpa;
	// Do not accept address higher than 512GB.
	if(trans.pml4e_offset)return false;
	while(pte_p)
	{
		if(hpa>=pte_p->gpa_start && hpa<pte_p->gpa_start+page_2mb_size)
		{
			pte_p->virt[trans.pte_offset].memory_type=memory_type;
			return true;
		}
		pte_p=pte_p->next;
	}
	// The 2MB page was not described yet.
	pte_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
	if(pte_p)
	{
		pte_p->virt=noir_alloc_contd_memory(page_size);
		if(pte_p->virt)
		{
			u64 index=(trans.pdpte_offset<<9)+trans.pde_offset;
			ia32_ept_pde_p pde_p=(ia32_ept_pde_p)&eptm->pde.virt[index];
			// PTE Descriptor
			pte_p->phys=noir_get_physical_address(pte_p->virt);
			pte_p->gpa_start=index<<9;
			for(u32 i=0;i<512;i++)
			{
				pte_p->virt[i].read=pde_p->read;
				pte_p->virt[i].write=pde_p->write;
				pte_p->virt[i].execute=pde_p->execute;
				pte_p->virt[i].memory_type=pde_p->reserved0&7;
				pte_p->virt[i].page_offset=pte_p->gpa_start+i;
			}
			pte_p->gpa_start<<=12;
			// Set specific page memory type.
			pte_p->virt[trans.pte_offset].memory_type=memory_type;
			// Update PDE
			pde_p->reserved0=0;
			pde_p->large_pde=0;
			pde_p->pte_offset=pte_p->phys>>page_shift;
			// Update the linked list.
			if(unlikely(eptm->pte.head==null))
				eptm->pte.head=eptm->pte.tail=pte_p;
			else
			{
				eptm->pte.tail->next=pte_p;
				eptm->pte.tail=pte_p;
			}
			return true;
		}
		noir_free_nonpg_memory(pte_p);
	}
	return false;
}

bool nvc_ept_update_pte(noir_ept_manager_p eptm,u64 hpa,u64 gpa,bool r,bool w,bool x)
{
	ia32_addr_translator hat,gat;
	noir_ept_pte_descriptor_p pte_p=eptm->pte.head;
	hat.value=hpa;
	gat.value=gpa;
	// We cannot accept address higher than 512GB here.
	if(hat.pml4e_offset || gat.pml4e_offset)return false;
	while(pte_p)
	{
		if(hpa>=pte_p->gpa_start && hpa<pte_p->gpa_start+page_2mb_size)
		{
			// Then the 2MB page is already described as PTE.
			pte_p->virt[hat.pte_offset].read=r;
			pte_p->virt[hat.pte_offset].write=w;
			pte_p->virt[hat.pte_offset].execute=x;
			pte_p->virt[hat.pte_offset].page_offset=gpa>>12;
			return true;
		}
		pte_p=pte_p->next;
	}
	// The 2MB page was not described as PTE yet.
	pte_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
	if(pte_p)
	{
		pte_p->virt=noir_alloc_contd_memory(page_size);
		if(pte_p->virt)
		{
			u64 index=(hat.pdpte_offset<<9)+hat.pde_offset;
			ia32_ept_pde_p pde_p=(ia32_ept_pde_p)&eptm->pde.virt[index];
			// PTE Descriptor
			pte_p->phys=noir_get_physical_address(pte_p->virt);
			pte_p->gpa_start=index<<9;
			for(u32 i=0;i<512;i++)
			{
				pte_p->virt[i].read=1;
				pte_p->virt[i].write=1;
				pte_p->virt[i].execute=1;
				pte_p->virt[i].memory_type=pde_p->reserved0&7;
				pte_p->virt[i].page_offset=pte_p->gpa_start+i;
			}
			pte_p->gpa_start<<=12;
			// Page Attributes
			pte_p->virt[hat.pte_offset].read=r;
			pte_p->virt[hat.pte_offset].write=w;
			pte_p->virt[hat.pte_offset].execute=x;
			pte_p->virt[hat.pte_offset].page_offset=gpa>>12;
			// Update PDE
			pde_p->reserved0=0;
			pde_p->large_pde=0;
			pde_p->pte_offset=pte_p->phys>>12;
			// Update the linked list.
			if(unlikely(eptm->pte.head==null))
				eptm->pte.head=eptm->pte.tail=pte_p;
			else
			{
				eptm->pte.tail->next=pte_p;
				eptm->pte.tail=pte_p;
			}
			return true;
		}
		noir_free_nonpg_memory(pte_p);
	}
	return false;
}

void static nvc_ept_update_per_var_mtrr(noir_ept_manager_p eptm,u32 mtrr_msr_index)
{
	ia32_mtrr_phys_mask_msr phys_mask;
	phys_mask.value=noir_rdmsr(mtrr_msr_index+1);
	if(phys_mask.valid)
	{
		ia32_mtrr_phys_base_msr phys_base;
		phys_base.value=noir_rdmsr(mtrr_msr_index);
		// Ignore MTRRs that defines the same memory type as default MTRR.
		if(phys_base.type!=eptm->def_type.type)
		{
			u32 min_bitp1,min_bitp2;
			u8 b1,b2;
			// Determine the length of range.
			b1=noir_bsf64(&min_bitp1,phys_mask.phys_mask);
			b2=noir_bsf64(&min_bitp2,phys_base.phys_base);
			// If the MTRR specified a range that aligned under 2MiB, we can't proceed.
			if((b1 && min_bitp1<page_shift_diff) || (b2 && min_bitp2<page_shift_diff))
			{
				nv_panicf("MTRR MSR 0x%X is aligned under 2MiB!\n",mtrr_msr_index);
				noir_int3();
			}
			else
			{
				// Determine the comparison basis.
				const u64 mask_phys=phys_mask.phys_mask<<page_shift;
				const u64 mask_base=(phys_base.phys_base&phys_mask.phys_mask)<<page_shift;
				// This is exhaustive but precision-oriented algorithm.
				// The highest memory we can reach is 512GiB.
				for(u64 mask_addr=0;mask_addr<(1i64<<page_512gb_shift);mask_addr+=page_2mb_size)
				{
					const u64 mask_target=mask_phys&mask_addr;
					if(mask_target==mask_base)nvc_ept_update_pde_memory_type(eptm,mask_addr,(u8)phys_base.type,false);
				}
			}
		}
	}
}

// This procedure is to be invoked on a per-processor basis.
void nvc_ept_update_by_mtrr(noir_ept_manager_p eptm)
{
	ia32_mtrr_cap_msr mtrr_cap;
	noir_ept_pte_descriptor_p pte_p=eptm->pte.head;
	// Get the default memory type.
	eptm->def_type.value=noir_rdmsr(ia32_mtrr_def_type);
	// Traverse all EPT Paging Entries and set corresponding memory types.
	// Set memory types for all large-page PDEs.
	for(u32 i=0;i<0x40000;i++)
	{
		if(eptm->pde.virt[i].large_pde)
		{
			eptm->pde.virt[i].memory_type=eptm->def_type.type;
			eptm->pde.virt[i].ignored0=0;
		}
	}
	// Set memory types for all PTEs.
	while(pte_p)
	{
		for(u32 i=0;i<512;i++)
		{
			pte_p->virt[i].memory_type=eptm->def_type.type;
			pte_p->virt[i].ignored1=0;
		}
		pte_p=pte_p->next;
	}
	// Read MTRR capabilities.
	mtrr_cap.value=noir_rdmsr(ia32_mtrr_cap);
	// Traverse variable-range MTRRs.
	for(u32 i=0;i<mtrr_cap.variable_count;i++)
		nvc_ept_update_per_var_mtrr(eptm,ia32_mtrr_phys_base0+(i<<1));
	// By the way, set the SMRR.
	if(mtrr_cap.support_smrr)
		nvc_ept_update_per_var_mtrr(eptm,ia32_smrr_phys_base);
	// Traverse fixed-range MTRRs.
	if(eptm->def_type.fix_enabled)
	{
		u8* type;
		// Read Fixed Range MTRRs.
		// All Fixed Range MTRRs span the first MiB of system memory.
		u64 fix64k_00000=noir_rdmsr(ia32_mtrr_fix64k_00000);
		u64 fix16k_80000=noir_rdmsr(ia32_mtrr_fix16k_80000);
		u64 fix16k_a0000=noir_rdmsr(ia32_mtrr_fix16k_a0000);
		u64 fix4k_c0000=noir_rdmsr(ia32_mtrr_fix4k_c0000);
		u64 fix4k_c8000=noir_rdmsr(ia32_mtrr_fix4k_c8000);
		u64 fix4k_d0000=noir_rdmsr(ia32_mtrr_fix4k_d0000);
		u64 fix4k_d8000=noir_rdmsr(ia32_mtrr_fix4k_d8000);
		u64 fix4k_e0000=noir_rdmsr(ia32_mtrr_fix4k_e0000);
		u64 fix4k_e8000=noir_rdmsr(ia32_mtrr_fix4k_e8000);
		u64 fix4k_f0000=noir_rdmsr(ia32_mtrr_fix4k_f0000);
		u64 fix4k_f8000=noir_rdmsr(ia32_mtrr_fix4k_f8000);
		// MTRR Fixed64K 00000
		// This register specifies eight 64KiB ranges, 512KiB in total.
		type=(u8*)&fix64k_00000;
		for(u32 i=0;i<8;i++)
			for(u32 j=0;j<16;j++)	// 64KiB is actually 16 pages.
				eptm->pte.head->virt[(i<<4)+j].memory_type=type[i];
				// if(nvc_ept_update_pte_memory_type(eptm,(i<<16)+(j<<12),type[i])==false)return false;
		// MTRR Fixed16K_80000
		// This register specifies eight 16KiB ranges, 128KiB in total.
		type=(u8*)&fix16k_80000;
		for(u32 i=0;i<8;i++)
			for(u32 j=0;j<4;j++)	// 16KiB is actually 4 pages.
				eptm->pte.head->virt[0x80+(i<<2)+j].memory_type=type[i];
				// if(nvc_ept_update_pte_memory_type(eptm,0x80000+(i<<14)+(j<<12),type[i])==false)return false;
		// MTRR Fixed16K_a0000
		// This register specifies eight 16KiB ranges, 128KiB in total.
		type=(u8*)&fix16k_a0000;
		for(u32 i=0;i<8;i++)
			for(u32 j=0;j<4;j++)	// 16KiB is actually 4 pages.
				eptm->pte.head->virt[0xa0+(i<<2)+j].memory_type=type[i];
				// if(nvc_ept_update_pte_memory_type(eptm,0xa0000+(i<<14)+(j<<12),type[i])==false)return false;
		// MTRR Fixed4K_c0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_c0000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xc0+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xc0000+(i<<12),type[i])==false)return false;
		// MTRR Fixed4K_c8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_c8000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xc8+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xc8000+(i<<12),type[i])==false)return false;
		// MTRR Fixed4K_d0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_d0000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xd0+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xd0000+(i<<12),type[i])==false)return false;
		// MTRR Fixed4K_d8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_d8000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xd8+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xd8000+(i<<12),type[i])==false)return false;
		// MTRR Fixed4K_e0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_e0000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xe0+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xe0000+(i<<12),type[i])==false)return false;
		// MTRR Fixed4K_e8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_e8000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xe8+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xe8000+(i<<12),type[i])==false)return false;
		// MTRR Fixed4K_f0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_f0000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xf0+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xf0000+(i<<12),type[i])==false)return false;
		// MTRR Fixed4K_f8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_f8000;
		for(u32 i=0;i<8;i++)
			eptm->pte.head->virt[0xf8+i].memory_type=type[i];
			// if(nvc_ept_update_pte_memory_type(eptm,0xf8000+(i<<12),type[i])==false)return false;
	}
	// return true;
}

bool nvc_ept_initialize_ci(noir_ept_manager_p eptm)
{
	bool r=true;
	for(u32 i=0;i<noir_ci->pages;i++)
	{
		u64 phys=noir_ci->page_ci[i].phys;
		r&=nvc_ept_update_pte(eptm,phys,phys,true,false,true);
		if(!r)break;
	}
	return r;
}

/*
  It is important that the hypervisor essentials should be protected.
  Here, one thing should be clarified: even if the guest cannot execute
  vmread or vmwrite to tamper the VMCS, guest may use memory instructions
  to modify the VMCS state. It is possible that a malware knows the
  format of VMCS on a specific processor. To eliminate the risk, we must
  protect the VMCS, VMXON area and so on through the Intel EPT.

  By Now, any read/write to the protected area would be redirected a
  page that is purposefully and deliberately left blank.
  This design could reduce VM-Exit of EPT violation.
*/
bool nvc_ept_protect_hypervisor(noir_hypervisor_p hvm,noir_ept_manager_p eptm)
{
	// First of all, allocate a blank page.
	eptm->blank_page.virt=noir_alloc_contd_memory(page_size);
	if(eptm->blank_page.virt)
	{
		bool result=true;
		eptm->blank_page.phys=noir_get_physical_address(eptm->blank_page.virt);
		// Protect MSR, I/O bitmap, and MSR Auto List.
		result&=nvc_ept_update_pte(eptm,hvm->relative_hvm->msr_bitmap.phys,eptm->blank_page.phys,true,true,true);
		// nvc_ept_update_pte(eptm,hvm->relative_hvm->io_bitmap_a.phys,eptm->blank_page.phys,true,true,true);
		// nvc_ept_update_pte(eptm,hvm->relative_hvm->io_bitmap_b.phys,eptm->blank_page.phys,true,true,true);
		for(u32 i=0;i<hvm->cpu_count;i++)
		{
			noir_vt_vcpu_p vcpu=&hvm->virtual_cpu[i];
			noir_ept_manager_p eptmt=(noir_ept_manager_p)vcpu->ept_manager;
			noir_ept_pte_descriptor_p cur=null;
			// Protect VMXON region and VMCS.
			result&=nvc_ept_update_pte(eptm,vcpu->vmxon.phys,eptm->blank_page.phys,true,true,true);
			result&=nvc_ept_update_pte(eptm,vcpu->vmcs.phys,eptm->blank_page.phys,true,true,true);
			// Protect MSR Auto List.
			result&=nvc_ept_update_pte(eptm,vcpu->msr_auto.phys,eptm->blank_page.phys,true,true,true);
			// Protect EPT Paging Structure.
			result&=nvc_ept_update_pte(eptm,eptmt->eptp.phys.value,eptm->blank_page.phys,true,true,true);
			result&=nvc_ept_update_pte(eptm,eptmt->pdpt.phys,eptm->blank_page.phys,true,true,true);
			// Allow Guest read the original PDE so that EPT-violation VM-Exits can be reduced.
			result&=nvc_ept_update_pde(eptm,eptmt->pde.phys,true,false,false);
			// Update PTEs of paging structure.
			for(cur=eptm->pte.head;cur;cur=cur->next)
				result&=nvc_ept_update_pte(eptm,cur->phys,eptm->blank_page.phys,true,true,true);
		}
		return result;
	}
	return false;
}

/*
  Introduction to Identity map:

  Intel EPT introduces a built-in GPA to HPA technology.
  For NoirVisor, we translate GPA to HPA with GPA=HPA term.
  Essentials: {GPA=HPA} (Important).
  This is key purpose of identity map.
  We use EPT for advanced feature - access filtering.

  NoirVisor's designation supports 512GB physical memory in total.

  Memory Consumption in Paging of each vCPU:
  4KB for 1 PML4E page - only 1 entry is used for mapping 512GB physical memory.
  4KB for 1 PDPTE page - all 512 entries are used for mapping 512*1GB=512GB physical memory.
  2MB for 512 PDE pages - all 262144 entries are used for mapping 512*512*2MB=512GB physical memory.
  We will allocate the PDEs on a single 2MB-aligned page.
*/
noir_ept_manager_p nvc_ept_build_identity_map()
{
	bool alloc_success=false;
	noir_ept_manager_p eptm=noir_alloc_nonpg_memory(sizeof(noir_ept_manager));
	if(eptm)
	{
		eptm->eptp.virt=noir_alloc_contd_memory(page_size);
		if(eptm->eptp.virt)
		{
			eptm->pdpt.virt=noir_alloc_contd_memory(page_size);
			if(eptm->pdpt.virt)
			{
				eptm->pde.virt=noir_alloc_2mb_page();
				if(eptm->pde.virt)
				{
					eptm->pde.phys=noir_get_physical_address(eptm->pde.virt);
					eptm->pte.head=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
					if(eptm->pte.head)
					{
						eptm->pte.head->virt=noir_alloc_contd_memory(page_size);
						if(eptm->pte.head->virt)
						{
							eptm->pte.head->phys=noir_get_physical_address(eptm->pte.head->virt);
							alloc_success=true;
						}
						eptm->pte.tail=eptm->pte.head;
					}
				}
			}
		}
	}
	if(alloc_success)
	{
		ia32_ept_pde_p pde=(ia32_ept_pde_p)eptm->pde.virt;
		u32 a;
		noir_cpuid(ia32_cpuid_ext_pcap_prm_eid,0,&a,null,null,null);
		eptm->phys_addr_size=a&0xff;
		eptm->virt_addr_size=(a<<8)&0xff;
		for(u32 i=0;i<512;i++)
		{
			for(u32 j=0;j<512;j++)
			{
				const u32 k=(i<<9)+j;
				// Build Page-Directory Entries (PDEs)
				eptm->pde.virt[k].value=0;
				eptm->pde.virt[k].page_offset=k;
				eptm->pde.virt[k].read=1;
				eptm->pde.virt[k].write=1;
				eptm->pde.virt[k].execute=1;
				eptm->pde.virt[k].large_pde=1;
			}
			// Build Page-Directory-Pointer-Table Entries (PDPTEs)
			eptm->pdpt.virt[i].value=0;
			eptm->pdpt.virt[i].pde_offset=(eptm->pde.phys>>page_shift)+i;
			eptm->pdpt.virt[i].read=1;
			eptm->pdpt.virt[i].write=1;
			eptm->pdpt.virt[i].execute=1;
		}
		// Initialize the first page for Fixed-Range MTRRs.
		pde->large_pde=0;
		pde->pte_offset=eptm->pte.head->phys>>page_shift;
		for(u32 i=0;i<512;i++)
		{
			eptm->pte.head->virt[i].value=0;
			eptm->pte.head->virt[i].read=1;
			eptm->pte.head->virt[i].write=1;
			eptm->pte.head->virt[i].execute=1;
			eptm->pte.head->virt[i].page_offset=i;
		}
#if !defined(_hv_type1)
		if(hvm_p->options.stealth_inline_hook)
			if(nvc_ept_insert_pte(eptm,noir_hook_pages)==false)
				goto alloc_failure;
#endif
		if(nvc_ept_initialize_ci(eptm)==false)
			goto alloc_failure;
		// Build Page Map Level-4 Entry (PML4E)
		eptm->pdpt.phys=noir_get_physical_address(eptm->pdpt.virt);
		eptm->eptp.virt->value=0;
		eptm->eptp.virt->pdpte_offset=eptm->pdpt.phys>>page_shift;
		eptm->eptp.virt->read=1;
		eptm->eptp.virt->write=1;
		eptm->eptp.virt->execute=1;
		// Make EPT-Pointer (EPTP)
		eptm->eptp.phys.value=noir_get_physical_address(eptm->eptp.virt);
		eptm->eptp.phys.memory_type=ia32_write_back;
		eptm->eptp.phys.walk_length=3;
	}
	else
	{
alloc_failure:
		nvc_ept_cleanup(eptm);
		nv_dprintf("Allocation Failure! Failed to build EPT paging structure!\n");
		eptm=null;
	}
	return eptm;
}