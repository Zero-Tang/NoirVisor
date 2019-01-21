/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

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
#include <intrin.h>
#include <ia32.h>
#include "vt_vmcs.h"
#include "vt_ept.h"
#include "vt_def.h"

bool nvc_ept_insert_pte(noir_ept_manager_p eptm,noir_hook_page_p nhp)
{
	noir_hook_page_p cur_h=nhp;
	while(cur_h)
	{
		noir_ept_pte_descriptor_p cur_d=eptm->pte.head;
		ia32_addr_translator addr;
		addr.value=nhp->orig.phys;
		while(cur_d)
		{
			//Find if the desired page has already been described.
			if(nhp->orig.phys>=cur_d->gpa_start && nhp->orig.phys<cur_d->gpa_start+page_2mb_size)
				break;
			cur_d=cur_d->next;
		}
		if(cur_d==null)
		{
			//The desired page is not described.
			//Describe the page, then perform hooking.
			ia32_ept_pde_p pde_p=(ia32_ept_pde_p)&eptm->pde.virt[addr.pdpte_offset][addr.pde_offset];
			u32 i=0;
			cur_d=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
			if(cur_d==null)return false;
			cur_d->virt=noir_alloc_contd_memory(page_size);
			if(cur_d->virt==null)return false;
			cur_d->phys=noir_get_physical_address(cur_d->virt);
			cur_d->gpa_start=(addr.pdpte_offset<<18)+(addr.pde_offset<<9);
			//Setup identity map.
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
			//Insert to descriptor linked-list.
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
			//Reset the Page-Directory Entry
			pde_p->reserved0=0;
			pde_p->large_pde=0;
			pde_p->pte_offset=cur_d->phys>>12;
		}
		//At this moment, we assume the desired page is described.
		cur_h->pte_descriptor=(void*)&cur_d->virt[addr.pte_offset];
		//Reset the page translation.
		cur_d->virt[addr.pte_offset].page_offset=nhp->hook.phys>>12;
		//Clear R/W but Reserve X.
		cur_d->virt[addr.pte_offset].read=0;
		cur_d->virt[addr.pte_offset].write=0;
		//Go to Setup the Next Hook.
		cur_h=cur_h->next;
	}
	return true;
}

void nvc_ept_cleanup(noir_ept_manager_p eptm)
{
	if(eptm)
	{
		if(eptm->eptp.virt)
			noir_free_contd_memory(eptm->eptp.virt);
		if(eptm->pdpt.virt)
			noir_free_contd_memory(eptm->pdpt.virt);
		if(eptm->pde.phys)
			noir_free_nonpg_memory(eptm->pde.phys);
		if(eptm->pde.virt)
		{
			u32 i=0;
			for(;i<512;i++)
			{
				if(eptm->pde.virt[i])
					noir_free_contd_memory(eptm->pde.virt[i]);
				else
					break;
			}
			noir_free_nonpg_memory(eptm->pde.virt);
		}
		if(eptm->pte.head)
		{
			noir_ept_pte_descriptor_p cur=eptm->pte.head;
			while(cur)
			{
				noir_ept_pte_descriptor_p next=cur->next;
				noir_free_contd_memory(cur->virt);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
	}
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
  2MB for 512 PDE pages - all 262144 entries are used for mapping 512*512*2MB=512GB physical memory
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
				eptm->pde.virt=noir_alloc_nonpg_memory(sizeof(void*)*512);
				eptm->pde.phys=noir_alloc_nonpg_memory(page_size);
				if(eptm->pde.virt && eptm->pde.phys)
				{
					u32 i=0;
					alloc_success=true;
					for(;i<512;i++)
					{
						eptm->pde.virt[i]=noir_alloc_contd_memory(page_size);
						if(eptm->pde.virt[i]==null)
						{
							alloc_success=false;
							break;
						}
					}
				}
			}
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
				//Build Page-Directory Entries (PDEs)
				eptm->pde.virt[i][j].value=0;
				eptm->pde.virt[i][j].page_offset=(i<<9)+j;
				eptm->pde.virt[i][j].read=1;
				eptm->pde.virt[i][j].write=1;
				eptm->pde.virt[i][j].execute=1;
				eptm->pde.virt[i][j].memory_type=ia32_write_back;
				eptm->pde.virt[i][j].large_pde=1;
			}
			//Build Page-Directory-Pointer-Table Entries (PDPTEs)
			eptm->pde.phys[i]=noir_get_physical_address(eptm->pde.virt[i]);
			eptm->pdpt.virt[i].value=0;
			eptm->pdpt.virt[i].pde_offset=eptm->pde.phys[i]>>12;
			eptm->pdpt.virt[i].read=1;
			eptm->pdpt.virt[i].write=1;
			eptm->pdpt.virt[i].execute=1;
		}
		if(nvc_ept_insert_pte(eptm,noir_hook_pages)==false)
			goto alloc_failure;
		//Build Page Map Level-4 Entry (PML4E)
		eptm->pdpt.phys=noir_get_physical_address(eptm->pdpt.virt);
		eptm->eptp.virt->value=0;
		eptm->eptp.virt->pdpte_offset=eptm->pdpt.phys>>12;
		eptm->eptp.virt->read=1;
		eptm->eptp.virt->write=1;
		eptm->eptp.virt->execute=1;
		//Make EPT-Pointer (EPTP)
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