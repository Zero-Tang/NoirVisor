/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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
	// Save Extended Control Registers...
	cvcpu->header.xcrs.xcr0=noir_xgetbv(0);
	// Save Debug Registers...
	cvcpu->header.drs.dr0=noir_readdr0();
	cvcpu->header.drs.dr1=noir_readdr1();
	cvcpu->header.drs.dr2=noir_readdr2();
	cvcpu->header.drs.dr3=noir_readdr3();
	// Save the event injection field...
	cvcpu->header.injected_event.attributes.value=noir_svm_vmread32(cvcpu->vmcb.virt,event_injection);
	cvcpu->header.injected_event.error_code=noir_svm_vmread32(cvcpu->vmcb.virt,event_error_code);
	// If the injected event is not valid, check out the local APIC field in VMCB.
	if(!cvcpu->header.injected_event.attributes.valid)
	{
		nvc_svm_avic_control avic_ctrl;
		avic_ctrl.value=noir_svm_vmread64(cvcpu->vmcb.virt,avic_control);
		cvcpu->header.injected_event.attributes.value=0;
		cvcpu->header.injected_event.attributes.vector=(u32)avic_ctrl.virtual_interrupt_vector;
		cvcpu->header.injected_event.attributes.priority=(u32)avic_ctrl.virtual_interrupt_priority;
		cvcpu->header.injected_event.attributes.valid=(u32)avic_ctrl.virtual_irq;
		noir_svm_vmwrite64(cvcpu->vmcb.virt,avic_control,avic_ctrl.value);
	}
	// If AVIC is supported, set it to be not runnning so IPIs won't be delivered to a wrong processor.
	if(noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_avic))
	{
		nvc_svm_avic_physical_apic_id_entry_p apic_physical=(nvc_svm_avic_physical_apic_id_entry_p)cvcpu->vm->avic_physical.virt;
		apic_physical[cvcpu->proc_id].is_running=false;
	}
	// The rest of processor states are already saved in VMCB.
	// Step 2: Load Host State.
	// Load General-Purpose Registers...
	noir_movsp(gpr_state,&vcpu->cvm_state.gpr,sizeof(void*)*2);
	// Load Extended Control Registers...
	noir_xsetbv(0,vcpu->cvm_state.xcrs.xcr0);
	// Save x87 FPU and SSE/AVX State...
	noir_xsave(cvcpu->header.xsave_area,maxu64);
	// Load x87 FPU and SSE/AVX State...
	noir_xrestore(vcpu->cvm_state.xsave_area,maxu64);
	// Load Debug Registers...
	noir_writedr0(vcpu->cvm_state.drs.dr0);
	noir_writedr1(vcpu->cvm_state.drs.dr1);
	noir_writedr2(vcpu->cvm_state.drs.dr2);
	noir_writedr3(vcpu->cvm_state.drs.dr3);
	// Step 3: Switch vCPU to Host.
	loader_stack->custom_vcpu=&nvc_svm_idle_cvcpu;		// Indicate that CVM is not running.
	loader_stack->guest_vmcb_pa=vcpu->vmcb.phys;
	// The context will go to the host when vmrun is executed.
}

