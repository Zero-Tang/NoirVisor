/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

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
			// Find if the desired page has already been described.
			if(nhp->orig.phys>=cur_d->gpa_start && nhp->orig.phys<cur_d->gpa_start+page_2mb_size)
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
		cur_h->pte_descriptor=(void*)&cur_d->virt[addr.pte_offset];
		// Reset the page translation.
		cur_d->virt[addr.pte_offset].page_offset=nhp->hook.phys>>12;
		// Clear R/W but Reserve X.
		cur_d->virt[addr.pte_offset].read=0;
		cur_d->virt[addr.pte_offset].write=0;
		// Go to Setup the Next Hook.
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
		if(eptm->pde.virt)
			noir_free_2mb_page(eptm->pde.virt);
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

bool nvc_ept_update_pte(noir_ept_manager_p eptm,u64 hpa,u64 gpa,bool r,bool w,bool x)
{
	ia32_addr_translator hat,gat;
	noir_ept_pte_descriptor_p pte_p;
	hat.value=hpa;
	gat.value=gpa;
	// We cannot accept address higher than 512GB here.
	if(hat.pml4e_offset || gat.pml4e_offset)return false;
	for(pte_p=eptm->pte.head;pte_p->next;pte_p=pte_p->next)
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
	}
	// The 2MB page was not described as PTE yet.
	pte_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
	if(pte_p)
	{
		pte_p->virt=noir_alloc_contd_memory(page_size);
		if(pte_p->virt)
		{
			u32 i=0;
			u64 index=(hat.pdpte_offset<<9)+hat.pde_offset;
			ia32_ept_pde_p pde_p=(ia32_ept_pde_p)&eptm->pde.virt[index];
			// PTE Descriptor
			pte_p->phys=noir_get_physical_address(pte_p->virt);
			pte_p->gpa_start=index<<9;
			for(;i<512;i++)
			{
				pte_p->virt[i].read=1;
				pte_p->virt[i].write=1;
				pte_p->virt[i].execute=1;
				pte_p->virt[i].memory_type=ia32_write_back;
				pte_p->virt[i].page_offset=pte_p->gpa_start+i;
			}
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
			eptm->pte.tail->next=pte_p;
			eptm->pte.tail=pte_p;
			return true;
		}
		noir_free_nonpg_memory(pte_p);
	}
	return false;
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
		u32 i=0;
		eptm->blank_page.phys=noir_get_physical_address(eptm->blank_page.virt);
		// Protect MSR, I/O bitmap, and MSR Auto List.
		result&=nvc_ept_update_pte(eptm,hvm->relative_hvm->msr_bitmap.phys,eptm->blank_page.phys,true,true,true);
		// nvc_ept_update_pte(eptm,hvm->relative_hvm->io_bitmap_a.phys,eptm->blank_page.phys,true,true,true);
		// nvc_ept_update_pte(eptm,hvm->relative_hvm->io_bitmap_b.phys,eptm->blank_page.phys,true,true,true);
		result&=nvc_ept_update_pte(eptm,hvm->relative_hvm->msr_auto_list.phys,eptm->blank_page.phys,true,true,true);
		for(;i<hvm->cpu_count;i++)
		{
			noir_vt_vcpu_p vcpu=&hvm->virtual_cpu[i];
			noir_ept_manager_p eptmt=(noir_ept_manager_p)vcpu->ept_manager;
			// Protect VMXON region and VMCS.
			result&=nvc_ept_update_pte(eptm,vcpu->vmxon.phys,eptm->blank_page.phys,true,true,true);
			result&=nvc_ept_update_pte(eptm,vcpu->vmcs.phys,eptm->blank_page.phys,true,true,true);
			// Protect EPT Paging Structure.
			result&=nvc_ept_update_pte(eptm,eptmt->eptp.phys.value,eptm->blank_page.phys,true,true,true);
			result&=nvc_ept_update_pte(eptm,eptmt->pdpt.phys,eptm->blank_page.phys,true,true,true);
			// Allow Guest read the original PDE so that EPT-violation VM-Exits can be reduced.
			result&=nvc_ept_update_pde(eptm,eptmt->pde.phys,true,false,false);
			// It is unnecessary to protect the PTEs here.
			// Even if the malware changes the PTE, it wouldn't work
			// as long as we don't invalidate the relevant EPT TLB.
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
					alloc_success=true;
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
				const u32 k=(i<<9)+j;
				// Build Page-Directory Entries (PDEs)
				eptm->pde.virt[k].value=0;
				eptm->pde.virt[k].page_offset=k;
				eptm->pde.virt[k].read=1;
				eptm->pde.virt[k].write=1;
				eptm->pde.virt[k].execute=1;
				eptm->pde.virt[k].memory_type=ia32_write_back;
				eptm->pde.virt[k].large_pde=1;
			}
			// Build Page-Directory-Pointer-Table Entries (PDPTEs)
			eptm->pdpt.virt[i].value=0;
			eptm->pdpt.virt[i].pde_offset=(eptm->pde.phys>>12)+i;
			eptm->pdpt.virt[i].read=1;
			eptm->pdpt.virt[i].write=1;
			eptm->pdpt.virt[i].execute=1;
		}
		if(nvc_ept_insert_pte(eptm,noir_hook_pages)==false)
			goto alloc_failure;
		// Build Page Map Level-4 Entry (PML4E)
		eptm->pdpt.phys=noir_get_physical_address(eptm->pdpt.virt);
		eptm->eptp.virt->value=0;
		eptm->eptp.virt->pdpte_offset=eptm->pdpt.phys>>12;
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