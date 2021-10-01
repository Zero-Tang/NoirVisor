/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the customizable VM engine for AMD-V

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_custom.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
#include "svm_def.h"
#include "svm_npt.h"

void nvc_svm_switch_to_host_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_initial_stack_p loader_stack=(noir_svm_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_svm_initial_stack));
	noir_svm_custom_vcpu_p cvcpu=loader_stack->custom_vcpu;
	// Step 1: Save State of the Customizable VM.
	// Save General-Purpose Registers...
	gpr_state->rax=noir_svm_vmread(cvcpu->vmcb.virt,guest_rax);
	gpr_state->rsp=noir_svm_vmread(cvcpu->vmcb.virt,guest_rsp);
	noir_movsp(&cvcpu->header.gpr,gpr_state,sizeof(void*)*2);
	cvcpu->header.rip=noir_svm_vmread64(cvcpu->vmcb.virt,guest_rip);
	cvcpu->header.rflags=noir_svm_vmread64(cvcpu->vmcb.virt,guest_rflags);
	// Save x87 FPU and SSE/AVX State...
	noir_xsave(cvcpu->header.xsave_area);
	// Save Extended Control Registers...
	cvcpu->header.xcrs.xcr0=noir_xgetbv(0);
	// Save Debug Registers...
	cvcpu->header.drs.dr0=noir_readdr0();
	cvcpu->header.drs.dr1=noir_readdr1();
	cvcpu->header.drs.dr2=noir_readdr2();
	cvcpu->header.drs.dr3=noir_readdr3();
	// The rest of processor states are already saved in VMCB.
	// Step 2: Load Host State.
	// Load General-Purpose Registers...
	noir_movsp(gpr_state,&vcpu->cvm_state.gpr,sizeof(void*)*2);
	// Load x87 FPU and SSE/AVX State...
	noir_xrestore(vcpu->cvm_state.xsave_area);
	// Load Extended Control Registers...
	noir_xsetbv(0,vcpu->cvm_state.xcrs.xcr0);
	// Load Debug Registers...
	noir_writedr0(vcpu->cvm_state.drs.dr0);
	noir_writedr1(vcpu->cvm_state.drs.dr1);
	noir_writedr2(vcpu->cvm_state.drs.dr2);
	noir_writedr3(vcpu->cvm_state.drs.dr3);
	// Step 3: Switch vCPU to Host.
	loader_stack->custom_vcpu=&nvc_svm_idle_cvcpu;		// Indicate that CVM is not running.
	loader_stack->guest_vmcb_pa=vcpu->vmcb.phys;
}