void nvc_svm_switch_to_guest_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	noir_svm_initial_stack_p loader_stack=(noir_svm_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_svm_initial_stack));
	bool asid_updated=false;
	// IMPORTANT: If vCPU is scheduled to a different processor, resetting the VMCB cache state is required.
	if(cvcpu->proc_id!=loader_stack->proc_id)
	{
		cvcpu->proc_id=loader_stack->proc_id;
		noir_svm_vmwrite32(cvcpu->vmcb.virt,vmcb_clean_bits,0);
	}
	// Step 1: Save State of the Subverted Host.
	// Please note that it is unnecessary to save states which are already saved in VMCB.
	// Save General-Purpose Registers...
	noir_movsp(&vcpu->cvm_state.gpr,gpr_state,sizeof(void*)*2);
	// Save Extended Control Registers...
	vcpu->cvm_state.xcrs.xcr0=noir_xgetbv(0);
	// Save x87 FPU and SSE State...
	noir_xsave(vcpu->cvm_state.xsave_area,maxu64);
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
		cvcpu->header.state_cache.gprvalid=true;
	}
	// Load x87 FPU and SSE State...
	noir_xrestore(cvcpu->header.xsave_area,maxu64);
	// Load Extended Control Registers...
	noir_xsetbv(0,cvcpu->header.xcrs.xcr0);
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
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_control_reg);
		cvcpu->header.state_cache.cr_valid=true;
		// Change to control registers can cause TLBs to be invalid.
		noir_svm_vmwrite8(cvcpu->vmcb.virt,tlb_control,nvc_svm_tlb_control_flush_guest);
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
		// Load Kernel-GS-Base MSR.
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_kernel_gs_base,cvcpu->header.msrs.gsswap);
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
	// Load EFER MSR
	if(!cvcpu->header.state_cache.ef_valid)
	{
		// SVME Shadowing
		cvcpu->shadowed_bits.svme=noir_bt((u32*)&cvcpu->header.msrs.efer,amd64_efer_svme);
		// Always enable EFER.SVME.
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_efer,cvcpu->header.msrs.efer|amd64_efer_svme_bit);
		cvcpu->header.state_cache.ef_valid=true;
	}
	// Load PAT MSR
	if(!cvcpu->header.state_cache.pa_valid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_pat,cvcpu->header.msrs.pat);
		// Mark the VMCB cache invalid.
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_npt);
		cvcpu->header.state_cache.pa_valid=true;
	}
	// Load MSRs for System Call (sysenter/sysexit)
	if(!cvcpu->header.state_cache.se_valid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_sysenter_cs,cvcpu->header.msrs.sysenter_cs);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_sysenter_esp,cvcpu->header.msrs.sysenter_esp);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_sysenter_eip,cvcpu->header.msrs.sysenter_eip);
		// No need to invalidate VMCB. The vmload instruction will load them.
		cvcpu->header.state_cache.se_valid=true;
	}
	// Load MSRs for System Call (syscall/sysret)
	if(!cvcpu->header.state_cache.sc_valid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_star,cvcpu->header.msrs.star);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_lstar,cvcpu->header.msrs.lstar);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_cstar,cvcpu->header.msrs.cstar);
		noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_sfmask,cvcpu->header.msrs.sfmask);
		// No need to invalidate VMCB. The vmload instruction will load them.
		cvcpu->header.state_cache.sc_valid=true;
	}
	// Set the event injection
	if(cvcpu->header.injected_event.attributes.type)
	{
		if(cvcpu->header.injected_event.attributes.vector==amd64_nmi_interrupt)
		{
			// Special treatments for NMIs.
			// Intercept the iret instructions for NMI-windows.
			noir_svm_vmcb_bts32(cvcpu->vmcb.virt,intercept_instruction1,nvc_svm_intercept_vector1_iret);
			noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_interception);
			cvcpu->special_state.prev_nmi=true;
			cvcpu->header.vcpu_options.blocking_by_nmi=true;
		}
		else
		{
			noir_svm_vmwrite32(cvcpu->vmcb.virt,event_injection,cvcpu->header.injected_event.attributes.value);
			noir_svm_vmwrite32(cvcpu->vmcb.virt,event_error_code,cvcpu->header.injected_event.error_code);
		}
	}
	else
	{
		// Use AMD-V virtual interrupt mechanism to inject an external interrupt.
		nvc_svm_avic_control avic_ctrl;
		avic_ctrl.value=noir_svm_vmread64(cvcpu->vmcb.virt,avic_control);
		avic_ctrl.virtual_irq=cvcpu->special_state.prev_virq=cvcpu->header.injected_event.attributes.valid;
		avic_ctrl.virtual_interrupt_vector=cvcpu->header.injected_event.attributes.vector;
		avic_ctrl.virtual_interrupt_priority=cvcpu->header.injected_event.attributes.priority;
		noir_svm_vmwrite64(cvcpu->vmcb.virt,avic_control,avic_ctrl.value);
		// Note that the AVIC Control field is cached. Invalidate it.
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_tpr);
	}
	// Flush TLB if the ASID is updated.
	if(!cvcpu->header.state_cache.as_valid)
	{
		noir_svm_vmwrite64(cvcpu->vmcb.virt,npt_cr3,cvcpu->vm->nptm[cvcpu->selected_mapping].ncr3.phys);
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_npt);
		noir_svm_vmwrite32(cvcpu->vmcb.virt,guest_asid,cvcpu->vm->nptm[cvcpu->selected_mapping].asid);
		noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_asid);
		cvcpu->header.state_cache.as_valid=true;
	}
	// Flush TLB if the NPT is updated.
	if(!cvcpu->header.state_cache.tl_valid)
	{
		noir_svm_vmwrite8(cvcpu->vmcb.virt,tlb_control,nvc_svm_tlb_control_flush_guest);
		cvcpu->header.state_cache.tl_valid=true;
	}
	// If AVIC is supported, set the Physical APIC ID Entry to be running.
	if(noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_avic))
	{
		nvc_svm_avic_physical_apic_id_entry_p apic_physical=(nvc_svm_avic_physical_apic_id_entry_p)cvcpu->vm->avic_physical.virt;
		apic_physical[cvcpu->proc_id].is_running=true;
		apic_physical[cvcpu->proc_id].host_physical_apic_id=cvcpu->proc_id;
	}
	// Step 3. Switch vCPU to Guest.
	loader_stack->custom_vcpu=cvcpu;
	loader_stack->guest_vmcb_pa=cvcpu->vmcb.phys;
	noir_sti();		// If both RFLAGS.IF in Host and Guest is cleared, physical interrupts might never be unblocked.
	// The context will go to the guest when vmrun is executed.
}

