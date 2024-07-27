/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

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

void nvc_ept_cleanup(noir_ept_manager_p eptm)
{
	if(eptm)
	{
		if(eptm->eptp.virt)
			noir_free_contd_memory(eptm->eptp.virt,page_size);
		if(eptm->pdpt.virt)
			noir_free_2mb_page(eptm->pdpt.virt);
		if(eptm->pde.head)
		{
			noir_ept_pde_descriptor_p cur=eptm->pde.head;
			do
			{
				noir_ept_pde_descriptor_p next=cur->next;
				noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}while(cur);
		}
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

// Split 1GiB Page into 512 2MiB Pages.
noir_ept_pde_descriptor_p nvc_ept_split_pdpte(noir_ept_manager_p eptm,u64 gpa,bool host,bool alloc)
{
	// Traverse PDE Descriptor Linked-List.
	noir_ept_pde_descriptor_p pde_p=eptm->pde.head;
	while(pde_p)
	{
		if(gpa>=pde_p->gpa_start && gpa<pde_p->gpa_start+page_1gb_size)
			break;		// This PDPTE is already splitted.
		pde_p=pde_p->next;
	}
	if(pde_p==null && alloc==true)
	{
		// This 1GiB page has not been described yet.
		nvd_printf("Splitting PDPTE for GPA 0x%016llX...\n",gpa);
		pde_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pde_descriptor));
		if(pde_p)
		{
			pde_p->virt=noir_alloc_contd_memory(page_size);
			if(pde_p->virt)
			{
				const u64 index=page_1gb_count(gpa);
				// PDE Descriptor
				pde_p->phys=noir_get_physical_address(pde_p->virt);
				pde_p->gpa_start=index<<page_shift_diff64;
				for(u32 i=0;i<page_table_entries64;i++)
				{
					pde_p->large[i].read=true;
					pde_p->large[i].write=true;
					pde_p->large[i].execute=true;
					pde_p->large[i].memory_type=eptm->pdpt.virt[index].memory_type;
					pde_p->large[i].large_pde=true;
					pde_p->large[i].var_mtrr_covered=eptm->pdpt.virt[index].var_mtrr_covered;
					pde_p->large[i].page_offset=pde_p->gpa_start+i;
				}
				pde_p->gpa_start<<=page_2mb_shift;
				// Append to Linked-List
				if(eptm->pde.head && eptm->pde.tail)
				{
					eptm->pde.tail->next=pde_p;
					eptm->pde.tail=pde_p;
				}
				else
				{
					eptm->pde.head=pde_p;
					eptm->pde.tail=pde_p;
				}
				goto update_pdpte;
			}
			noir_free_nonpg_memory(pde_p);
		}
	}
update_pdpte:
	// If in host mode, update PDPTE.
	if(host)
	{
		const u64 index=page_1gb_count(gpa);
		ia32_ept_pdpte_p pdpte_p=(ia32_ept_pdpte_p)&eptm->pdpt.virt[index];
		pdpte_p->reserved0=0;
		pdpte_p->pde_offset=page_4kb_count(pde_p->phys);
	}
	return pde_p;
}