void nvc_svm_switch_to_guest_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	noir_svm_initial_stack_p loader_stack=(noir_svm_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_svm_initial_stack));
	// IMPORTANT: If vCPU is scheduled to a different processor, resetting the VMCB cache state is required.
	if(cvcpu->proc_id!=loader_stack->proc_id)noir_svm_vmwrite32(cvcpu->vmcb.virt,vmcb_clean_bits,0);
	// (FIXME) Step 0: Check Guest State Consistency not checked by vmrun instruction.
	// Step 1: Save State of the Subverted Host.
	// Please note that it is unnecessary to save states which are already saved in VMCB.
	// Save General-Purpose Registers...
	noir_movsp(&vcpu->cvm_state.gpr,gpr_state,sizeof(void*)*2);
	// Save Extended Control Registers...
	vcpu->cvm_state.xcrs.xcr0=noir_xgetbv(0);
	// Save x87 FPU and SSE State...
	noir_xsave(vcpu->cvm_state.xsave_area);
	// Save Debug Registers...
	vcpu->cvm_state.drs.dr0=noir_readdr0();
	vcpu->cvm_state.drs.dr1=noir_readdr1();
	vcpu->cvm_state.drs.dr2=noir_readdr2();
	vcpu->cvm_state.drs.dr3=noir_readdr3();
	// Step 2: Load Guest State.
	// Load General-Purpose Registers...
	noir_movsp(gpr_state,&cvcpu->header.gpr,sizeof(void*)*2);
	if(!cvcpu->header.state_cache.gprvalid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_rax,gpr_state->rax);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_rsp,gpr_state->rsp);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_rip,cvcpu->header.rip);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_rflags,cvcpu->header.rflags);
	}
	// Load Extended Control Registers...
	noir_xsetbv(0,cvcpu->header.xcrs.xcr0);
	// Load x87 FPU and SSE State...
	noir_xrestore(cvcpu->header.xsave_area);
	// Load Debug Registers...
	noir_writedr0(cvcpu->header.drs.dr0);
	noir_writedr1(cvcpu->header.drs.dr1);
	noir_writedr2(cvcpu->header.drs.dr2);
	noir_writedr3(cvcpu->header.drs.dr3);
	if(!cvcpu->header.state_cache.dr_valid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_dr6,cvcpu->header.drs.dr6);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_dr7,cvcpu->header.drs.dr7);
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_debug_reg);
		cvcpu->header.state_cache.dr_valid=true;
	}
	// Load Control Registers...
	if(!cvcpu->header.state_cache.cr_valid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_cr0,cvcpu->header.crs.cr0);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_cr3,cvcpu->header.crs.cr3);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_cr4,cvcpu->header.crs.cr4);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_efer,cvcpu->header.msrs.efer|amd64_efer_svme_bit);
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_control_reg);
		cvcpu->header.state_cache.cr_valid=true;
	}
	if(!cvcpu->header.state_cache.cr2valid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_cr2,cvcpu->header.crs.cr2);
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_cr2);
		cvcpu->header.state_cache.cr2valid=true;
	}
	if(!cvcpu->header.state_cache.tp_valid)
	{
		noir_svm_vmwrite8(cvcpu->vmcb.virt,avic_control,(u8)cvcpu->header.crs.cr8&0xf);
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_tpr);
		cvcpu->header.state_cache.tp_valid=true;
	}
	// Load Segment Registers...
	if(!cvcpu->header.state_cache.sr_valid)
	{
		// Load segment selectors.
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_cs_selector,cvcpu->header.seg.cs.selector);
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_ds_selector,cvcpu->header.seg.ds.selector);
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_es_selector,cvcpu->header.seg.es.selector);
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_ss_selector,cvcpu->header.seg.ss.selector);
		// Load segment attributes.
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_cs_attrib,svm_attrib(cvcpu->header.seg.cs.attrib));
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_ds_attrib,svm_attrib(cvcpu->header.seg.ds.attrib));
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_es_attrib,svm_attrib(cvcpu->header.seg.es.attrib));
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_ss_attrib,svm_attrib(cvcpu->header.seg.ss.attrib));
		// Load segment limits.
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_cs_limit,cvcpu->header.seg.cs.limit);
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_ds_limit,cvcpu->header.seg.ds.limit);
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_es_limit,cvcpu->header.seg.es.limit);
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_ss_limit,cvcpu->header.seg.ss.limit);
		// Load segment bases.
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_cs_base,cvcpu->header.seg.cs.base);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_ds_base,cvcpu->header.seg.ds.base);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_es_base,cvcpu->header.seg.es.base);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_ss_base,cvcpu->header.seg.ss.base);
		// Mark the VMCB cache invalid.
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_segment_reg);
		cvcpu->header.state_cache.sr_valid=true;	// Cache is refreshed. Mark it valid.
	}
	if(!cvcpu->header.state_cache.fg_valid)
	{
		// Load segment selectors.
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_fs_selector,cvcpu->header.seg.fs.selector);
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_gs_selector,cvcpu->header.seg.gs.selector);
		// Load segment attributes.
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_fs_attrib,svm_attrib(cvcpu->header.seg.fs.attrib));
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_gs_attrib,svm_attrib(cvcpu->header.seg.gs.attrib));
		// Load segment limits.
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_fs_limit,cvcpu->header.seg.fs.limit);
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_gs_limit,cvcpu->header.seg.gs.limit);
		// Load segment bases.
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_fs_base,cvcpu->header.seg.fs.base);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_gs_base,cvcpu->header.seg.gs.base);
		// No need to invalidate VMCB. The vmload instruction will load them.
		cvcpu->header.state_cache.fg_valid=true;
	}
	// Load Descriptor Tables...
	if(!cvcpu->header.state_cache.lt_valid)
	{
		// Load segment selectors.
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_tr_selector,cvcpu->header.seg.tr.selector);
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_ldtr_selector,cvcpu->header.seg.ldtr.selector);
		// Load segment attributes.
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_tr_attrib,svm_attrib(cvcpu->header.seg.tr.attrib));
		noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_ldtr_attrib,svm_attrib(cvcpu->header.seg.ldtr.attrib));
		// Load segment limits.
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_tr_limit,cvcpu->header.seg.tr.limit);
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_ldtr_limit,cvcpu->header.seg.ldtr.limit);
		// Load segment bases.
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_tr_base,cvcpu->header.seg.tr.base);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_ldtr_base,cvcpu->header.seg.ldtr.base);
		// No need to invalidate VMCB. The vmload instruction will load them.
		cvcpu->header.state_cache.lt_valid=true;
	}
	if(!cvcpu->header.state_cache.dt_valid)
	{
		// Load descriptor table limits.
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_gdtr_limit,cvcpu->header.seg.gdtr.limit);
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_idtr_limit,cvcpu->header.seg.idtr.limit);
		// Load descriptor table bases.
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_gdtr_base,cvcpu->header.seg.gdtr.base);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_idtr_base,cvcpu->header.seg.idtr.base);
		// Mark the VMCB cache invalid.
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_idt_gdt);
		cvcpu->header.state_cache.dt_valid=true;
	}
	// Step 3. Switch vCPU to Guest.
	loader_stack->custom_vcpu=cvcpu;
	loader_stack->guest_vmcb_pa=cvcpu->vmcb.phys;
}