// This function only dumps state saved in VMCB.
void nvc_svm_dump_guest_vcpu_state(noir_svm_custom_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// If the state is marked invalid, do not dump from VMCB in that
	// the state is changed by layered hypervisor.
	if(vcpu->header.state_cache.cr_valid)
	{
		vcpu->header.crs.cr0=noir_svm_vmread64(vmcb,guest_cr0);
		vcpu->header.crs.cr3=noir_svm_vmread64(vmcb,guest_cr3);
		vcpu->header.crs.cr4=noir_svm_vmread64(vmcb,guest_cr4);
	}
	if(vcpu->header.state_cache.cr2valid)
		vcpu->header.crs.cr2=noir_svm_vmread64(vmcb,guest_cr2);
	if(vcpu->header.state_cache.dr_valid)
	{
		vcpu->header.drs.dr6=noir_svm_vmread64(vmcb,guest_dr6);
		vcpu->header.drs.dr7=noir_svm_vmread64(vmcb,guest_dr7);
	}
	if(vcpu->header.state_cache.sr_valid)
	{
		vcpu->header.seg.cs.selector=noir_svm_vmread16(vmcb,guest_cs_selector);
		vcpu->header.seg.ds.selector=noir_svm_vmread16(vmcb,guest_ds_selector);
		vcpu->header.seg.es.selector=noir_svm_vmread16(vmcb,guest_es_selector);
		vcpu->header.seg.ss.selector=noir_svm_vmread16(vmcb,guest_ss_selector);
		vcpu->header.seg.cs.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_cs_attrib));
		vcpu->header.seg.ds.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_ds_attrib));
		vcpu->header.seg.es.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_es_attrib));
		vcpu->header.seg.ss.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_ss_attrib));
		vcpu->header.seg.cs.limit=noir_svm_vmread32(vmcb,guest_cs_limit);
		vcpu->header.seg.ds.limit=noir_svm_vmread32(vmcb,guest_ds_limit);
		vcpu->header.seg.es.limit=noir_svm_vmread32(vmcb,guest_es_limit);
		vcpu->header.seg.ss.limit=noir_svm_vmread32(vmcb,guest_ss_limit);
		vcpu->header.seg.cs.base=noir_svm_vmread64(vmcb,guest_cs_base);
		vcpu->header.seg.ds.base=noir_svm_vmread64(vmcb,guest_ds_base);
		vcpu->header.seg.es.base=noir_svm_vmread64(vmcb,guest_es_base);
		vcpu->header.seg.ss.base=noir_svm_vmread64(vmcb,guest_ss_base);
	}
	if(vcpu->header.state_cache.fg_valid)
	{
		vcpu->header.seg.fs.selector=noir_svm_vmread16(vmcb,guest_fs_selector);
		vcpu->header.seg.gs.selector=noir_svm_vmread16(vmcb,guest_gs_selector);
		vcpu->header.seg.fs.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_fs_attrib));
		vcpu->header.seg.gs.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_gs_attrib));
		vcpu->header.seg.fs.limit=noir_svm_vmread32(vmcb,guest_fs_limit);
		vcpu->header.seg.gs.limit=noir_svm_vmread32(vmcb,guest_gs_limit);
		vcpu->header.seg.fs.base=noir_svm_vmread64(vmcb,guest_fs_base);
		vcpu->header.seg.gs.base=noir_svm_vmread64(vmcb,guest_gs_base);
	}
	if(vcpu->header.state_cache.dt_valid)
	{
		vcpu->header.seg.gdtr.limit=noir_svm_vmread32(vmcb,guest_gdtr_limit);
		vcpu->header.seg.idtr.limit=noir_svm_vmread32(vmcb,guest_idtr_limit);
		vcpu->header.seg.gdtr.base=noir_svm_vmread64(vmcb,guest_gdtr_base);
		vcpu->header.seg.idtr.base=noir_svm_vmread64(vmcb,guest_idtr_base);
	}
	if(vcpu->header.state_cache.lt_valid)
	{
		vcpu->header.seg.ldtr.selector=noir_svm_vmread16(vmcb,guest_ldtr_selector);
		vcpu->header.seg.ldtr.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_ldtr_attrib));
		vcpu->header.seg.ldtr.limit=noir_svm_vmread32(vmcb,guest_ldtr_limit);
		vcpu->header.seg.ldtr.base=noir_svm_vmread64(vmcb,guest_ldtr_base);
		vcpu->header.seg.tr.selector=noir_svm_vmread16(vmcb,guest_tr_selector);
		vcpu->header.seg.tr.attrib=svm_attrib_inverse(noir_svm_vmread16(vmcb,guest_tr_attrib));
		vcpu->header.seg.tr.limit=noir_svm_vmread32(vmcb,guest_tr_limit);
		vcpu->header.seg.tr.base=noir_svm_vmread64(vmcb,guest_tr_base);
	}
	if(vcpu->header.state_cache.sc_valid)
	{
		vcpu->header.msrs.star=noir_svm_vmread64(vmcb,guest_star);
		vcpu->header.msrs.lstar=noir_svm_vmread64(vmcb,guest_lstar);
		vcpu->header.msrs.cstar=noir_svm_vmread64(vmcb,guest_cstar);
		vcpu->header.msrs.sfmask=noir_svm_vmread64(vmcb,guest_sfmask);
	}
	if(vcpu->header.state_cache.se_valid)
	{
		vcpu->header.msrs.sysenter_cs=noir_svm_vmread64(vmcb,guest_sysenter_cs);
		vcpu->header.msrs.sysenter_esp=noir_svm_vmread64(vmcb,guest_sysenter_esp);
		vcpu->header.msrs.sysenter_eip=noir_svm_vmread64(vmcb,guest_sysenter_eip);
	}
	if(vcpu->header.state_cache.tp_valid)
		vcpu->header.crs.cr8=noir_svm_vmread8(vmcb,avic_control)&0xf;
	if(vcpu->header.state_cache.ef_valid)
	{
		vcpu->header.msrs.efer=noir_svm_vmread64(vmcb,guest_efer);
		// Shadow the SVME bit.
		if(!vcpu->shadowed_bits.svme)noir_btr((u32*)&vcpu->header.msrs.efer,amd64_efer_svme);
	}
	if(vcpu->header.state_cache.pa_valid)
		vcpu->header.msrs.pat=noir_svm_vmread64(vmcb,guest_pat);
	// Tell the layered hypervisor that the vCPU state is already synchronized.
	vcpu->header.state_cache.synchronized=1;
}