// Split 2MiB Page into 512 4KiB Pages.
noir_ept_pte_descriptor_p nvc_ept_split_pde(noir_ept_manager_p eptm,u64 gpa,bool host,bool alloc)
{
	noir_ept_pte_descriptor_p pte_p=eptm->pte.head;
	while(pte_p)
	{
		if(gpa>=pte_p->gpa_start && gpa<pte_p->gpa_start+page_2mb_size)
			break;		// The 2MiB page has been described.
		pte_p=pte_p->next;
	}
	if(alloc==true && pte_p==null)
	{
		// The 2MiB page has not been described yet.
		nvd_printf("Splitting PDE for GPA 0x%016llX...\n",gpa);
		pte_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
		if(pte_p)
		{
			pte_p->virt=noir_alloc_contd_memory(page_size);
			if(pte_p->virt)
			{
				// Split the PDPTE first.
				noir_ept_pde_descriptor_p pde_p=nvc_ept_split_pdpte(eptm,gpa,host,true);
				if(pde_p)
				{
					const u64 pfn_index=page_2mb_count(gpa);
					const u64 pde_index=page_entry_index64(pfn_index);
					// PTE Descriptor
					pte_p->phys=noir_get_physical_address(pte_p->virt);
					pte_p->gpa_start=pfn_index<<page_shift_diff64;
					for(u32 i=0;i<page_table_entries64;i++)
					{
						pte_p->virt[i].read=true;
						pte_p->virt[i].write=true;
						pte_p->virt[i].execute=true;
						pte_p->virt[i].memory_type=pde_p->large[pde_index].memory_type;
						pte_p->virt[i].var_mtrr_covered=pde_p->large[pde_index].var_mtrr_covered;
						pte_p->virt[i].page_offset=pte_p->gpa_start+i;
					}
					pte_p->gpa_start<<=page_4kb_shift;
					// Append to Linked-List.
					if(eptm->pte.head && eptm->pte.tail)
					{
						eptm->pte.tail->next=pte_p;
						eptm->pte.tail=pte_p;
					}
					else
					{
						eptm->pte.head=pte_p;
						eptm->pte.tail=pte_p;
					}
					goto update_pde;
				}
				noir_free_contd_memory(pte_p->virt,page_size);
			}
			noir_free_nonpg_memory(pte_p);
		}
	}
update_pde:
	if(host)
	{
		// If in host mode, update PDE.
		noir_ept_pde_descriptor_p pde_p=nvc_ept_split_pdpte(eptm,gpa,host,alloc);
		if(pde_p)
		{
			const u64 pfn_index=page_2mb_count(gpa);
			const u64 pde_index=page_entry_index64(pfn_index);
			pde_p->virt[pde_index].reserved0=0;
			pde_p->virt[pde_index].pte_offset=page_4kb_count(pte_p->phys);
		}
	}
	return pte_p;
}

// The merge function is intended for setting new cache type for memory.
u8 nvc_ept_merge_memory_type(u8 old_type,u8 new_type,bool force)
{
	// Set the new memory type due to higher precedence (e.g.: Fixed MTRR precedes Variable MTRR).
	if(force)return new_type;
	// If no precedence, compare the memory type.
	// The order is UC>WC>WT>WB, so simply compare the value.
	if(old_type<new_type)return old_type;
	return new_type;
}

bool nvc_ept_update_pte_memory_type(noir_ept_manager_p eptm,u64 gpa,u8 memory_type,bool force)
{
	noir_ept_pte_descriptor_p pte_p=nvc_ept_split_pde(eptm,gpa,true,true);
	if(pte_p)
	{
		ia32_addr_translator gat;
		gat.value=gpa;
		// Simply update this PTE entry.
		pte_p->virt[gat.pte_offset].memory_type=pte_p->virt[gat.pte_offset].var_mtrr_covered?nvc_ept_merge_memory_type((u8)pte_p->virt[gat.pte_offset].memory_type,memory_type,force):memory_type;
		// Mark this PTE is covered by variable MTRR.
		pte_p->virt[gat.pte_offset].var_mtrr_covered=true;
		return true;
	}
	return false;
}

bool nvc_ept_update_pde_memory_type(noir_ept_manager_p eptm,u64 gpa,u8 memory_type,bool force)
{
	noir_ept_pde_descriptor_p pde_p=nvc_ept_split_pdpte(eptm,gpa,true,true);
	if(pde_p)
	{
		ia32_addr_translator gat;
		gat.value=gpa;
		if(pde_p->large[gat.pde_offset].large_pde)
		{
			// If this is Large PDE, simply update the PDE.
			pde_p->large[gat.pde_offset].memory_type=pde_p->large[gat.pde_offset].var_mtrr_covered?nvc_ept_merge_memory_type((u8)pde_p->large[gat.pde_offset].memory_type,memory_type,force):memory_type;
			// Mark this PDE is covered by variable MTRR.
			pde_p->large[gat.pde_offset].var_mtrr_covered=true;
			return true;
		}
		else
		{
			// Otherwise, update all underlying PTE entries.
			for(u32 i=0;i<page_table_entries64;i++)
			{
				bool result=nvc_ept_update_pte_memory_type(eptm,gpa+page_4kb_mult(i),memory_type,force);
				if(!result)return false;
			}
		}
	}
	return false;
}

bool nvc_ept_update_pdpte_memory_type(noir_ept_manager_p eptm,u64 gpa,u8 memory_type,bool force)
{
	const u64 index=page_1gb_count(gpa);
	if(eptm->pdpt.virt[index].huge_pdpte)
	{
		// If this is Huge PDPTE, simply update the PDPTE.
		eptm->pdpt.virt[index].memory_type=eptm->pdpt.virt[index].var_mtrr_covered?nvc_ept_merge_memory_type((u8)eptm->pdpt.virt[index].memory_type,memory_type,force):memory_type;
		// Mark this PDPTE is covered by variable MTRR.
		eptm->pdpt.virt[index].var_mtrr_covered=true;
		return true;
	}
	else
	{
		// Otherwise, update all underlying PDE entries.
		for(u32 i=0;i<page_table_entries64;i++)
		{
			bool result=nvc_ept_update_pde_memory_type(eptm,gpa+page_2mb_mult(i),memory_type,force);
			if(!result)return false;
		}
		return true;
	}
}

