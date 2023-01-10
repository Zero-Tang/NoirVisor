/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

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

// FIXME: Not Implemented.
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
			noir_free_2mb_page(nptm->pdpt.virt);
		if(nptm->pde.head)
		{
			noir_npt_pde_descriptor_p cur=nptm->pde.head;
			while(cur)
			{
				noir_npt_pde_descriptor_p next=cur->next;
				noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
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

// Splitted PDPTE must be described.
noir_npt_pde_descriptor_p nvc_npt_split_pdpte(noir_npt_manager_p nptm,u64 gpa,bool host,bool alloc)
{
	noir_npt_pde_descriptor_p pde_p=nptm->pde.head;
	while(pde_p)
	{
		if(gpa>=pde_p->gpa_start && gpa<pde_p->gpa_start+page_1gb_size)
			break;		// The 1GB page has already been described.
		pde_p=pde_p->next;
	}
	if(alloc==true && pde_p==null)
	{
		// The 1GB page has not been described yet.
		pde_p=noir_alloc_nonpg_memory(sizeof(noir_npt_pde_descriptor));
		if(pde_p)
		{
			pde_p->virt=noir_alloc_contd_memory(page_size);
			if(pde_p->virt)
			{
				const u64 index=page_1gb_count(gpa);
				amd64_npt_pdpte_p pdpte_p=(amd64_npt_pdpte_p)&nptm->pdpt.virt[index];
				// PDE Descriptor
				pde_p->phys=noir_get_physical_address(pde_p->virt);
				pde_p->gpa_start=index<<page_shift_diff64;
				for(u32 i=0;i<page_table_entries64;i++)
				{
					pde_p->large[i].present=true;
					pde_p->large[i].write=true;
					pde_p->large[i].user=true;
					pde_p->large[i].large_pde=true;
					pde_p->large[i].no_execute=false;
					pde_p->large[i].page_base=pde_p->gpa_start+i;
				}
				pde_p->gpa_start<<=page_2mb_shift;
				// Append to Linked-List
				if(nptm->pde.head && nptm->pde.tail)
				{
					nptm->pde.tail->next=pde_p;
					nptm->pde.tail=pde_p;
				}
				else
				{
					nptm->pde.head=pde_p;
					nptm->pde.tail=pde_p;
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
		amd64_npt_pdpte_p pdpte_p=(amd64_npt_pdpte_p)&nptm->pdpt.virt[index];
		pdpte_p->reserved1=0;
		pdpte_p->pde_base=page_4kb_count(pde_p->phys);
	}
	return pde_p;
}

// Splitted PDE must be described.
noir_npt_pte_descriptor_p nvc_npt_split_pde(noir_npt_manager_p nptm,u64 gpa,bool host,bool alloc)
{
	noir_npt_pte_descriptor_p pte_p=nptm->pte.head;
	while(pte_p)
	{
		if(gpa>=pte_p->gpa_start && gpa<pte_p->gpa_start+page_2mb_size)
			break;		// The 2MB page has been described.
		pte_p=pte_p->next;
	}
	if(alloc==true && pte_p==null)
	{
		// The 2MB page has not been described yet.
		pte_p=noir_alloc_nonpg_memory(sizeof(noir_npt_pte_descriptor));
		if(pte_p)
		{
			pte_p->virt=noir_alloc_contd_memory(page_size);
			if(pte_p->virt)
			{
				// Split the PDPTE first.
				noir_npt_pde_descriptor_p pde_p=nvc_npt_split_pdpte(nptm,gpa,host,true);
				if(pde_p)
				{
					const u64 pfn_index=page_2mb_count(gpa);
					const u64 pde_index=page_entry_index64(pfn_index);
					// PTE Descriptor
					pte_p->phys=noir_get_physical_address(pte_p->virt);
					pte_p->gpa_start=pfn_index<<page_shift_diff64;
					for(u32 i=0;i<page_table_entries64;i++)
					{
						pte_p->virt[i].present=true;
						pte_p->virt[i].write=true;
						pte_p->virt[i].user=true;
						pte_p->virt[i].no_execute=false;
						pte_p->virt[i].page_base=pte_p->gpa_start+i;
					}
					pte_p->gpa_start<<=page_4kb_shift;
					// Append to Linked-List
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
		noir_npt_pde_descriptor_p pde_p=nvc_npt_split_pdpte(nptm,gpa,host,alloc);
		if(pde_p)
		{
			const u64 pfn_index=page_2mb_count(gpa);
			const u64 pde_index=page_entry_index64(pfn_index);
			pde_p->virt[pde_index].reserved1=0;
			pde_p->virt[pde_index].pte_base=page_4kb_count(pte_p->phys);
		}
	}
	return pte_p;
}

bool nvc_npt_update_pdpte(noir_npt_manager_p nptm,u64 hpa,u64 gpa,bool r,bool w,bool x,bool h,bool alloc)
{
	const u64 index=page_1gb_count(gpa);
	amd64_npt_pdpte_p pdpte_p=(amd64_npt_pdpte_p)&nptm->pdpt.virt[index];
	if(h)
	{
		// Reset to huge PDPTE.
		nptm->pdpt.virt[index].huge_pdpte=true;
		nptm->pdpt.virt[index].page_base=page_1gb_count(hpa);
	}
	else
	{
		// Split the PDPTE.
		// If it is previously splitted, restore the mapping.
		noir_npt_pde_descriptor_p pde_p=nvc_npt_split_pdpte(nptm,gpa,true,alloc);
		if(!pde_p)return false;
		pdpte_p->reserved1=0;
		pdpte_p->pde_base=page_4kb_count(pde_p->phys);
	}
	pdpte_p->present=r;
	pdpte_p->write=w;
	pdpte_p->no_execute=!x;
	return true;
}

bool nvc_npt_update_pde(noir_npt_manager_p nptm,u64 hpa,u64 gpa,bool r,bool w,bool x,bool l,bool alloc)
{
	// Split the PDPTE.
	noir_npt_pde_descriptor_p pde_p=nvc_npt_split_pdpte(nptm,gpa,true,alloc);
	if(pde_p)
	{
		amd64_addr_translator gat;
		gat.value=gpa;
		if(l)
		{
			// Reset to large PDE.
			pde_p->large[gat.pde_offset].large_pde=true;
			pde_p->large[gat.pde_offset].page_base=page_2mb_count(hpa);
		}
		else
		{
			// Split the PDE.
			// If it is previously splitted, restore the mapping.
			noir_npt_pte_descriptor_p pte_p=nvc_npt_split_pde(nptm,gpa,true,alloc);
			if(!pte_p)return false;
			pde_p->virt[gat.pde_offset].reserved1=0;
			pde_p->virt[gat.pde_offset].pte_base=page_4kb_count(pte_p->phys);
		}
		pde_p->virt[gat.pde_offset].present=r;
		pde_p->virt[gat.pde_offset].write=w;
		pde_p->virt[gat.pde_offset].no_execute=!x;
		return true;
	}
	return false;
}

bool nvc_npt_update_pte(noir_npt_manager_p nptm,u64 hpa,u64 gpa,bool r,bool w,bool x,bool alloc)
{
	// Split the PTE
	noir_npt_pte_descriptor_p pte_p=nvc_npt_split_pde(nptm,gpa,true,alloc);
	if(pte_p)
	{
		amd64_addr_translator gat;
		gat.value=gpa;
		// Update the specific PTE.
		pte_p->virt[gat.pte_offset].present=r;
		pte_p->virt[gat.pte_offset].write=w;
		pte_p->virt[gat.pte_offset].no_execute=!x;
		pte_p->virt[gat.pte_offset].page_base=page_4kb_count(hpa);
		return true;
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
		noir_npt_manager_p pri_nptm=(noir_npt_manager_p)hvm->relative_hvm->primary_nptm;
		bool result=true;
		hvm->relative_hvm->blank_page.phys=noir_get_physical_address(hvm->relative_hvm->blank_page.virt);
		// Protect HSAVE and VMCB
		for(u32 i=0;i<hvm->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm->virtual_cpu[i];
			// Protect MSRPM and IOPM
			result&=nvc_npt_update_pte(pri_nptm,hvm->relative_hvm->msrpm.phys,hvm->relative_hvm->blank_page.phys,true,true,true,true);
			result&=nvc_npt_update_pte(pri_nptm,vcpu->hsave.phys,hvm->relative_hvm->blank_page.phys,true,true,true,true);
			result&=nvc_npt_update_pte(pri_nptm,vcpu->vmcb.phys,hvm->relative_hvm->blank_page.phys,true,true,true,true);
		}
		// Protect Nested Paging Structure
		result&=nvc_npt_update_pte(pri_nptm,pri_nptm->ncr3.phys,hvm->relative_hvm->blank_page.phys,true,true,true,true);
		// For PDPTE, memory usage would be too high to protect a 2MB page.
		// Disable the writing permission is enough, though.
		result&=nvc_npt_update_pde(pri_nptm,pri_nptm->pdpt.phys,pri_nptm->pdpt.phys,true,false,true,true,true);
		// Update PDEs...
		for(noir_npt_pde_descriptor_p cur=pri_nptm->pde.head;cur;cur=cur->next)
			result&=nvc_npt_update_pte(pri_nptm,cur->phys,hvm->relative_hvm->blank_page.phys,true,true,true,true);
		// Update PTEs...
		for(noir_npt_pte_descriptor_p cur=pri_nptm->pte.head;cur;cur=cur->next)
			result&=nvc_npt_update_pte(pri_nptm,cur->phys,hvm->relative_hvm->blank_page.phys,true,true,true,true);
		// Protect Reverse-Mapping Table...
		for(u64 phys=hvm->rmt.table.phys;phys<hvm->rmt.table.phys+hvm->rmt.size;phys+=page_size)
			result&=nvc_npt_update_pte(pri_nptm,phys,phys,true,false,true,true);
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
		r&=nvc_npt_update_pte(nptm,phys,phys,true,false,true,true);
		if(!r)break;
	}
	return r;
}

#if !defined(_hv_type1)
void nvc_npt_build_hook_mapping(noir_npt_manager_p pri_nptm,noir_npt_manager_p sec_nptm)
{
	u32 i=0;
	noir_npt_pde_descriptor_p pde_p=null;
	noir_npt_pte_descriptor_p pte_p=null;
	noir_hook_page_p nhp=null;
	// In this function, we build mappings necessary to make stealth inline hook.
	for(nhp=noir_hook_pages;i<noir_hook_pages_count;nhp=&noir_hook_pages[++i])
		nvc_npt_update_pte(pri_nptm,nhp->orig.phys,nhp->orig.phys,true,true,false,true);
	for(nhp=noir_hook_pages,i=0;i<noir_hook_pages_count;nhp=&noir_hook_pages[++i])
		nvc_npt_update_pte(sec_nptm,nhp->hook.phys,nhp->orig.phys,true,true,true,true);
	// Set all necessary PDPTEs to NX.
	for(i=0;i<512;i++)
	{
		for(u32 j=0;j<512;j++)
		{
			u32 k=(i<<9)+j;
			if(sec_nptm->pdpt.virt[k].huge_pdpte)sec_nptm->pdpt.virt[k].no_execute=true;
		}
	}
	// Set all PDEs to NX
	for(pde_p=sec_nptm->pde.head;pde_p;pde_p=pde_p->next)
		for(i=0;i<512;i++)
			pde_p->virt[i].no_execute=true;
	// Set all PTEs to NX
	for(pte_p=sec_nptm->pte.head;pte_p;pte_p=pte_p->next)
		for(i=0;i<512;i++)
			pte_p->virt[i].no_execute=true;
	// Select PTEs and PDEs that should be X.
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
				nvc_npt_update_pde(sec_nptm,0,pte_p->gpa_start,true,true,true,false,true);
				pte_p->virt[trans.pte_offset].no_execute=false;
				nhp->pte_descriptor=(void*)&pte_p->virt[trans.pte_offset];
				break;
			}
		}
		// Select PDE
		for(pde_p=sec_nptm->pde.head;pde_p;pde_p=pde_p->next)
		{
			if(nhp->orig.phys>=pde_p->gpa_start && nhp->orig.phys<pde_p->gpa_start+page_1gb_size)
			{
				// To enable execution of a specific 4KiB page, upper PDPTE should be executable as well.
				nvc_npt_update_pdpte(sec_nptm,0,pde_p->gpa_start,true,true,true,false,true);
				pde_p->virt[trans.pde_offset].no_execute=false;
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
	nv_dprintf("APIC Base=0x%llX\n",vcpu->apic_base);
	pde_base=(amd64_npt_pde_p)&vcpu->primary_nptm->pde.virt[(trans.pdpte_offset<<page_shift_diff)+trans.pde_offset];
	pde_base->reserved1=0;
	pde_base->pte_base=vcpu->primary_nptm->pte.apic.phys>>page_shift;
	for(u32 i=0;i<512;i++)
	{
		pte_p[i].value=0;
		pte_p[i].present=1;
		pte_p[i].write=1;
		pte_p[i].user=1;
		pte_p[i].page_base=(trans.pdpte_offset<<(page_shift_diff*2))+(trans.pde_offset<<page_shift_diff)+i;
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

  In NoirVisor's design, the lower 512GiB must be fully mapped. Higher memories can be optionally
  mapped as long as top-of-memory is there. This allows scalable systems.

  Memory Consumption in Paging of each vCPU:
  4KiB for 1 PML4E page - all 512 entries are used for to fully cover the address space.
  2MiB for 512 PDPTE page - the lowest entry uses 2MiB mapping and the higher 511 entries use 1GiB mapping.
  2MiB for 512 PDE pages - all 262144 entries are used for mapping 512*512*2MB=512GB physical memory.
  We will allocate the PDEs on a single 2MB-aligned page.
*/

noir_npt_manager_p nvc_npt_build_identity_map()
{
	u64 tom=noir_get_top_of_memory();
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
			nptm->pdpt.virt=noir_alloc_2mb_page();
			if(nptm->pdpt.virt)
			{
				alloc_success=true;
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
				// Build Page-Directory Pointer Table Entries (PDPTE)
				nptm->pdpt.virt[k].value=0;
				nptm->pdpt.virt[k].present=1;
				nptm->pdpt.virt[k].write=1;
				nptm->pdpt.virt[k].user=1;
				nptm->pdpt.virt[k].huge_pdpte=1;
				nptm->pdpt.virt[k].page_base=k;
			}
			// Build Page Map Level 4 Entries (PML4E)
			nptm->ncr3.virt[i].value=0;
			nptm->ncr3.virt[i].present=1;
			nptm->ncr3.virt[i].write=1;
			nptm->ncr3.virt[i].user=1;
			nptm->ncr3.virt[i].pdpte_base=(nptm->pdpt.phys>>12)+i;
		}
#if !defined(_hv_type1)
		// Copy the hook-page info to the NPT Manager.
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

// Reverse Map: Essentials of NoirVisor Secure Virtualization.
void nvc_npt_build_reverse_map()
{
	noir_npt_manager_p pri_nptm=hvm_p->relative_hvm->primary_nptm;
	noir_npt_manager_p sec_nptm=hvm_p->relative_hvm->secondary_nptm;
	// This is an initialization routine, call this at subversion only.
	for(u32 i=0;i<hvm_p->cpu_count;i++)
	{
		noir_svm_vcpu_p vcpu=&hvm_p->virtual_cpu[i];
		// Critical Hypervisor Pages...
		nvc_configure_reverse_mapping(vcpu->vmcb.phys,vcpu->vmcb.phys,0,true,noir_nsv_rmt_noirvisor);
		nvc_configure_reverse_mapping(vcpu->hvmcb.phys,vcpu->hvmcb.phys,0,true,noir_nsv_rmt_noirvisor);
		nvc_configure_reverse_mapping(vcpu->hsave.phys,vcpu->hsave.phys,0,true,noir_nsv_rmt_noirvisor);
	}
	// Nested Paging Structures...
	nvc_configure_reverse_mapping(pri_nptm->ncr3.phys,pri_nptm->ncr3.phys,0,true,noir_nsv_rmt_noirvisor);
	for(u32 j=0;j<512;j++)nvc_configure_reverse_mapping(pri_nptm->pdpt.phys+page_mult(j),pri_nptm->pdpt.phys+page_mult(j),0,true,noir_nsv_rmt_noirvisor);
	for(noir_npt_pde_descriptor_p cur=pri_nptm->pde.head;cur;cur=cur->next)nvc_configure_reverse_mapping(cur->phys,cur->phys,0,true,noir_nsv_rmt_noirvisor);
	for(noir_npt_pte_descriptor_p cur=pri_nptm->pte.head;cur;cur=cur->next)nvc_configure_reverse_mapping(cur->phys,cur->phys,0,true,noir_nsv_rmt_noirvisor);
	// MSRPM & IOPM
	nvc_configure_reverse_mapping(hvm_p->relative_hvm->msrpm.phys,hvm_p->relative_hvm->msrpm.phys,0,true,noir_nsv_rmt_noirvisor);
	nvc_configure_reverse_mapping(hvm_p->relative_hvm->msrpm.phys+page_size,hvm_p->relative_hvm->msrpm.phys+page_size,0,true,noir_nsv_rmt_noirvisor);
	nvc_configure_reverse_mapping(hvm_p->relative_hvm->iopm.phys,hvm_p->relative_hvm->iopm.phys,0,true,noir_nsv_rmt_noirvisor);
	nvc_configure_reverse_mapping(hvm_p->relative_hvm->iopm.phys+page_size,hvm_p->relative_hvm->iopm.phys+page_size,0,true,noir_nsv_rmt_noirvisor);
	nvc_configure_reverse_mapping(hvm_p->relative_hvm->iopm.phys+page_size*2,hvm_p->relative_hvm->iopm.phys+page_size*2,0,true,noir_nsv_rmt_noirvisor);
}

void nvc_npt_reassign_page_ownership_hvrt(noir_svm_vcpu_p vcpu,noir_rmt_remap_context_p context)
{
	noir_npt_manager_p nptm=hvm_p->relative_hvm->primary_nptm;
	noir_rmt_entry_p rmt=(noir_rmt_entry_p)hvm_p->rmt.table.virt;
	// Perform remapping.
	for(u32 i=0;i<context->pages;i++)
	{
		const u64 hpa=context->hpa_list[i];
		noir_npt_pde_descriptor_p pde_p=nvc_npt_split_pdpte(nptm,hpa,true,false);
		noir_npt_pte_descriptor_p pte_p=nvc_npt_split_pde(nptm,hpa,true,false);
		if(pde_p==null || pte_p==null)
		{
			context->status=noir_unsuccessful;
			break;
		}
		else
		{
			const u64 index=page_4kb_count(hpa);
			const u64 pte_i=page_entry_index64(index);
			if(rmt[index].low.ownership==noir_nsv_rmt_noirvisor)
			{
				// For debugging-purpose, pages assigned to NoirVisor can be readable.
				pte_p->virt[pte_i].present=true;
				pte_p->virt[pte_i].write=false;
				pte_p->virt[pte_i].user=true;
			}
			else if(rmt[index].low.ownership==noir_nsv_rmt_secure_guest)
			{
				// If the page is assigned to a secure guest, no accesses should be granted.
				pte_p->virt[pte_i].present=false;
				pte_p->virt[pte_i].write=false;
				pte_p->virt[pte_i].user=false;
			}
			else
			{
				// For subverted-host and insecure guest, pages should be fully accessible in host.
				pte_p->virt[pte_i].present=true;
				pte_p->virt[pte_i].write=true;
				pte_p->virt[pte_i].user=true;
			}
			pte_p->virt[pte_i].no_execute=false;
			context->status=noir_success;
		}
	}
	// This operation changes the mapping, so flush the TLB.
	noir_svm_vmwrite8(vcpu->vmcb.virt,tlb_control,nvc_svm_tlb_control_flush_guest);
}

void static nvc_npt_flush_tlb_generic_worker(void* context,u32 processor_id)
{
	// Flush TLBs
	noir_svm_vmmcall(noir_svm_call_flush_tlb,(ulong_ptr)context);
}

bool static nvc_npt_reassign_page_ownership_unsafe(u64p hpa,u64p gpa,u32 pages,u32 asid,bool shared,u8 ownership)
{
	noir_rmt_entry_p rmt=(noir_rmt_entry_p)hvm_p->rmt.table.virt;
	noir_rmt_remap_context remap;
	noir_rmt_reassignment_context reassignment;
	bool result=false;
	noir_npt_manager_p pri_nptm=hvm_p->relative_hvm->primary_nptm;
	// Stage I: Split the PDPTEs and PDEs, if haven't done already.
	for(u32 i=0;i<pages;i++)
	{
		// Get splitted PDPTE and PDE.
		noir_npt_pde_descriptor_p pde_p=nvc_npt_split_pdpte(pri_nptm,hpa[i],false,false);
		noir_npt_pte_descriptor_p pte_p=nvc_npt_split_pde(pri_nptm,hpa[i],false,false);
		if(pde_p==null)
		{
			// Retry: PDPTE was not splitted yet. Split it now.
			pde_p=nvc_npt_split_pdpte(pri_nptm,hpa[i],false,true);
			// PDEs should be assigned to NoirVisor.
			if(pde_p==null)goto alloc_failure;
			result=nvc_npt_reassign_page_ownership_unsafe(&pde_p->phys,&pde_p->phys,1,0,true,noir_nsv_rmt_noirvisor);
			if(result==false)goto alloc_failure;
		}
		if(pte_p==null)
		{
			// Retry: PDE was not splitted yet. Split it now.
			pte_p=nvc_npt_split_pde(pri_nptm,hpa[i],false,true);
			// PTEs should be assigned to NoirVisor.
			if(pte_p==null)goto alloc_failure;
			result=nvc_npt_reassign_page_ownership_unsafe(&pte_p->phys,&pte_p->phys,1,0,true,noir_nsv_rmt_noirvisor);
			if(result==false)goto alloc_failure;
		}
	}
	// Stage II: Modify Reverse-Mapping Table
	reassignment.hpa_list=hpa;
	reassignment.gpa_list=gpa;
	reassignment.pages=pages;
	reassignment.asid=asid;
	reassignment.shared=shared;
	reassignment.ownership=ownership;
	noir_svm_vmmcall(noir_svm_nsv_reassign_rmt,(ulong_ptr)&reassignment);
	result=reassignment.result;
	if(result)
	{
		// Stage III: Acquire exclusive executor.
		remap.hpa_list=hpa;
		remap.pages=pages;
		noir_svm_vmmcall(noir_svm_nsv_remap_by_rmt,(ulong_ptr)&remap);
		result=remap.status==noir_success;
		if(result)		// Flush TLBs on all processors.
			noir_generic_call(nvc_npt_flush_tlb_generic_worker,null);
		else
		{
			nv_dprintf("[NoirVisor RMT] Failed to remap! Status=0x%X\n",remap.status);
			noir_int3();
		}
	}
	else
	{
		nv_dprintf("[NoirVisor RMT] Failed to reassign RMT!\n");
		noir_int3();
		result=false;
	}
alloc_failure:
	if(!result)nv_dprintf("Failed to allocate memory for remapping!\n");
	return result;
}

// Warning: this procedure does not gain exclusion of the VM!
// Schedule out all vCPUs from execution before reassignment!
bool nvc_npt_reassign_page_ownership(u64p hpa,u64p gpa,u32 pages,u32 asid,bool shared,u8 ownership)
{
	bool result;
	// Lock everything we need here in order to circumvent race condition and reentrance of locks.
	noir_acquire_pushlock_exclusive(&hvm_p->relative_hvm->primary_nptm->nptm_lock);
	noir_acquire_pushlock_exclusive(&hvm_p->rmt.lock);
	// Now, perform reassignment.
	result=nvc_npt_reassign_page_ownership_unsafe(hpa,gpa,pages,asid,shared,ownership);
	// Unlock everything we locked.
	noir_release_pushlock_exclusive(&hvm_p->rmt.lock);
	noir_release_pushlock_exclusive(&hvm_p->relative_hvm->primary_nptm->nptm_lock);
	// Return
	return result;
}

// Warning: this procedure does not gain exclusion of the VM!
// Schedule out all vCPUs from execution before reassignment!
bool nvc_npt_reassign_cvm_all_pages_ownership(noir_svm_custom_vm_p vm,u32 asid,bool shared,u8 ownership)
{
	bool result=false;
	noir_rmt_entry_p rm_table=(noir_rmt_entry_p)hvm_p->rmt.table.virt;
	const u64 total_pages=hvm_p->rmt.size>>4;
	u32 pages=0;
	// Lock everything we need here in order to circumvent race condition and reentrance of locks.
	noir_acquire_pushlock_exclusive(&hvm_p->relative_hvm->primary_nptm->nptm_lock);
	noir_acquire_pushlock_exclusive(&hvm_p->rmt.lock);
	// Stage I: Scan the Reverse-Mapping Table.
	for(u64 i=0;i<total_pages;i++)
		if(rm_table[i].low.asid==vm->asid)
			pages++;
	// Stage II: Construct the list.
	if(pages)
	{
		u64p gpa_list=noir_alloc_nonpg_memory(pages<<3);
		u64p hpa_list=noir_alloc_nonpg_memory(pages<<3);
		u32 j=0;
		if(gpa_list && hpa_list)
		{
			for(u64 i=0;i<total_pages;i++)
			{
				if(rm_table[i].low.asid==vm->asid)
				{
					gpa_list[j]=page_4kb_mult(rm_table[i].high.guest_pfn);
					hpa_list[j]=page_4kb_mult(i);
					j++;
				}
			}
		}
		// Stage III: Perform reassignment.
		if(j==pages)result=nvc_npt_reassign_page_ownership_unsafe(hpa_list,gpa_list,pages,asid,shared,ownership);
		// Release list...
		if(gpa_list)noir_free_nonpg_memory(gpa_list);
		if(hpa_list)noir_free_nonpg_memory(hpa_list);
	}
	// Unlock everything we locked.
	noir_release_pushlock_exclusive(&hvm_p->rmt.lock);
	noir_release_pushlock_exclusive(&hvm_p->relative_hvm->primary_nptm->nptm_lock);
	// Return
	return result;
}