void nvc_svm_initialize_cvm_vmcb(noir_svm_custom_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// Initialize the VMCB for vCPU.
	nvc_svm_cra_intercept cr_vector;
	nvc_svm_instruction_intercept1 vector1;
	nvc_svm_instruction_intercept2 vector2;
	nvc_svm_avic_control avic_ctrl;
	nvc_svm_npt_control npt_ctrl;
	// Initialize Interceptions
	// INIT Signal and Machine-Check must be intercepted.
	noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_machine_check);
	noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_security_exception);
	// Intercept CR4 Accesses so that CR4.MCE could be shadowed.
	cr_vector.value=0;
	cr_vector.read.cr4=1;
	cr_vector.write.cr4=1;
	noir_svm_vmwrite32(vmcb,intercept_access_cr,cr_vector.value);
	vector1.value=0;
	// All external interrupts must be intercepted for scheduler's sake.
	vector1.intercept_intr=1;
	vector1.intercept_nmi=1;
	vector1.intercept_smi=1;
	// We need to hook the cpuid instruction.
	vector1.intercept_cpuid=1;
	// The invd instruction could corrupt main memory globally. Intercept it.
	vector1.intercept_invd=1;
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
	noir_svm_vmwrite16(vmcb,intercept_instruction2,vector2.value);
	// Initialize TLB: Flushing TLB & Setting ASID.
	// Flush all TLBs associated with this ASID before running it.
	noir_svm_vmwrite32(vmcb,guest_asid,vcpu->vm->nptm->asid);
	noir_svm_vmwrite8(vmcb,tlb_control,nvc_svm_tlb_control_flush_guest);
	// Initialize Interrupt Control.
	avic_ctrl.value=0;
	// Virtual interrupt masking must be enabled. Otherwise, the vCPU might block the host forever.
	avic_ctrl.virtual_interrupt_masking=1;
	noir_svm_vmwrite64(vmcb,avic_control,avic_ctrl.value);
	// Initialize Nested Paging.
	npt_ctrl.value=0;
	npt_ctrl.enable_npt=1;
	noir_svm_vmwrite64(vmcb,npt_control,npt_ctrl.value);
	noir_svm_vmwrite64(vmcb,npt_cr3,vcpu->vm->nptm->ncr3.phys);
	// Initialize IOPM/MSRPM
	noir_svm_vmwrite64(vmcb,iopm_physical_address,vcpu->vm->iopm.phys);
	noir_svm_vmwrite64(vmcb,msrpm_physical_address,vcpu->vm->msrpm.phys);
}

void nvc_svm_set_guest_vcpu_options(noir_svm_custom_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	nvc_svm_instruction_intercept1 vector1;
	vector1.value=noir_svm_vmread32(vmcb,intercept_instruction1);
	// Set interception vectors according to the options.
	// Exceptions
	if(!vcpu->header.vcpu_options.intercept_exceptions)
		noir_svm_vmwrite32(vmcb,intercept_exceptions,(1<<amd64_machine_check)|(1<<amd64_security_exception));
	else
	{
		noir_svm_vmwrite32(vmcb,intercept_exceptions,vcpu->header.exception_bitmap);
		// Ensure that Machine Checks and Security Exceptions are always intercepted.
		noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_machine_check);
		noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_security_exception);
	}
	// CR3
	if(vcpu->header.vcpu_options.intercept_cr3)
	{
		noir_svm_vmcb_bts32(vmcb,intercept_access_cr,0x03);
		noir_svm_vmcb_bts32(vmcb,intercept_access_cr,0x13);
	}
	else
	{
		noir_svm_vmcb_btr32(vmcb,intercept_access_cr,0x03);
		noir_svm_vmcb_btr32(vmcb,intercept_access_cr,0x13);
	}
	// Debug Registers
	noir_svm_vmwrite32(vmcb,intercept_access_dr,vcpu->header.vcpu_options.intercept_drx?0xFFFFFFFF:0);
	// MSR Interceptions
	noir_svm_vmwrite64(vmcb,msrpm_physical_address,vcpu->header.vcpu_options.intercept_msr?vcpu->vm->msrpm_full.phys:vcpu->vm->msrpm.phys);
	// RSM Interception
	vector1.intercept_rsm=vcpu->header.vcpu_options.intercept_rsm;
	// FIXME: Implement NPIEP and Pause-Filters.
	noir_svm_vmwrite32(vmcb,intercept_instruction1,vector1.value);
	// Invalidate VMCB Cache.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_interception);
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_iomsrpm);
}