bool nvc_ept_update_pdpte(noir_ept_manager_p eptm,u64 hpa,u64 gpa,bool r,bool w,bool x,bool h,bool ignore_mt,u8 memory_type,bool alloc)
{
	const u64 index=page_1gb_count(gpa);
	ia32_ept_pdpte_p pdpte_p=(ia32_ept_pdpte_p)&eptm->pdpt.virt[index];
	if(h)
	{
		// Reset to huge PDPTE.
		if(!ignore_mt)
			eptm->pdpt.virt[index].memory_type=memory_type;
		eptm->pdpt.virt[index].huge_pdpte=true;
		eptm->pdpt.virt[index].page_offset=page_1gb_count(hpa);
	}
	else
	{
		// Split the PDPTE.
		// If it is previously splitted, restore the mapping.
		noir_ept_pde_descriptor_p pde_p=nvc_ept_split_pdpte(eptm,gpa,true,alloc);
		if(!pde_p)return false;
		pdpte_p->reserved0=0;
		pdpte_p->pde_offset=page_4kb_count(pde_p->phys);
	}
	pdpte_p->read=r;
	pdpte_p->write=w;
	pdpte_p->execute=x;
	return true;
}

bool nvc_ept_update_pde(noir_ept_manager_p eptm,u64 hpa,u64 gpa,bool r,bool w,bool x,bool l,bool ignore_mt,u8 memory_type,bool alloc)
{
	// Split the PDPTE.
	noir_ept_pde_descriptor_p pde_p=nvc_ept_split_pdpte(eptm,gpa,true,alloc);
	if(pde_p)
	{
		ia32_addr_translator gat;
		gat.value=gpa;
		if(l)
		{
			// Reset to large PDE.
			if(ignore_mt)
				pde_p->large[gat.pde_offset].memory_type=memory_type;
			pde_p->large[gat.pde_offset].large_pde=true;
			pde_p->large[gat.pde_offset].page_offset=page_2mb_count(hpa);
		}
		else
		{
			// Split the PDE.
			// If it is previously splitted, restore the mapping.
			noir_ept_pte_descriptor_p pte_p=nvc_ept_split_pde(eptm,gpa,true,alloc);
			if(!pte_p)return false;
			pde_p->virt[gat.pde_offset].reserved0=0;
			pde_p->virt[gat.pde_offset].pte_offset=page_count(pte_p->phys);
		}
		pde_p->virt[gat.pde_offset].read=r;
		pde_p->virt[gat.pde_offset].write=w;
		pde_p->virt[gat.pde_offset].execute=x;
		return true;
	}
	return false;
}

noir_ept_pte_descriptor_p nvc_ept_update_pte(noir_ept_manager_p eptm,u64 hpa,u64 gpa,bool r,bool w,bool x,bool ignore_mt,u8 memory_type,bool alloc)
{
	// Split the PDE
	noir_ept_pte_descriptor_p pte_p=nvc_ept_split_pde(eptm,gpa,true,alloc);
	if(pte_p)
	{
		ia32_addr_translator gat;
		gat.value=gpa;
		// Update the specific PTE.
		pte_p->virt[gat.pte_offset].read=r;
		pte_p->virt[gat.pte_offset].write=w;
		pte_p->virt[gat.pte_offset].execute=x;
		if(!ignore_mt)pte_p->virt[gat.pte_offset].memory_type=memory_type;
		pte_p->virt[gat.pte_offset].page_offset=page_4kb_count(hpa);
	}
	return pte_p;
}

#if !defined(_hv_type1)
bool nvc_ept_insert_pte(noir_ept_manager_p eptm,noir_hook_page_p nhp)
{
	// Traverse all hook pages and set hooks.
	for(u32 i=0;i<noir_hook_pages_count;i++)
	{
		noir_ept_pte_descriptor_p pte_p=nvc_ept_update_pte(eptm,nhp[i].hook.phys,nhp[i].orig.phys,false,false,true,true,0,true);
		ia32_addr_translator addr;
		if(!pte_p)return false;
		addr.value=nhp[i].orig.phys;
		nhp[i].pte_descriptor=(void*)&pte_p->virt[addr.pte_offset];
	}
	return true;
}
#endif