void nvc_svm_initialize_cvm_vmcb(noir_svm_custom_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// Initialize the VMCB for vCPU.
	nvc_svm_instruction_intercept1 vector1;
	nvc_svm_instruction_intercept2 vector2;
	nvc_svm_npt_control npt_ctrl;
	// Initialize Interceptions
	// INIT Signal must be intercepted.
	noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_security_exception);
	vector1.value=0;
	// All external interrupts must be intercepted for scheduler's sake.
	vector1.intercept_intr=1;
	vector1.intercept_nmi=1;
	vector1.intercept_smi=1;
	// We need to hook the cpuid instruction.
	vector1.intercept_cpuid=1;
	// The hlt instruction is intended for scheduler.
	vector1.intercept_hlt=1;
	// The invlpga is an SVM instruction, which must be intercepted.
	vector1.intercept_invlpga=1;
	// I/O operations must be intercepted.
	vector1.intercept_io=1;
	// MSR accesses must be intercepted.
	vector1.intercept_msr=1;
	// The shutdown conditions must be intercepted.
	vector1.intercept_shutdown=1;
	noir_svm_vmwrite32(vmcb,intercept_instruction1,vector1.value);
	vector2.value=0;
	// All SVM-Related instructions must be intercepted.
	vector2.intercept_vmrun=1;
	vector2.intercept_vmmcall=1;
	vector2.intercept_vmload=1;
	vector2.intercept_vmsave=1;
	vector2.intercept_stgi=1;
	vector2.intercept_clgi=1;
	vector2.intercept_skinit=1;
	// The xsetbv instruction should be intercepted for SIMD Processor States.
	vector2.intercept_xsetbv=1;
	noir_svm_vmwrite16(vmcb,intercept_instruction2,vector2.value);
	// Initialize TLB: Flushing TLB & Setting ASID.
	// Flush all TLBs associated with this ASID before running it.
	noir_svm_vmwrite32(vmcb,guest_asid,vcpu->vm->asid);
	noir_svm_vmwrite8(vmcb,tlb_control,nvc_svm_tlb_control_flush_guest);
	// Initialize Nested Paging.
	npt_ctrl.value=0;
	npt_ctrl.enable_npt=1;
	noir_svm_vmwrite64(vmcb,npt_control,npt_ctrl.value);
	noir_svm_vmwrite64(vmcb,npt_cr3,vcpu->vm->nptm.ncr3.phys);
	// Initialize IOPM/MSRPM
	noir_svm_vmwrite64(vmcb,iopm_physical_address,vcpu->vm->iopm.phys);
	noir_svm_vmwrite64(vmcb,msrpm_physical_address,vcpu->vm->msrpm.phys);
}

#if !defined(_hv_type1)
u32 nvc_svmc_get_vm_asid(noir_svm_custom_vm_p vm)
{
	return vm->asid;
}

noir_svm_custom_vcpu_p nvc_svmc_reference_vcpu(noir_svm_custom_vm_p vm,u32 vcpu_id)
{
	return vm->vcpu[vcpu_id];
}

void nvc_svmc_release_vcpu(noir_svm_custom_vcpu_p vcpu)
{
	if(vcpu)
	{
		if(vcpu->vmcb.virt)noir_free_contd_memory(vcpu->vmcb.virt);
		if(vcpu->header.xsave_area)noir_free_contd_memory(vcpu->header.xsave_area);
		if(vcpu->vm)vcpu->vm->vcpu[vcpu->proc_id]=null;
		noir_free_nonpg_memory(vcpu);
	}
}