#if !defined(_hv_type1)
noir_status nvc_svmc_run_vcpu(noir_svm_custom_vcpu_p vcpu)
{
	noir_status st=noir_success;
	if(vcpu->vm->nptm[vcpu->selected_mapping].asid==0xffffffff)
		vcpu->header.exit_context.intercept_code=cv_invalid_state;
	else
	{
		noir_acquire_reslock_shared(vcpu->vm->header.vcpu_list_lock);
		// Abort execution if rescission is specified.
		if(noir_locked_btr64(&vcpu->special_state,63))
			vcpu->header.exit_context.intercept_code=cv_rescission;
		else if(vcpu->header.scheduling_priority<noir_cvm_vcpu_priority_kernel)
			noir_svm_vmmcall(noir_svm_run_custom_vcpu,(ulong_ptr)vcpu);
		else
		{
			do
			{
				noir_svm_vmmcall(noir_svm_run_custom_vcpu,(ulong_ptr)vcpu);
			}while(vcpu->header.exit_context.intercept_code==cv_scheduler_exit);
		}
		noir_release_reslock(vcpu->vm->header.vcpu_list_lock);
	}
	return st;
}

noir_status nvc_svmc_rescind_vcpu(noir_svm_custom_vcpu_p vcpu)
{
	noir_status st=noir_success;
	noir_acquire_reslock_shared(vcpu->vm->header.vcpu_list_lock);
	st=noir_locked_bts64(&vcpu->special_state,63)?noir_already_rescinded:noir_success;
	noir_release_reslock(vcpu->vm->header.vcpu_list_lock);
	return st;
}

noir_status nvc_svmc_select_mapping_for_vcpu(noir_svm_custom_vcpu_p vcpu,u32 mapping_id)
{
	noir_status st=noir_invalid_parameter;
	if(st<vcpu->vm->asid_total)
	{
		st=noir_success;
		if(mapping_id!=vcpu->selected_mapping)
		{
			vcpu->selected_mapping=mapping_id;
			// Switched to another ASID, invalidate VMCB cache.
			vcpu->header.state_cache.as_valid=false;
		}
	}
	return st;
}

noir_status nvc_svmc_retrieve_mapping_id_for_vcpu(noir_svm_custom_vcpu_p vcpu,u32p mapping_id)
{
	*mapping_id=vcpu->selected_mapping;
	return noir_success;
}

u32 nvc_svmc_get_vm_asid(noir_svm_custom_vm_p vm)
{
	return vm->nptm->asid;
}

noir_svm_custom_vcpu_p nvc_svmc_reference_vcpu(noir_svm_custom_vm_p vm,u32 vcpu_id)
{
	return vm->vcpu[vcpu_id];
}

void nvc_svmc_release_vcpu(noir_svm_custom_vcpu_p vcpu)
{
	if(vcpu)
	{
		// Release VMCB.
		if(vcpu->vmcb.virt)noir_free_contd_memory(vcpu->vmcb.virt,page_size);
		// Release XSAVE State Area,
		if(vcpu->header.xsave_area)noir_free_contd_memory(vcpu->header.xsave_area,hvm_p->xfeat.supported_size_max);
		// Remove vCPU from VM.
		if(vcpu->vm)vcpu->vm->vcpu[vcpu->vcpu_id]=null;
		// In addition, remove the vCPU from AVIC.
		if(noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_avic))
		{
			// Remove from AVIC Logical & Physical APIC ID Table.
			nvc_svm_avic_physical_apic_id_entry_p avic_physical=(nvc_svm_avic_physical_apic_id_entry_p)vcpu->vm->avic_physical.virt;
			nvc_svm_avic_logical_apic_id_entry_p avic_logical=(nvc_svm_avic_logical_apic_id_entry_p)vcpu->vm->avic_logical.virt;
			avic_physical[vcpu->proc_id].value=0;
			avic_logical[vcpu->proc_id].value=0;
			// Release APIC Backing Page.
			if(vcpu->apic_backing.virt)noir_free_contd_memory(vcpu->apic_backing.virt,page_size);
		}
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
			if(noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_avic))
			{
				nvc_svm_avic_physical_apic_id_entry_p avic_physical=(nvc_svm_avic_physical_apic_id_entry_p)virtual_machine->avic_physical.virt;
				nvc_svm_avic_logical_apic_id_entry_p avic_logical=(nvc_svm_avic_logical_apic_id_entry_p)virtual_machine->avic_logical.virt;
				// Allocate APIC Backing Page
				vcpu->apic_backing.virt=noir_alloc_contd_memory(page_size);
				if(vcpu->apic_backing.virt)
					vcpu->apic_backing.phys=noir_get_physical_address(vcpu->apic_backing.virt);
				else
					goto alloc_failure;
				// Setup the AVIC Physical/Logical APIC ID Tables.
				avic_physical[vcpu_id].backing_page_pointer=vcpu->apic_backing.phys>>12;
				avic_physical[vcpu_id].valid=true;
				avic_logical[vcpu_id].guest_physical_apic_id=vcpu_id;
				avic_logical[vcpu_id].valid=true;
			}
			// Allocate XSAVE State Area
			vcpu->header.xsave_area=noir_alloc_contd_memory(hvm_p->xfeat.supported_size_max);
			if(vcpu->header.xsave_area==null)goto alloc_failure;
			// Insert the vCPU into the VM.
			virtual_machine->vcpu[vcpu_id]=vcpu;
			vcpu->vcpu_id=vcpu_id;
			vcpu->proc_id=0xffffffff;
			// Mark the owner VM of vCPU.
			vcpu->vm=virtual_machine;
			// Initialize the VMCB via hypercall. It is supposed that only hypervisor can operate VMCB.
			noir_svm_vmmcall(noir_svm_init_custom_vmcb,(ulong_ptr)vcpu);
		}
		*virtual_cpu=vcpu;
		return noir_success;
