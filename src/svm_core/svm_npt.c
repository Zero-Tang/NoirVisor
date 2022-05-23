/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the basic driver of AMD-V Nested Paging.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_npt.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include <ci.h>
#include "svm_vmcb.h"
#include "svm_def.h"
#include "svm_npt.h"

u64 nvc_gva_to_gpa(noir_npt_manager_p nptm,void* gva)
{
	return 0;
}

void nvc_npt_cleanup(noir_npt_manager_p nptm)
{
	if(nptm)
	{
		if(nptm->ncr3.virt)
			noir_free_contd_memory(nptm->ncr3.virt,page_size);
		if(nptm->pdpt.virt)
			noir_free_contd_memory(nptm->pdpt.virt,page_size);
		if(nptm->pde.virt)
			noir_free_2mb_page(nptm->pde.virt);
		if(nptm->pte.head)
		{
			noir_npt_pte_descriptor_p cur=nptm->pte.head;
			while(cur)
			{
				noir_npt_pte_descriptor_p next=cur->next;
				noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		noir_free_nonpg_memory(nptm);
	}
}

u8 nvc_npt_get_host_pat_index(u8 type)
{
	for(u8 i=0;i<8;i++)
		if(hvm_p->host_pat.list[i]==type)
			return i;
	return 0xff;
}

u8 nvc_npt_convert_pat_type(u64 pat,u8 type)
{
	u8* pat_array=(u8*)&pat;
	u8 final_type=0xff;
	// Search if such type is defined in PAT
	for(u8 i=0;i<8;i++)
		if((pat_array[i]&0x7)==type)
			final_type=i;
	return final_type;
}

bool nvc_npt_update_pde(noir_npt_manager_p nptm,u64 hpa,bool r,bool w,bool x)
{
	amd64_addr_translator trans;
	trans.value=hpa;
	if(trans.pml4e_offset==0)
	{
		u32 index=(u32)((trans.pdpte_offset<<9)+trans.pde_offset);
		nptm->pde.virt[index].present=r;
		nptm->pde.virt[index].write=w;
		nptm->pde.virt[index].no_execute=!x;
		return true;
	}
	return false;
}

bool nvc_npt_update_pte(noir_npt_manager_p nptm,u64 hpa,u64 gpa,bool r,bool w,bool x)
{
	amd64_addr_translator hat,gat;
	noir_npt_pte_descriptor_p pte_p=nptm->pte.head;
	hat.value=hpa;
	gat.value=gpa;
	// Address above 512GB cannot be operated.
	if(hat.pml4e_offset || gat.pml4e_offset)return false;
	while(pte_p)
	{
		if(hpa>=pte_p->gpa_start && hpa<pte_p->gpa_start+page_2mb_size)
		{
			// The 2MB page has already been described.
			pte_p->virt[gat.pte_offset].present=r;
			pte_p->virt[gat.pte_offset].write=w;
			pte_p->virt[gat.pte_offset].no_execute=!x;
			pte_p->virt[gat.pte_offset].page_base=hpa>>12;
			return true;
		}
		pte_p=pte_p->next;
	}
	// The 2MB page has not been described yet.
	pte_p=noir_alloc_nonpg_memory(sizeof(noir_npt_pte_descriptor));
	if(pte_p)
	{
		pte_p->virt=noir_alloc_contd_memory(page_size);
		if(pte_p->virt)
		{
			u64 index=(gat.pdpte_offset<<9)+gat.pde_offset;
			amd64_npt_pde_p pde_p=(amd64_npt_pde_p)&nptm->pde.virt[index];
			// PTE Descriptor
			pte_p->phys=noir_get_physical_address(pte_p->virt);
			pte_p->gpa_start=index<<9;
			for(u32 i=0;i<512;i++)
			{
				pte_p->virt[i].present=true;
				pte_p->virt[i].write=true;
				pte_p->virt[i].user=true;
				pte_p->virt[i].no_execute=false;
				pte_p->virt[i].page_base=pte_p->gpa_start+i;
			}
			pte_p->gpa_start<<=12;
			// Page Attributes
			pte_p->virt[gat.pte_offset].present=r;
			pte_p->virt[gat.pte_offset].write=w;
			pte_p->virt[gat.pte_offset].no_execute=!x;
			pte_p->virt[gat.pte_offset].page_base=hpa>>12;
			// Update PDE
			pde_p->reserved1=0;
			pde_p->pte_base=pte_p->phys>>12;
			// Update Linked-List
			if(nptm->pte.head && nptm->pte.tail)
			{
				nptm->pte.tail->next=pte_p;
				nptm->pte.tail=pte_p;
			}
			else
			{
				nptm->pte.head=pte_p;
				nptm->pte.tail=pte_p;
			}
			return true;
		}
		noir_free_nonpg_memory(pte_p);
	}
	return false;
}


/*
  It is important that the hypervisor essentials should be protected.
  The malware in guest may tamper the VMCB through any read or write
  instruction. Since SVM VMCB layout is publicized, if malware knows
  the address of VMCB, then things will be problematic.

  By Now, any read/write to the protected area would be redirected a
  page that is purposefully and deliberately left blank.
  This design could reduce #NPF.
*/
bool nvc_npt_protect_critical_hypervisor(noir_hypervisor_p hvm)
{
	hvm->relative_hvm->blank_page.virt=noir_alloc_contd_memory(page_size);
	if(hvm->relative_hvm->blank_page.virt)
	{
		bool result=true;
		hvm->relative_hvm->blank_page.phys=noir_get_physical_address(hvm->relative_hvm->blank_page.virt);
		// Protect HSAVE and VMCB
		for(u32 i=0;i<hvm->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm->virtual_cpu[i];
			noir_npt_manager_p pri_nptm=(noir_npt_manager_p)vcpu->primary_nptm;
			// Protect MSRPM and IOPM
			result&=nvc_npt_update_pte(pri_nptm,hvm->relative_hvm->msrpm.phys,hvm->relative_hvm->blank_page.phys,true,true,true);
			result&=nvc_npt_update_pte(pri_nptm,vcpu->hsave.phys,hvm->relative_hvm->blank_page.phys,true,true,true);
			result&=nvc_npt_update_pte(pri_nptm,vcpu->vmcb.phys,hvm->relative_hvm->blank_page.phys,true,true,true);
			// Protect Nested Paging Structure
			result&=nvc_npt_update_pte(pri_nptm,pri_nptm->ncr3.phys,hvm->relative_hvm->blank_page.phys,true,true,true);
			result&=nvc_npt_update_pte(pri_nptm,pri_nptm->pdpt.phys,hvm->relative_hvm->blank_page.phys,true,true,true);
			// For PDE, memory usage would be too high to protect a 2MB page.
			// Disable the writing permission is enough, though.
			result&=nvc_npt_update_pde(pri_nptm,pri_nptm->pde.phys,true,false,true);
			// Update PTEs...
			for(noir_npt_pte_descriptor_p cur=pri_nptm->pte.head;cur;cur=cur->next)
				result&=nvc_npt_update_pte(pri_nptm,cur->phys,hvm->relative_hvm->blank_page.phys,true,true,true);
		}
		return result;
	}
	return false;
}

bool nvc_npt_initialize_ci(noir_npt_manager_p nptm)
{
	bool r=true;
	for(u32 i=0;i<noir_ci->pages;i++)
	{
		u64 phys=noir_ci->page_ci[i].phys;
		r&=nvc_npt_update_pte(nptm,phys,phys,true,false,true);
		if(!r)break;
	}
	return r;
}

#if !defined(_hv_type1)
void nvc_npt_build_hook_mapping(noir_svm_vcpu_p vcpu)
{
	u32 i=0;
	noir_npt_pte_descriptor_p pte_p=null;
	noir_hook_page_p nhp=null;
	noir_npt_manager_p pri_nptm=(noir_npt_manager_p)vcpu->primary_nptm;
	noir_npt_manager_p sec_nptm=(noir_npt_manager_p)vcpu->secondary_nptm;
	// In this function, we build mappings necessary to make stealth inline hook.
	for(nhp=noir_hook_pages;i<noir_hook_pages_count;nhp=&noir_hook_pages[++i])
		nvc_npt_update_pte(pri_nptm,nhp->orig.phys,nhp->orig.phys,true,true,false);
	for(nhp=noir_hook_pages,i=0;i<noir_hook_pages_count;nhp=&noir_hook_pages[++i])
		nvc_npt_update_pte(sec_nptm,nhp->hook.phys,nhp->orig.phys,true,true,true);
	// Set all necessary PDEs to NX.
	for(i=0;i<512;i++)
	{
		for(u32 j=0;j<512;j++)
		{
			u32 k=(i<<9)+j;
			if(sec_nptm->pde.virt[k].large_pde)
				sec_nptm->pde.virt[k].no_execute=true;
		}
	}
	// Set all PTEs to NX
	for(pte_p=sec_nptm->pte.head;pte_p;pte_p=pte_p->next)
		for(i=0;i<512;i++)
			pte_p->virt[i].no_execute=true;
	// Select PTEs that should be X.
	for(nhp=sec_nptm->hook_pages,i=0;i<noir_hook_pages_count;nhp=&sec_nptm->hook_pages[++i])
	{
		amd64_addr_translator trans;
		trans.value=nhp->orig.phys;
		// Select PTE
		for(pte_p=sec_nptm->pte.head;pte_p;pte_p=pte_p->next)
		{
			if(nhp->orig.phys>=pte_p->gpa_start && nhp->orig.phys<pte_p->gpa_start+page_2mb_size)
			{
				// To enable execution of a specific 4KiB page, upper PDE should be executable as well.
				nvc_npt_update_pde(sec_nptm,pte_p->gpa_start,true,true,true);
				pte_p->virt[trans.pte_offset].no_execute=false;
				nhp->pte_descriptor=(void*)&pte_p->virt[trans.pte_offset];
				break;
			}
		}
	}
}
#else
bool nvc_npt_build_apic_shadowing(noir_svm_vcpu_p vcpu)
{
	vcpu->primary_nptm->pte.apic.virt=noir_alloc_contd_memory(page_size);
	if(vcpu->primary_nptm->pte.apic.virt)
	{
		vcpu->primary_nptm->pte.apic.phys=noir_get_physical_address(vcpu->primary_nptm->pte.apic.virt);
		return true;
	}
	return false;
}

void nvc_npt_setup_apic_shadowing(noir_svm_vcpu_p vcpu)
{
	amd64_addr_translator trans;
	amd64_npt_pte_p pte_p=vcpu->primary_nptm->pte.apic.virt;
	amd64_npt_pde_p pde_base=null;
	vcpu->apic_base=trans.value=page_base(noir_rdmsr(amd64_apic_base));
	pde_base=(amd64_npt_pde_p)&vcpu->primary_nptm->pde.virt[trans.pde_offset];
	pde_base->reserved1=0;
	pde_base->pte_base=vcpu->primary_nptm->pte.apic.phys>>page_shift;
	for(u32 i=0;i<512;i++)
	{
		pte_p[i].value=0;
		pte_p[i].present=1;
		pte_p[i].write=1;
		pte_p[i].user=1;
		pte_p[i].page_base=(trans.pde_offset<<page_shift_diff)+i;
	}
	vcpu->apic_pte=&pte_p[trans.pte_offset];
	vcpu->apic_pte->write=0;
}
#endif

/*
  Introduction to Identity map:

  AMD-V Nested Paging introduces a built-in GPA to HPA technology.
  For NoirVisor, we translate GPA to HPA with GPA=HPA term.
  Essentials: {GPA=HPA} (Important).
  This is key purpose of identity map.
  We use NPT for advanced feature - access filtering.

  NoirVisor's designation supports 512GB physical memory in total.

  Memory Consumption in Paging of each vCPU:
  4KB for 1 PML4E page - only 1 entry is used for mapping 512GB physical memory.
  4KB for 1 PDPTE page - all 512 entries are used for mapping 512*1GB=512GB physical memory.
  2MB for 512 PDE pages - all 262144 entries are used for mapping 512*512*2MB=512GB physical memory.
  We will allocate the PDEs on a single 2MB-aligned page.
*/
noir_npt_manager_p nvc_npt_build_identity_map()
{
	bool alloc_success=false;
#if defined(_hv_type1)
	noir_npt_manager_p nptm=noir_alloc_nonpg_memory(sizeof(noir_npt_manager));
#else
	noir_npt_manager_p nptm=noir_alloc_nonpg_memory(sizeof(noir_npt_manager)+sizeof(noir_hook_page)*noir_hook_pages_count);
#endif
	if(nptm)
	{
		nptm->ncr3.virt=noir_alloc_contd_memory(page_size);
		if(nptm->ncr3.virt)
		{
			nptm->pdpt.virt=noir_alloc_contd_memory(page_size);
			if(nptm->pdpt.virt)
			{
				nptm->pde.virt=noir_alloc_2mb_page();
				if(nptm->pde.virt)
				{
					nptm->pde.phys=noir_get_physical_address(nptm->pde.virt);
					alloc_success=true;
				}
				nptm->pdpt.phys=noir_get_physical_address(nptm->pdpt.virt);
			}
			nptm->ncr3.phys=noir_get_physical_address(nptm->ncr3.virt);
		}
	}
	if(alloc_success)
	{
		for(u32 i=0;i<512;i++)
		{
			for(u32 j=0;j<512;j++)
			{
				const u32 k=(i<<9)+j;
				// Build Page-Directory Entries (PDE)
				nptm->pde.virt[k].value=0;
				nptm->pde.virt[k].present=1;
				nptm->pde.virt[k].write=1;
				nptm->pde.virt[k].user=1;
				nptm->pde.virt[k].large_pde=1;
				nptm->pde.virt[k].page_base=k;
			}
			// Build Page-Directory Pointer Table Entries (PDPTE)
			nptm->pdpt.virt[i].value=0;
			nptm->pdpt.virt[i].present=1;
			nptm->pdpt.virt[i].write=1;
			nptm->pdpt.virt[i].user=1;
			nptm->pdpt.virt[i].pde_base=(nptm->pde.phys>>12)+i;
		}
		// Build Page Map Level 4 Entries (PML4E)
		nptm->ncr3.virt->value=0;
		nptm->ncr3.virt->present=1;
		nptm->ncr3.virt->write=1;
		nptm->ncr3.virt->user=1;
		nptm->ncr3.virt->pdpte_base=nptm->pdpt.phys>>12;
#if !defined(_hv_type1)
		// Distribute the hook pages to each vCPU.
		noir_copy_memory(&nptm->hook_pages,noir_hook_pages,sizeof(noir_hook_page)*noir_hook_pages_count);
#endif
	}
	else
	{
		nvc_npt_cleanup(nptm);
		nv_dprintf("Allocation Failure! Failed to build NPT paging structure!\n");
		nptm=null;
	}
	return nptm;
}