noir_status nvc_svmc_create_vcpu(noir_svm_custom_vcpu_p* virtual_cpu,noir_svm_custom_vm_p virtual_machine,u32 vcpu_id)
{
	if(virtual_machine->vcpu[vcpu_id]==null)
	{
		noir_svm_custom_vcpu_p vcpu=noir_alloc_nonpg_memory(sizeof(noir_svm_custom_vcpu));
		if(vcpu)
		{
			// Allocate VMCB
			vcpu->vmcb.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->vmcb.virt)
				vcpu->vmcb.phys=noir_get_physical_address(vcpu->vmcb.virt);
			else
				goto alloc_failure;
			// Allocate XSAVE State Area
			vcpu->header.xsave_area=noir_alloc_contd_memory(hvm_p->xfeat.supported_size_max);
			// Insert the vCPU into the VM.
			virtual_machine->vcpu[vcpu_id]=vcpu;
			// Mark the owner VM of vCPU.
			vcpu->vm=virtual_machine;
			vcpu->proc_id=vcpu_id;
			// Initialize the VMCB via hypercall. It is supposed that only hypervisor can operate VMCB.
			noir_svm_vmmcall(noir_svm_init_custom_vmcb,(ulong_ptr)vcpu);
		}
		*virtual_cpu=vcpu;
		return noir_success;
alloc_failure:
		return noir_insufficient_resources;
	}
	return noir_vcpu_already_created;
}

void static nvc_svmc_set_pte_entry(amd64_npt_pte_p entry,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	u64 pat_index=(u64)nvc_npt_get_host_pat_index(map_attrib.caching);
	entry->value=0;
	// Protection attributes...
	entry->present=map_attrib.present;
	entry->write=map_attrib.write;
	entry->user=map_attrib.user;
	entry->no_execute=!map_attrib.execute;
	// Caching attributes...
	entry->pwt=noir_bt(&pat_index,0);
	entry->pcd=noir_bt(&pat_index,1);
	entry->pat=noir_bt(&pat_index,2);
	// Address translation...
	entry->page_base=page_4kb_count(hpa);
}

void static nvc_svmc_set_pde_entry(amd64_npt_pde_p entry,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	entry->value=0;
	if(map_attrib.psize!=1)
	{
		entry->present=entry->write=entry->user=1;
		entry->pte_base=page_4kb_count(hpa);
	}
	else
	{
		u64 pat_index=(u64)nvc_npt_get_host_pat_index(map_attrib.caching);
		amd64_npt_large_pde_p large_pde=(amd64_npt_large_pde_p)entry;
		// Protection attributes...
		large_pde->present=map_attrib.present;
		large_pde->write=map_attrib.write;
		large_pde->user=map_attrib.user;
		large_pde->no_execute=!map_attrib.execute;
		// Caching attributes...
		large_pde->pwt=noir_bt(&pat_index,0);
		large_pde->pcd=noir_bt(&pat_index,1);
		large_pde->pat=noir_bt(&pat_index,2);
		// Address translation...
		large_pde->page_base=page_2mb_count(hpa);
		large_pde->large_pde=1;
	}
}

void static nvc_svmc_set_pdpte_entry(amd64_npt_pdpte_p entry,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	entry->value=0;
	if(map_attrib.psize!=2)
	{
		entry->present=entry->write=entry->user=1;
		entry->pde_base=page_4kb_count(hpa);
	}
	else
	{
		u64 pat_index=(u64)nvc_npt_get_host_pat_index(map_attrib.caching);
		amd64_npt_huge_pdpte_p huge_pdpte=(amd64_npt_huge_pdpte_p)entry;
		// Protection attributes...
		huge_pdpte->present=map_attrib.present;
		huge_pdpte->write=map_attrib.write;
		huge_pdpte->user=map_attrib.user;
		huge_pdpte->no_execute=!map_attrib.execute;
		// Caching attributes...
		huge_pdpte->pwt=noir_bt(&pat_index,0);
		huge_pdpte->pcd=noir_bt(&pat_index,1);
		huge_pdpte->pat=noir_bt(&pat_index,2);
		// Address translation...
		huge_pdpte->page_base=page_1gb_count(hpa);
		huge_pdpte->huge_pdpte=1;
	}
}