alloc_failure:
		if(vcpu->vmcb.virt)
			noir_free_contd_memory(vcpu->vmcb.virt,page_size);
		noir_free_nonpg_memory(vcpu);
		return noir_insufficient_resources;
	}
	return noir_vcpu_already_created;
}

void nvc_svmc_free_asid(u32 asid)
{
	noir_acquire_reslock_exclusive(hvm_p->tlb_tagging.asid_pool_lock);
	noir_reset_bitmap(hvm_p->tlb_tagging.asid_pool,asid);
	noir_release_reslock(hvm_p->tlb_tagging.asid_pool_lock);
}

u32 nvc_svmc_alloc_asid()
{
	u32 asid;
	noir_acquire_reslock_exclusive(hvm_p->tlb_tagging.asid_pool_lock);
	asid=noir_find_clear_bit(hvm_p->tlb_tagging.asid_pool,hvm_p->tlb_tagging.limit);
	if(asid!=0xffffffff)
	{
		noir_set_bitmap(hvm_p->tlb_tagging.asid_pool,asid);
		asid+=hvm_p->tlb_tagging.start;
	}
	noir_release_reslock(hvm_p->tlb_tagging.asid_pool_lock);
	return asid;
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
				// This 512GiB page is not yet described.
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
				// This 1GiB page is not yet described.
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

noir_status nvc_svmc_set_mapping(noir_svm_custom_vm_p virtual_machine,u32 mapping_id,noir_cvm_address_mapping_p mapping_info)
{
	noir_status st=noir_invalid_parameter;
	if(mapping_id<virtual_machine->asid_total)
	{
		amd64_addr_translator gpa;
		u32 increment[4]={page_4kb_shift,page_2mb_shift,page_1gb_shift,page_512gb_shift};
		if(virtual_machine->nptm[mapping_id].asid==0xffffffff)virtual_machine->nptm[mapping_id].asid=nvc_svmc_alloc_asid();
		if(virtual_machine->nptm[mapping_id].asid==0xffffffff)
			st=noir_insufficient_resources;
		else
		{
			st=noir_unsuccessful;
			gpa.value=mapping_info->gpa;
			for(u32 i=0;i<mapping_info->pages;i++)
			{
				u64 hva=mapping_info->hva+(i<<increment[mapping_info->attributes.psize]);
				bool valid,locked,large_page;
				if(noir_query_page_attributes((void*)hva,&valid,&locked,&large_page))
				{
					st=noir_user_page_violation;
					if(valid && locked || !mapping_info->attributes.present)
					{
						u64 gpa=mapping_info->gpa+(i<<increment[mapping_info->attributes.psize]);
						u64 hpa=noir_get_user_physical_address((void*)hva);
						st=nvc_svmc_set_page_map(&virtual_machine->nptm[mapping_id],gpa,hpa,mapping_info->attributes);
					}
				}
				if(st!=noir_success)break;
			}
			// Broadcast to all vCPUs that the TLBs are invalid now.
			for(u32 i=0;i<255;i++)
				if(virtual_machine->vcpu[i])
					virtual_machine->vcpu[i]->header.state_cache.tl_valid=false;
		}
	}
	return st;
}

bool static nvc_svmc_clear_gpa_accessing_bit(noir_svm_custom_npt_manager_p nptm,u64 gpa)
{
	// Start from PML4E.
	amd64_addr_translator trans;
	trans.value=gpa;
	if(nptm->ncr3.virt[trans.pml4e_offset].present)
	{
		// Traverse the linked list to check the nested paging.
		noir_npt_pdpte_descriptor_p pdpte_p=nptm->pdpte.head;
		while(pdpte_p)
		{
			if(gpa>=pdpte_p->gpa_start && gpa<pdpte_p->gpa_start+page_512gb_size && pdpte_p->virt[trans.pdpte_offset].present)
			{
				amd64_npt_huge_pdpte_p pdpte_t=(amd64_npt_huge_pdpte_p)&pdpte_p->virt[trans.pdpte_offset];
				if(pdpte_t->huge_pdpte)
				{
					pdpte_t->accessed=pdpte_t->dirty=false;
					return true;
				}
				else
				{
					noir_npt_pde_descriptor_p pde_p=nptm->pde.head;
					while(pde_p)
					{
						if(gpa>=pde_p->gpa_start && gpa<pde_p->gpa_start+page_1gb_size && pde_p->virt[trans.pde_offset].present)
						{
							amd64_npt_large_pde_p pde_t=(amd64_npt_large_pde_p)&pde_p->virt[trans.pde_offset];
							if(pde_t->large_pde)
							{
								pde_t->accessed=pde_t->dirty=false;
								return true;
							}
							else
							{
								noir_npt_pte_descriptor_p pte_p=nptm->pte.head;
								while(pte_p)
								{
									if(gpa>=pte_p->gpa_start && gpa<pte_p->gpa_start+page_2mb_size && pte_p->virt[trans.pte_offset].present)
									{
										pte_p->virt[trans.pte_offset].accessed=pte_p->virt[trans.pte_offset].dirty=false;
										return true;
									}
									pte_p=pte_p->next;
								}
							}
						}
						pde_p=pde_p->next;
					}
				}
			}
			pdpte_p=pdpte_p->next;
		}
	}
	return false;
}

noir_status nvc_svmc_clear_gpa_accessing_bits(noir_svm_custom_vm_p virtual_machine,u32 mapping_id,u64 gpa_start,u32 page_count)
{
	noir_status st=noir_invalid_parameter;
	if(mapping_id<noir_cvm_mapping_limit)
	{
		st=noir_success;
		for(u32 i=0;i<page_count;i++)
		{
			bool r=nvc_svmc_clear_gpa_accessing_bit(&virtual_machine->nptm[mapping_id],gpa_start+(i<<page_4kb_shift));
			if(r==false)
			{
				st=noir_guest_page_absent;
				break;
			}
		}
	}
	return st;
}

u8 static nvc_svmc_query_gpa_accessing_bit(noir_svm_custom_npt_manager_p nptm,u64 gpa)
{
	// Start from PML4E.
	amd64_addr_translator trans;
	trans.value=gpa;
	if(nptm->ncr3.virt[trans.pml4e_offset].present)
	{
		// Traverse the linked list to check the nested paging.
		noir_npt_pdpte_descriptor_p pdpte_p=nptm->pdpte.head;
		while(pdpte_p)
		{
			if(gpa>=pdpte_p->gpa_start && gpa<pdpte_p->gpa_start+page_512gb_size && pdpte_p->virt[trans.pdpte_offset].present)
			{
				amd64_npt_huge_pdpte_p pdpte_t=(amd64_npt_huge_pdpte_p)&pdpte_p->virt[trans.pdpte_offset];
				if(pdpte_t->huge_pdpte)
					return (u8)((pdpte_t->dirty<<1)+pdpte_t->accessed);
				else
				{
					noir_npt_pde_descriptor_p pde_p=nptm->pde.head;
					while(pde_p)
					{
						if(gpa>=pde_p->gpa_start && gpa<pde_p->gpa_start+page_1gb_size && pde_p->virt[trans.pde_offset].present)
						{
							amd64_npt_large_pde_p pde_t=(amd64_npt_large_pde_p)&pde_p->virt[trans.pde_offset];
							if(pde_t->large_pde)
								return (u8)((pde_t->dirty<<1)+pde_t->accessed);
							else
							{
								noir_npt_pte_descriptor_p pte_p=nptm->pte.head;
								while(pte_p)
								{
									if(gpa>=pte_p->gpa_start && gpa<pte_p->gpa_start+page_2mb_size && pte_p->virt[trans.pte_offset].present)
										return (u8)((pte_p->virt[trans.pte_offset].dirty<<1)+pte_p->virt[trans.pte_offset].accessed);
									pte_p=pte_p->next;
								}
							}
						}
					}
					pde_p=pde_p->next;
				}
			}
			pdpte_p=pdpte_p->next;
		}
	}
	return 0xff;
}

noir_status nvc_svmc_query_gpa_accessing_bitmap(noir_svm_custom_vm_p virtual_machine,u32 mapping_id,u64 gpa_start,u32 page_count,void* bitmap,u32 bitmap_size)
{
	noir_status st=noir_invalid_parameter;
	if(mapping_id<noir_cvm_mapping_limit)
	{
		st=noir_buffer_too_small;
		if(page_count<=(bitmap_size<<2))
		{
			// Acquire the vCPU list lock. No vCPUs should be running.
			st=noir_success;
			for(u32 i=0;i<page_count;i++)
			{
				u8 r=nvc_svmc_query_gpa_accessing_bit(&virtual_machine->nptm[mapping_id],gpa_start+(i<<page_4kb_shift));
				if(r==0xff)
				{
					st=noir_guest_page_absent;
					break;
				}
				else
				{
					if(noir_bt(&r,0))
						noir_set_bitmap(bitmap,i<<1);
					else
						noir_reset_bitmap(bitmap,i<<1);
					if(noir_bt(&r,1))
						noir_set_bitmap(bitmap,(i<<1)+1);
					else
						noir_reset_bitmap(bitmap,(i<<1)+1);
				}
			}
		}
	}
	return st;
}

void nvc_svmc_setup_msr_interception_exception(void* msrpm)
{
	void* bitmap1=(void*)((ulong_ptr)msrpm+0x0);
	void* bitmap2=(void*)((ulong_ptr)msrpm+0x800);
	void* bitmap3=(void*)((ulong_ptr)msrpm+0x1000);
	// System-Enter MSRs are unnecessary to be intercepted.
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_cs,0));
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_cs,1));
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_esp,0));
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_esp,1));
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_eip,0));
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_eip,1));
	// System-Call MSRs are unnecessary to be intercepted.
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_star,0));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_star,1));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_lstar,0));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_lstar,1));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_cstar,0));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_cstar,1));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_sfmask,0));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_sfmask,1));
	// FS/GS Base MSRs are unnecessary to be intercepted.
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_fs_base,0));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_fs_base,1));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_gs_base,0));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_gs_base,1));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_kernel_gs_base,0));
	noir_reset_bitmap(bitmap2,svm_msrpm_bit(2,amd64_kernel_gs_base,1));
	// The PAT MSR is unnecessary to be intercepted.
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_pat,0));
	noir_reset_bitmap(bitmap1,svm_msrpm_bit(1,amd64_pat,1));
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
		// Release Nested Paging Structure.
		if(vm->nptm)
		{
			for(u32 i=0;i<vm->asid_total;i++)
			{
				if(vm->nptm[i].ncr3.virt)
					noir_free_contd_memory(vm->nptm[i].ncr3.virt,page_size);
				// Release PDPTE descriptors and paging structures...
				if(vm->nptm[i].pdpte.head)
				{
					noir_npt_pdpte_descriptor_p cur=vm->nptm[i].pdpte.head;
					while(cur)
					{
						noir_npt_pdpte_descriptor_p next=cur->next;
						if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
						noir_free_nonpg_memory(cur);
						cur=next;
					}
				}
				// Release PDE descriptors and paging structures...
				if(vm->nptm[i].pde.head)
				{
					noir_npt_pde_descriptor_p cur=vm->nptm[i].pde.head;
					while(cur)
					{
						noir_npt_pde_descriptor_p next=cur->next;
						if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
						noir_free_nonpg_memory(cur);
						cur=next;
					}
				}
				// Release PTE descriptors and paging structures...
				if(vm->nptm[i].pte.head)
				{
					noir_npt_pte_descriptor_p cur=vm->nptm[i].pte.head;
					while(cur)
					{
						noir_npt_pte_descriptor_p next=cur->next;
						if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
						noir_free_nonpg_memory(cur);
						cur=next;
					}
				}
				// Release ASID.
				if(vm->nptm[i].asid!=0xffffffff)nvc_svmc_free_asid(vm->nptm[i].asid);
			}
			noir_free_nonpg_memory(vm->nptm);
		}
		// Release MSRPM & IOPM
		if(vm->msrpm.virt)noir_free_contd_memory(vm->msrpm.virt,page_size*2);
		if(vm->msrpm_full.virt)noir_free_contd_memory(vm->msrpm_full.virt,page_size*2);
		if(vm->iopm.virt)noir_free_contd_memory(vm->iopm.virt,page_size*3);
		// Release AVIC Pages if AVIC is supported
		if(noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_avic))
		{
			if(vm->avic_logical.virt)noir_free_contd_memory(vm->avic_logical.virt,page_size);
			if(vm->avic_physical.virt)noir_free_contd_memory(vm->avic_physical.virt,page_size);
		}
		// Release VM Structure.
		noir_free_nonpg_memory(vm);
	}
}