/*
	According to the rule of variable MTRR precedence:

	1. If one of the overlapped region is UC, then the memory type will be UC.
	2. If two or more overlapped region are WT and WB, then the memory type will be WT.
	3. If the overlapped regions do not match rule 1 and 2, then behavior of processor is undefined.

	Therefore, we may simply compare the value of memory type.
	The final value of the memory type will be the one that have smallest value.
*/

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
			u32 exponential;
			u64 len=0,increment=0,base=page_mult(phys_base.phys_base);
			noir_bsf64(&exponential,page_mult(phys_mask.phys_mask));
			len=1ui64<<exponential;
			nv_dprintf("Variable MTRR (0x%X) Base: 0x%016llX, Mask: 0x%016llX, Length: 0x%016llX, Type: %u\n",mtrr_msr_index,phys_base.value,phys_mask.value,len,phys_base.type);
			for(u64 addr=base;addr<base+len;addr+=increment)
			{
				// The increment depends on address alignment and remainder length.
				const u64 remainder=base+len-addr;
				if(page_1gb_offset(addr)==0)
				{
					if(remainder>=page_1gb_size)
						increment=page_1gb_size;
					else if(remainder>=page_2mb_size)
						increment=page_2mb_size;
					else
						increment=page_4kb_size;
				}
				else if(page_2mb_offset(addr)==0)
				{
					if(remainder>=page_2mb_size)
						increment=page_2mb_size;
					else
						increment=page_4kb_size;
				}
				else
					increment=page_4kb_size;
				// Update the paging structure.
				switch(increment)
				{
					case page_1gb_size:
					{
						nvc_ept_update_pdpte_memory_type(eptm,addr,(u8)phys_base.type,false);
						break;
					}
					case page_2mb_size:
					{
						nvc_ept_update_pde_memory_type(eptm,addr,(u8)phys_base.type,false);
						break;
					}
					case page_4kb_size:
					{
						nvc_ept_update_pte_memory_type(eptm,addr,(u8)phys_base.type,false);
						break;
					}
					default:
					{
						nv_dprintf("[Variable MTRR] Unknown increment: 0x%llX!\n",increment);
						break;
					}
				}
			}
		}
	}
}

void nvc_ept_update_by_mtrr(noir_ept_manager_p eptm)
{
	if(eptm->def_type.enabled)
	{
		ia32_mtrr_cap_msr mtrr_cap;
		// Read MTRR capabilities.
		mtrr_cap.value=noir_rdmsr(ia32_mtrr_cap);
		// Traverse variable-range MTRRs.
		for(u32 i=0;i<mtrr_cap.variable_count;i++)
			nvc_ept_update_per_var_mtrr(eptm,ia32_mtrr_phys_base0+(i<<1));
		// By the way, set the SMRR.
		if(mtrr_cap.support_smrr)
			nvc_ept_update_per_var_mtrr(eptm,ia32_smrr_phys_base);
	}
	// Traverse fixed-range MTRRs.
	if(eptm->def_type.enabled && eptm->def_type.fix_enabled)
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
		// Locate the PTE descriptor for first 2MiB.
		noir_ept_pte_descriptor_p pte_p=nvc_ept_split_pde(eptm,0,true,true);
		// MTRR Fixed64K 00000
		// This register specifies eight 64KiB ranges, 512KiB in total.
		type=(u8*)&fix64k_00000;
		for(u32 i=0;i<8;i++)
			for(u32 j=0;j<16;j++)	// 64KiB is actually 16 pages.
				pte_p->virt[(i<<4)+j].memory_type=type[i];
		// MTRR Fixed16K_80000
		// This register specifies eight 16KiB ranges, 128KiB in total.
		type=(u8*)&fix16k_80000;
		for(u32 i=0;i<8;i++)
			for(u32 j=0;j<4;j++)	// 16KiB is actually 4 pages.
				pte_p->virt[0x80+(i<<2)+j].memory_type=type[i];
		// MTRR Fixed16K_a0000
		// This register specifies eight 16KiB ranges, 128KiB in total.
		type=(u8*)&fix16k_a0000;
		for(u32 i=0;i<8;i++)
			for(u32 j=0;j<4;j++)	// 16KiB is actually 4 pages.
				pte_p->virt[0xa0+(i<<2)+j].memory_type=type[i];
		// MTRR Fixed4K_c0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_c0000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xc0+i].memory_type=type[i];
		// MTRR Fixed4K_c8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_c8000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xc8+i].memory_type=type[i];
		// MTRR Fixed4K_d0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_d0000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xd0+i].memory_type=type[i];
		// MTRR Fixed4K_d8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_d8000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xd8+i].memory_type=type[i];
		// MTRR Fixed4K_e0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_e0000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xe0+i].memory_type=type[i];
		// MTRR Fixed4K_e8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_e8000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xe8+i].memory_type=type[i];
		// MTRR Fixed4K_f0000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_f0000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xf0+i].memory_type=type[i];
		// MTRR Fixed4K_f8000
		// This register specifies eight 4KiB ranges, 32KiB in total
		type=(u8*)&fix4k_f8000;
		for(u32 i=0;i<8;i++)
			pte_p->virt[0xf8+i].memory_type=type[i];
	}
	// Clear all MTRR-related bits.
	for(u32 i=0;i<512*512;i++)
		eptm->pdpt.virt[i].var_mtrr_covered=false;
	for(noir_ept_pde_descriptor_p pde_p=eptm->pde.head;pde_p;pde_p=pde_p->next)
		for(u32 i=0;i<512;i++)
			pde_p->large[i].var_mtrr_covered=false;
	for(noir_ept_pte_descriptor_p pte_p=eptm->pte.head;pte_p;pte_p=pte_p->next)
		for(u32 i=0;i<512;i++)
			pte_p->virt[i].var_mtrr_covered=false;
}