void static nvc_svmc_set_pml4e_entry(amd64_npt_pml4e_p entry,u64 hpa)
{
	entry->value=0;
	entry->present=entry->write=entry->user=1;
	entry->pdpte_base=page_4kb_count(hpa);
}

noir_status static nvc_svmc_create_1gb_page_map(noir_svm_custom_npt_manager_p npt_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_insufficient_resources;
	noir_npt_pdpte_descriptor_p pdpte_p=noir_alloc_nonpg_memory(sizeof(noir_npt_pdpte_descriptor));
	if(pdpte_p)
	{
		pdpte_p->virt=noir_alloc_contd_memory(page_size);
		if(pdpte_p->virt==null)
			noir_free_nonpg_memory(pdpte_p);
		else
		{
			amd64_addr_translator gpa_t;
			gpa_t.value=gpa;
			// Setup PDPTE descriptor.
			pdpte_p->phys=noir_get_physical_address(pdpte_p->virt);
			pdpte_p->gpa_start=page_512gb_base(gpa);
			// Do mapping - this level.
			nvc_svmc_set_pdpte_entry(&pdpte_p->virt[gpa_t.pdpte_offset],hpa,map_attrib);
			// Do mapping - prior level.
			// Note that PML4E is already described.
			nvc_svmc_set_pml4e_entry(&npt_manager->ncr3.virt[gpa_t.pml4e_offset],pdpte_p->phys);
			// Add to the linked list.
			if(npt_manager->pdpte.head)
				npt_manager->pdpte.tail->next=pdpte_p;
			else
				npt_manager->pdpte.head=pdpte_p;
			npt_manager->pdpte.tail=pdpte_p;
			st=noir_success;
		}
	}
	return st;
}

noir_status static nvc_svmc_create_2mb_page_map(noir_svm_custom_npt_manager_p npt_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_insufficient_resources;
	noir_npt_pde_descriptor_p pde_p=noir_alloc_nonpg_memory(sizeof(noir_npt_pde_descriptor));
	if(pde_p)
	{
		pde_p->virt=noir_alloc_contd_memory(page_size);
		if(pde_p->virt==null)
			noir_free_nonpg_memory(pde_p);
		else
		{
			noir_npt_pdpte_descriptor_p cur=npt_manager->pdpte.head;
			amd64_addr_translator gpa_t;
			gpa_t.value=gpa;
			// Setup PDE descriptor
			pde_p->phys=noir_get_physical_address(pde_p->virt);
			pde_p->gpa_start=page_1gb_base(gpa);
			// Do mapping
			nvc_svmc_set_pde_entry(&pde_p->virt[gpa_t.pte_offset],hpa,map_attrib);
			// Add to the linked list.
			if(npt_manager->pde.head)
				npt_manager->pde.tail->next=pde_p;
			else
				npt_manager->pde.head=pde_p;
			npt_manager->pde.tail=pde_p;
			// Search for existing PDPTEs and set up upper-level mapping.
			while(cur)
			{
				if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_512gb_size)break;
				cur=cur->next;
			}
			if(!cur)
			{
				noir_cvm_mapping_attributes null_map={0};
				// This 512GiB page is not yet described yet.
				st=nvc_svmc_create_1gb_page_map(npt_manager,gpa,0,null_map);
				if(st==noir_success)cur=npt_manager->pdpte.tail;
			}
			if(cur)
			{
				nvc_svmc_set_pdpte_entry(&cur->virt[gpa_t.pde_offset],pde_p->phys,map_attrib);
				st=noir_success;
			}
		}
	}
	return st;
}