// Creating a CVM does not create corresponding vCPUs and lower paging structures!
noir_status nvc_svmc_create_vm(noir_svm_custom_vm_p* virtual_machine,u32 asid_total)
{
	noir_status st=noir_invalid_parameter;
	if(virtual_machine && asid_total)
	{
		noir_svm_custom_vm_p vm=noir_alloc_nonpg_memory(sizeof(noir_svm_custom_vm));
		st=noir_insufficient_resources;
		*virtual_machine=vm;
		if(vm)
		{
			// Allocate NPT Manager.
			vm->nptm=noir_alloc_nonpg_memory(sizeof(noir_svm_custom_npt_manager)*asid_total);
			if(vm->nptm)
			{
				for(u32 i=0;i<asid_total;i++)
				{
					// Create a generic Page Map Level 4 (PML4) Table.
					vm->nptm[i].ncr3.virt=noir_alloc_contd_memory(page_size);
					if(vm->nptm[i].ncr3.virt)
						vm->nptm[i].ncr3.phys=noir_get_physical_address(vm->nptm[i].ncr3.virt);
					else
						goto alloc_failure;
				}
			}
			// Allocate ASID for CVM. Just allocate the first ASID.
			// ASIDs for the rest are allocated until needed.
			vm->asid_total=asid_total;
			vm->nptm->asid=nvc_svmc_alloc_asid();
			for(u32 i=1;i<asid_total;i++)
				vm->nptm[i].asid=0xffffffff;
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
			vm->msrpm_full.virt=noir_alloc_contd_memory(page_size*3);
			if(vm->msrpm.virt)
				vm->msrpm_full.phys=noir_get_physical_address(vm->msrpm_full.virt);
			else
				goto alloc_failure;
			// Allocate vCPU pointer list.
			// According to AVIC, 255 physical cores are permitted.
			vm->vcpu=noir_alloc_nonpg_memory(sizeof(void*)*256);
			if(vm->vcpu==null)goto alloc_failure;
			// Allocate AVIC-related pages if AVIC is supported.
			if(noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_avic))
			{
				// Allocate Logical APIC ID Table
				vm->avic_logical.virt=noir_alloc_contd_memory(page_size);
				if(vm->avic_logical.virt)
					vm->avic_logical.phys=noir_get_physical_address(vm->avic_logical.virt);
				else
					goto alloc_failure;
				// Allocate Physical APIC ID Table
				vm->avic_physical.virt=noir_alloc_contd_memory(page_size);
				if(vm->avic_physical.virt)
					vm->avic_physical.phys=noir_get_physical_address(vm->avic_physical.virt);
				else
					goto alloc_failure;
			}
			// Setup MSR & I/O Interceptions.
			// We want mostly-unconditional exits.
			noir_stosb(vm->msrpm.virt,0xff,noir_svm_msrpm_size);
			noir_stosb(vm->iopm.virt,0xff,noir_svm_iopm_size);
			// Some MSRs are unnessary to be intercepted. Rule them out of interception.
			nvc_svmc_setup_msr_interception_exception(vm->msrpm.virt);
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