bool nvc_ept_install_mmio_hook(noir_ept_manager_p eptm,noir_io_avl_node_p node)
{
	bool r=true;
	for(u64 i=0;i<node->mmio.size;i+=page_size)
	{
		const u64 p=node->mmio.phys+i;
		bool r=(nvc_ept_update_pte(eptm,p,p,false,false,false,true,0,true)!=null);
		if(!r)return r;
	}
	if(node->avl.left)
	{
		r=nvc_ept_install_mmio_hook(eptm,(noir_io_avl_node_p)node->avl.left);
		if(!r)return r;
	}
	if(node->avl.right)
	{
		r=nvc_ept_install_mmio_hook(eptm,(noir_io_avl_node_p)node->avl.right);
		if(!r)return r;
	}
	return r;
}

bool nvc_ept_setup_mmio_hooks(noir_ept_manager_p eptm)
{
	if(hvm_p->mmio_hooks.root)
		return nvc_ept_install_mmio_hook(eptm,hvm_p->mmio_hooks.root);
	return true;
}

bool nvc_ept_initialize_ci(noir_ept_manager_p eptm)
{
	bool r=true;
	for(u32 i=0;i<noir_ci->pages;i++)
	{
		u64 phys=noir_ci->page_ci[i].phys;
		r&=(nvc_ept_update_pte(eptm,phys,phys,true,false,true,true,0,true)!=null);
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
		result&=(nvc_ept_update_pte(eptm,hvm->relative_hvm->msr_bitmap.phys,eptm->blank_page.phys,true,true,true,true,0,true)!=null);
		// nvc_ept_update_pte(eptm,hvm->relative_hvm->io_bitmap_a.phys,eptm->blank_page.phys,true,true,true);
		// nvc_ept_update_pte(eptm,hvm->relative_hvm->io_bitmap_b.phys,eptm->blank_page.phys,true,true,true);
		for(u32 i=0;i<hvm->cpu_count;i++)
		{
			noir_vt_vcpu_p vcpu=&hvm->virtual_cpu[i];
			noir_ept_manager_p eptmt=(noir_ept_manager_p)vcpu->ept_manager;
			noir_ept_pte_descriptor_p cur_t=null;
			noir_ept_pde_descriptor_p cur_d=null;
			// Protect VMXON region and VMCS.
			result&=(nvc_ept_update_pte(eptm,vcpu->vmxon.phys,eptm->blank_page.phys,true,true,true,true,0,true)!=null);
			result&=(nvc_ept_update_pte(eptm,vcpu->vmcs.phys,eptm->blank_page.phys,true,true,true,true,0,true)!=null);
			// Protect MSR Auto List.
			result&=(nvc_ept_update_pte(eptm,vcpu->msr_auto.phys,eptm->blank_page.phys,true,true,true,true,0,true)!=null);
			// Protect EPT Paging Structure.
			result&=(nvc_ept_update_pte(eptm,eptmt->eptp.phys.value,eptm->blank_page.phys,true,true,true,true,0,true)!=null);
			// Allow Guest read the original PDPTE so that EPT-violation VM-Exits can be reduced.
			result&=nvc_ept_update_pde(eptm,eptmt->pdpt.phys,eptmt->pdpt.phys,true,false,false,true,true,0,true);
			// Update PDEs of paging structure.
			for(cur_d=eptm->pde.head;cur_d;cur_d=cur_d->next)
				result&=(nvc_ept_update_pte(eptm,cur_d->phys,eptm->blank_page.phys,true,true,true,true,0,true)!=null);
			// Update PTEs of paging structure.
			for(cur_t=eptm->pte.head;cur_t;cur_t=cur_t->next)
				result&=(nvc_ept_update_pte(eptm,cur_t->phys,eptm->blank_page.phys,true,true,true,true,0,true)!=null);
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

  NoirVisor's design supports 256TB physical memory in total.

  Memory Consumption in Paging of each vCPU:
  4KB for 1 PML4E page - all 512 entries is used for mapping 512*512GB=256TB physical memory.
  2MB for 512 PDPTE pages - all 262144 entries are used for mapping 512*512*1GB=256TB physical memory.
  2MB for 512 PDE pages - all 262144 entries are used for mapping lower 512*512*2MB=512GB physical memory.
  Note that we expect only lower 512GB are used as physical RAM. Higher physical addresses are intended for MMIO.
  We will allocate the PDPTEs and PDEs on two single 2MB-aligned pages.
*/
noir_ept_manager_p nvc_ept_build_identity_map()
{
	bool alloc_success=false;
	// Allocate structures for EPT Manager.
#if defined(_hv_type1)
	noir_ept_manager_p eptm=noir_alloc_nonpg_memory(sizeof(noir_ept_manager));
#else
	noir_ept_manager_p eptm=noir_alloc_nonpg_memory(sizeof(noir_ept_manager)+sizeof(noir_hook_page)*noir_hook_pages_count);
#endif
	if(eptm)
	{
		eptm->eptp.virt=noir_alloc_contd_memory(page_size);
		if(eptm->eptp.virt)
		{
			eptm->pdpt.virt=noir_alloc_2mb_page();
			if(eptm->pdpt.virt)
			{
				eptm->pdpt.phys=noir_get_physical_address(eptm->pdpt.virt);
				alloc_success=true;
			}
		}
	}
	if(alloc_success)
	{
		u64 def_type=ia32_uncacheable;
		u32 a;
		noir_cpuid(ia32_cpuid_ext_pcap_prm_eid,0,&a,null,null,null);
		eptm->phys_addr_size=a&0xff;
		eptm->virt_addr_size=(a>>8)&0xff;
		// Get the default memory type.
		eptm->def_type.value=noir_rdmsr(ia32_mtrr_def_type);
		// Although unlikely to happen, MTRRs can be disabled.
		// If disabled, default memory type is uncacheable.
		if(eptm->def_type.enabled)def_type=eptm->def_type.type;
		for(u32 i=0;i<512;i++)
		{
			for(u32 j=0;j<512;j++)
			{
				const u32 k=(i<<9)+j;
				// Build Page-Directory-Pointer-Table Entries (PDPTEs)
				eptm->pdpt.virt[k].value=0;
				eptm->pdpt.virt[k].page_offset=k;
				eptm->pdpt.virt[k].read=1;
				eptm->pdpt.virt[k].write=1;
				eptm->pdpt.virt[k].execute=1;
				eptm->pdpt.virt[k].huge_pdpte=1;
				eptm->pdpt.virt[k].memory_type=def_type;
			}
			// Build Page-Map-Level-4 Entries (PML4Es)
			eptm->eptp.virt[i].value=0;
			eptm->eptp.virt[i].pdpte_offset=page_count(eptm->pdpt.phys)+i;
			eptm->eptp.virt[i].read=1;
			eptm->eptp.virt[i].write=1;
			eptm->eptp.virt[i].execute=1;
		}
		// Update MTRR.
		nvc_ept_update_by_mtrr(eptm);
#if !defined(_hv_type1)
		// Make Hooked Pages.
		noir_copy_memory(eptm->hook_pages,noir_hook_pages,sizeof(noir_hook_page)*noir_hook_pages_count);
		if(hvm_p->options.stealth_inline_hook)
			if(nvc_ept_insert_pte(eptm,eptm->hook_pages)==false)
				goto alloc_failure;
#endif
		// Initialize CI.
		if(nvc_ept_initialize_ci(eptm)==false)
			goto alloc_failure;
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