noir_status static nvc_svmc_create_4kb_page_map(noir_svm_custom_npt_manager_p npt_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_insufficient_resources;
	noir_npt_pte_descriptor_p pte_p=noir_alloc_nonpg_memory(sizeof(noir_npt_pte_descriptor));
	if(pte_p)
	{
		pte_p->virt=noir_alloc_contd_memory(page_size);
		if(pte_p->virt==null)
			noir_free_nonpg_memory(pte_p);
		else
		{
			noir_npt_pde_descriptor_p cur=npt_manager->pde.head;
			amd64_addr_translator gpa_t;
			gpa_t.value=gpa;
			// Setup PTE descriptor
			pte_p->phys=noir_get_physical_address(pte_p->virt);
			pte_p->gpa_start=page_2mb_base(gpa);
			// Do mapping
			nvc_svmc_set_pte_entry(&pte_p->virt[gpa_t.pte_offset],0,map_attrib);
			// Add to the linked list
			if(npt_manager->pte.head)
				npt_manager->pte.tail->next=pte_p;
			else
				npt_manager->pte.head=pte_p;
			npt_manager->pte.tail=pte_p;
			// Search for existing PDEs and set up upper-level mapping.
			while(cur)
			{
				if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_1gb_size)break;
				cur=cur->next;
			}
			if(!cur)
			{
				noir_cvm_mapping_attributes null_map={0};
				// This 1GiB page is not yet described yet.
				st=nvc_svmc_create_2mb_page_map(npt_manager,gpa,0,null_map);
				if(st==noir_success)cur=npt_manager->pde.tail;
			}
			if(cur)
			{
				nvc_svmc_set_pde_entry(&cur->virt[gpa_t.pde_offset],pte_p->phys,map_attrib);
				st=noir_success;
			}
		}
	}
	return st;
}

noir_status static nvc_svmc_set_page_map(noir_svm_custom_npt_manager_p npt_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_unsuccessful;
	amd64_addr_translator gpa_t;
	gpa_t.value=gpa;
	switch(map_attrib.psize)
	{
		case 0:
		{
			// Search for existing PTEs.
			noir_npt_pte_descriptor_p cur=npt_manager->pte.head;
			while(cur)
			{
				if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_2mb_size)break;
				cur=cur->next;
			}
			if(!cur)
			{
				noir_cvm_mapping_attributes null_map={0};
				// This 2MiB page is not described yet.
				st=nvc_svmc_create_4kb_page_map(npt_manager,gpa,0,null_map);
				if(st==noir_success)cur=npt_manager->pte.tail;
			}
			if(cur)
			{
				nvc_svmc_set_pte_entry(&cur->virt[gpa_t.pte_offset],hpa,map_attrib);
				st=noir_success;
			}
			break;
		}
		default:
		{
			st=noir_not_implemented;
			break;
		}
	}
	return st;
}

noir_status nvc_svmc_set_mapping(noir_svm_custom_vm_p virtual_machine,noir_cvm_address_mapping_p mapping_info)
{
	noir_status st=noir_unsuccessful;
	amd64_addr_translator gpa;
	u32 increment[4]={page_4kb_shift,page_2mb_shift,page_1gb_shift,page_512gb_shift};
	gpa.value=mapping_info->gpa;
	for(u32 i=0;i<mapping_info->pages;i++)
	{
		u64 gpa=mapping_info->gpa+(i<<increment[mapping_info->attributes.psize]);
		u64 hva=mapping_info->hva+(i<<increment[mapping_info->attributes.psize]);
		u64 hpa=noir_get_user_physical_address((void*)hva);
		st=nvc_svmc_set_page_map(&virtual_machine->nptm,gpa,hpa,mapping_info->attributes);
		if(st!=noir_success)break;
	}
	return st;
}

u32 nvc_svmc_alloc_asid()
{
	u32 asid;
	noir_acquire_reslock_exclusive(hvm_p->tlb_tagging.asid_pool_lock);
	asid=noir_find_clear_bit(hvm_p->tlb_tagging.asid_pool,hvm_p->tlb_tagging.limit);
	if(asid!=0xffffffff)
	{
		noir_set_bitmap(hvm_p->tlb_tagging.asid_pool,asid);
		asid+=hvm_p->tlb_tagging.limit;
	}
	noir_release_reslock(hvm_p->tlb_tagging.asid_pool_lock);
	return asid;
}

