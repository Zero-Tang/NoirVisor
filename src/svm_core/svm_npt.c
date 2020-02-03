/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the basic driver of AMD-V Nested Paging.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_npt.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <nvstatus.h>
#include <svm_intrin.h>
#include <intrin.h>
#include <amd64.h>
#include <ci.h>
#include "svm_vmcb.h"
#include "svm_def.h"
#include "svm_npt.h"

void nvc_npt_cleanup(noir_npt_manager_p nptm)
{
	if(nptm)
	{
		if(nptm->ncr3.virt)
			noir_free_contd_memory(nptm->ncr3.virt);
		if(nptm->pdpt.virt)
			noir_free_contd_memory(nptm->pdpt.virt);
		if(nptm->pde.virt)
			noir_free_2mb_page(nptm->pde.virt);
		if(nptm->pte.head)
		{
			noir_npt_pte_descriptor_p cur=nptm->pte.head;
			while(cur)
			{
				noir_npt_pte_descriptor_p next=cur->next;
				noir_free_contd_memory(cur->virt);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		noir_free_nonpg_memory(nptm);
	}
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
			pte_p->virt[hat.pte_offset].present=r;
			pte_p->virt[hat.pte_offset].write=w;
			pte_p->virt[hat.pte_offset].no_execute=!x;
			pte_p->virt[hat.pte_offset].page_base=gpa>>12;
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
			u32 i=0;
			u64 index=(hat.pdpte_offset<<9)+hat.pde_offset;
			amd64_npt_pde_p pde_p=(amd64_npt_pde_p)&nptm->pde.virt[index];
			// PTE Descriptor
			pte_p->phys=noir_get_physical_address(pte_p->virt);
			pte_p->gpa_start=index<<9;
			for(;i<512;i++)
			{
				pte_p->virt[i].present=1;
				pte_p->virt[i].write=1;
				pte_p->virt[i].user=1;
				pte_p->virt[i].page_base=pte_p->gpa_start+i;
			}
			// Page Attributes
			pte_p->virt[hat.pte_offset].present=r;
			pte_p->virt[hat.pte_offset].write=w;
			pte_p->virt[hat.pte_offset].no_execute=!x;
			pte_p->virt[hat.pte_offset].page_base=gpa>>12;
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

bool nvc_npt_initialize_ci(noir_npt_manager_p nptm)
{
	bool r=true;
	u32 i=0;
	for(;i<noir_ci->pages;i++)
	{
		u64 phys=noir_ci->page_ci[i].phys;
		r&=nvc_npt_update_pte(nptm,phys,phys,true,false,true);
		if(!r)break;
	}
	return r;
}

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
	noir_npt_manager_p nptm=noir_alloc_nonpg_memory(sizeof(noir_npt_manager));
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
		u32 i=0;
		for(;i<512;i++)
		{
			u32 j=0;
			for(;j<512;j++)
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
		if(nvc_npt_initialize_ci(nptm)==false)
			goto alloc_failure;
		// Build Page Map Level 4 Entries (PML4E)
		nptm->ncr3.virt->value=0;
		nptm->ncr3.virt->present=1;
		nptm->ncr3.virt->write=1;
		nptm->ncr3.virt->user=1;
		nptm->ncr3.virt->pdpte_base=nptm->pdpt.phys>>12;
	}
	else
	{
alloc_failure:
		nvc_npt_cleanup(nptm);
		nv_dprintf("Allocation Failure! Failed to build NPT paging structure!\n");
		nptm=null;
	}
	return nptm;
}