void nvc_svmc_release_vm(noir_svm_custom_vm_p vm)
{
	if(vm)
	{
		// Release vCPUs and list. Make sure no vCPU is running as we release the structure.
		noir_acquire_reslock_exclusive(vm->header.vcpu_list_lock);
		// In that scheduling a vCPU requires acquiring the vCPU list lock with shared access,
		// acquiring the lock with exclusive access will rule all vCPUs out of any scheduling.
		if(vm->vcpu)
		{
			for(u32 i=0;i<255;i++)
				if(vm->vcpu[i])
					nvc_svmc_release_vcpu(vm->vcpu[i]);
			noir_free_nonpg_memory(vm->vcpu);
			vm->vcpu=null;
		}
		noir_release_reslock(vm->header.vcpu_list_lock);
		// Release NPT Paging Structure.
		if(vm->nptm.ncr3.virt)
			noir_free_contd_memory(vm->nptm.ncr3.virt);
		// Release PDPTE descriptors and paging structures...
		if(vm->nptm.pdpte.head)
		{
			noir_npt_pdpte_descriptor_p cur=vm->nptm.pdpte.head;
			while(cur)
			{
				noir_npt_pdpte_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		// Release PDE descriptors and paging structures...
		if(vm->nptm.pde.head)
		{
			noir_npt_pde_descriptor_p cur=vm->nptm.pde.head;
			while(cur)
			{
				noir_npt_pde_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		// Release PTE descriptors and paging structures...
		if(vm->nptm.pte.head)
		{
			noir_npt_pte_descriptor_p cur=vm->nptm.pte.head;
			while(cur)
			{
				noir_npt_pte_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		// Release MSRPM & IOPM
		if(vm->msrpm.virt)noir_free_contd_memory(vm->msrpm.virt);
		if(vm->iopm.virt)noir_free_contd_memory(vm->iopm.virt);
		// Release ASID.
		noir_acquire_reslock_exclusive(hvm_p->tlb_tagging.asid_pool_lock);
		noir_reset_bitmap(hvm_p->tlb_tagging.asid_pool_lock,vm->asid-hvm_p->tlb_tagging.limit);
		noir_release_reslock(hvm_p->tlb_tagging.asid_pool_lock);
		// Release VM Structure.
		noir_free_nonpg_memory(vm);
	}
}

// Creating a CVM does not create corresponding vCPUs and lower paging structures!
noir_status nvc_svmc_create_vm(noir_svm_custom_vm_p* virtual_machine)
{
	noir_status st=noir_invalid_parameter;
	if(virtual_machine)
	{
		noir_svm_custom_vm_p vm=noir_alloc_nonpg_memory(sizeof(noir_svm_custom_vm));
		st=noir_insufficient_resources;
		*virtual_machine=vm;
		if(vm)
		{
			// Allocate ASID for CVM.
			vm->asid=nvc_svmc_alloc_asid();
			if(vm->asid==0xffffffff)
				goto alloc_failure;
			// Create a generic Page Map Level 4 (PML4) Table.
			vm->nptm.ncr3.virt=noir_alloc_contd_memory(page_size);
			if(vm->nptm.ncr3.virt)
				vm->nptm.ncr3.phys=noir_get_physical_address(vm->nptm.ncr3.virt);
			else
				goto alloc_failure;
			// Allocate IOPM.
			vm->iopm.virt=noir_alloc_contd_memory(page_size*3);
			if(vm->iopm.virt)
				vm->iopm.phys=noir_get_physical_address(vm->iopm.virt);
			else
				goto alloc_failure;
			// Allocate MSRPM.
			vm->msrpm.virt=noir_alloc_contd_memory(page_size*2);
			if(vm->msrpm.virt)
				vm->msrpm.phys=noir_get_physical_address(vm->msrpm.virt);
			else
				goto alloc_failure;
			// Allocate vCPU pointer list.
			// According to AVIC, 255 physical cores are permitted.
			vm->vcpu=noir_alloc_nonpg_memory(sizeof(void*)*256);
			if(vm->vcpu==null)goto alloc_failure;
			// Setup MSR & I/O Interceptions.
			// We want unconditional exits.
			noir_stosb(vm->msrpm.virt,0xff,noir_svm_msrpm_size);
			noir_stosb(vm->iopm.virt,0xff,noir_svm_iopm_size);
			st=noir_success;
		}
	}
	return st;
alloc_failure:
	nvc_svmc_release_vm(*virtual_machine);
	*virtual_machine=null;
	return noir_insufficient_resources;
}

void nvc_svmc_finalize_cvm_module()
{
	if(noir_vm_list_lock)
		noir_finalize_reslock(noir_vm_list_lock);
}

noir_status nvc_svmc_initialize_cvm_module()
{
	// Initialization Phase I: Initialize the Resource Lock of the VM List Lock.
	noir_status st=noir_insufficient_resources;
	noir_vm_list_lock=noir_initialize_reslock();
	if(noir_vm_list_lock)
	{
		// Initialization Phase II: Ready the Idle VM.
		st=noir_success;
		hvm_p->idle_vm=&noir_idle_vm;
		noir_initialize_list_entry(&noir_idle_vm.active_vm_list);
	}
	return st;
}
#endif