/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the customizable VM engine for Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_custom.c
*/

#include <nvdef.h>
#include <nvstatus.h>
#include <nvbdk.h>
#include <vt_intrin.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include "vt_def.h"
#include "vt_vmcs.h"
#include "vt_exit.h"
#include "vt_ept.h"

void nvc_vt_switch_to_host_vcpu(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	noir_vt_initial_stack_p loader_stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	noir_vt_custom_vcpu_p cvcpu=loader_stack->custom_vcpu;
	// Step 1: Save State of the Customizable VM.
	// Save General-Purpose Registers...
	noir_vt_vmread(guest_rsp,&gpr_state->rsp);
	noir_movsp(&cvcpu->header.gpr,gpr_state,sizeof(void*)*2);
	noir_vt_vmread(guest_rip,&cvcpu->header.rip);
	noir_vt_vmread(guest_rflags,&cvcpu->header.rflags);
	// Save Extended Control Registers...
	cvcpu->header.xcrs.xcr0=noir_xgetbv(0);
	// Save Debug Registers...
	cvcpu->header.drs.dr0=noir_readdr0();
	cvcpu->header.drs.dr1=noir_readdr1();
	cvcpu->header.drs.dr2=noir_readdr2();
	cvcpu->header.drs.dr3=noir_readdr3();
	cvcpu->header.drs.dr6=noir_readdr6();
	// Save Control Regisers...
	cvcpu->header.crs.cr2=noir_readcr2();
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
	noir_writedr6(vcpu->cvm_state.drs.dr6);
	// Load Control Registers
	noir_writecr2(vcpu->cvm_state.crs.cr2);
	// Step 3: Switch the vCPU to Host.
	loader_stack->custom_vcpu=&nvc_vt_idle_cvcpu;
	noir_vt_vmptrld(&vcpu->vmcs.phys);
	// The context will go to the host when vmresume is executed.
}

void nvc_vt_switch_to_guest_vcpu(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	noir_vt_initial_stack_p loader_stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	ia32_vmx_msr_auto_p msr_auto=(ia32_vmx_msr_auto_p)cvcpu->msr_auto.virt;
	// IMPORTANT: If vCPU is scheduled to a different processor, clear the state of VMCS is required.
	if(cvcpu->proc_id!=loader_stack->proc_id)
	{
		cvcpu->proc_id=loader_stack->proc_id;
		noir_vt_vmclear(&cvcpu->vmcs.phys);
		// Mark that the vmlaunch instruction is supposed to be executed.
		loader_stack->flags.initial_vmcs=true;
	}
	// Step 1: Save State of the Subverted Host.
	// Please note that it is unnecessary to save states which are already saved in VMCS.
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
	vcpu->cvm_state.drs.dr6=noir_readdr6();
	// Save Control Registers...
	vcpu->cvm_state.crs.cr2=noir_readcr2();
	// Step 2: Switch vCPU to Guest.
	loader_stack->custom_vcpu=cvcpu;
	noir_vt_vmptrld(&cvcpu->vmcs.phys);
	// Step 3: Load Guest State.
	// Load General-Purpose Registers...
	noir_movsp(gpr_state,&cvcpu->header.gpr,sizeof(void*)*2);
	if(!cvcpu->header.state_cache.gprvalid)
	{
		noir_vt_vmwrite(guest_rsp,cvcpu->header.gpr.rsp);
		noir_vt_vmwrite(guest_rip,cvcpu->header.rip);
		noir_vt_vmwrite(guest_rflags,cvcpu->header.rflags);
		cvcpu->header.state_cache.gprvalid=true;
	}
	// Load x87 FPU and SSE/AVX State...
	noir_xrestore(cvcpu->header.xsave_area,maxu64);
	// Load Extended Control Registers...
	noir_xsetbv(0,cvcpu->header.xcrs.xcr0);
	// Load Debug Registers...
	noir_writedr0(cvcpu->header.drs.dr0);
	noir_writedr1(cvcpu->header.drs.dr1);
	noir_writedr2(cvcpu->header.drs.dr2);
	noir_writedr3(cvcpu->header.drs.dr3);
	noir_writedr6(cvcpu->header.drs.dr6);
	if(!cvcpu->header.state_cache.dr_valid)
	{
		noir_vt_vmwrite(guest_dr7,cvcpu->header.drs.dr7);
		cvcpu->header.state_cache.dr_valid=true;
	}
	// Load Control Registers...
	noir_writecr2(cvcpu->header.crs.cr2);
	if(!cvcpu->header.state_cache.cr_valid)
	{
		u64 cr4=cvcpu->header.crs.cr4;
		// Do not use CR0_FIXED_BITS_MSR because Unrestricted Guest is enabled.
		noir_vt_vmwrite(guest_cr0,cvcpu->header.crs.cr0);
		noir_vt_vmwrite(guest_cr3,cvcpu->header.crs.cr3);
		// Intel VT-x requires VMXE to be set.
		cr4|=noir_rdmsr(ia32_vmx_cr4_fixed0);
		cr4&=noir_rdmsr(ia32_vmx_cr4_fixed1);
		noir_vt_vmwrite(guest_cr4,cr4);
		noir_vt_vmwrite(cr0_read_shadow,cvcpu->header.crs.cr0);
		noir_vt_vmwrite(cr4_read_shadow,cvcpu->header.crs.cr4);
		cvcpu->header.state_cache.cr_valid=true;
	}
	// Do not write to CR8 because it is subject to be virtualized.
	// Load Segment Registers...
	if(!cvcpu->header.state_cache.sr_valid)
	{
		// Load segment selectors.
		noir_vt_vmwrite(guest_cs_selector,cvcpu->header.seg.cs.selector);
		noir_vt_vmwrite(guest_ds_selector,cvcpu->header.seg.ds.selector);
		noir_vt_vmwrite(guest_es_selector,cvcpu->header.seg.es.selector);
		noir_vt_vmwrite(guest_ss_selector,cvcpu->header.seg.ss.selector);
		// Load segment attributes.
		noir_vt_vmwrite(guest_cs_access_rights,cvcpu->header.seg.cs.attrib);
		noir_vt_vmwrite(guest_ds_access_rights,cvcpu->header.seg.ds.attrib);
		noir_vt_vmwrite(guest_es_access_rights,cvcpu->header.seg.es.attrib);
		noir_vt_vmwrite(guest_ss_access_rights,cvcpu->header.seg.ss.attrib);
		// Load segment limits.
		noir_vt_vmwrite(guest_cs_limit,cvcpu->header.seg.cs.limit);
		noir_vt_vmwrite(guest_ds_limit,cvcpu->header.seg.ds.limit);
		noir_vt_vmwrite(guest_es_limit,cvcpu->header.seg.es.limit);
		noir_vt_vmwrite(guest_ss_limit,cvcpu->header.seg.ss.limit);
		// Load segment bases.
		noir_vt_vmwrite(guest_cs_base,cvcpu->header.seg.cs.base);
		noir_vt_vmwrite(guest_ds_base,cvcpu->header.seg.ds.base);
		noir_vt_vmwrite(guest_es_base,cvcpu->header.seg.es.base);
		noir_vt_vmwrite(guest_ss_base,cvcpu->header.seg.ss.base);
		// Cache is refreshed. Mark it valid.
		cvcpu->header.state_cache.sr_valid=true;
	}
	if(!cvcpu->header.state_cache.fg_valid)
	{
		// Load segment selectors.
		noir_vt_vmwrite(guest_fs_selector,cvcpu->header.seg.fs.selector);
		noir_vt_vmwrite(guest_gs_selector,cvcpu->header.seg.gs.selector);
		// Load segment attributes.
		noir_vt_vmwrite(guest_fs_access_rights,cvcpu->header.seg.fs.attrib);
		noir_vt_vmwrite(guest_gs_access_rights,cvcpu->header.seg.gs.attrib);
		// Load segment limits.
		noir_vt_vmwrite(guest_fs_limit,cvcpu->header.seg.fs.limit);
		noir_vt_vmwrite(guest_gs_limit,cvcpu->header.seg.gs.limit);
		// Load segment bases.
		noir_vt_vmwrite(guest_fs_base,cvcpu->header.seg.fs.base);
		noir_vt_vmwrite(guest_gs_base,cvcpu->header.seg.gs.base);
		// Cache is refreshed. Mark it valid.
		cvcpu->header.state_cache.fg_valid=true;
	}
	// Load Descriptor Tables...
	if(!cvcpu->header.state_cache.lt_valid)
	{
		// Load segment selectors.
		noir_vt_vmwrite(guest_tr_selector,cvcpu->header.seg.tr.selector);
		noir_vt_vmwrite(guest_ldtr_selector,cvcpu->header.seg.ldtr.selector);
		// Load segment attributes.
		noir_vt_vmwrite(guest_tr_access_rights,cvcpu->header.seg.tr.attrib);
		noir_vt_vmwrite(guest_ldtr_access_rights,cvcpu->header.seg.ldtr.attrib);
		// Load segment limits.
		noir_vt_vmwrite(guest_tr_limit,cvcpu->header.seg.tr.limit);
		noir_vt_vmwrite(guest_ldtr_limit,cvcpu->header.seg.ldtr.limit);
		// Load segment bases.
		noir_vt_vmwrite(guest_tr_base,cvcpu->header.seg.tr.base);
		noir_vt_vmwrite(guest_ldtr_base,cvcpu->header.seg.ldtr.base);
		// Cache is refreshed. Mark it valid.
		cvcpu->header.state_cache.lt_valid=true;
	}
	if(!cvcpu->header.state_cache.dt_valid)
	{
		// Load segment limits.
		noir_vt_vmwrite(guest_gdtr_limit,cvcpu->header.seg.gdtr.limit);
		noir_vt_vmwrite(guest_idtr_limit,cvcpu->header.seg.idtr.limit);
		// Load segment bases.
		noir_vt_vmwrite(guest_gdtr_base,cvcpu->header.seg.gdtr.base);
		noir_vt_vmwrite(guest_idtr_base,cvcpu->header.seg.idtr.base);
		// Cache is refreshed. Mark it valid.
		cvcpu->header.state_cache.dt_valid=true;
	}
	// Load EFER MSR
	if(!cvcpu->header.state_cache.ef_valid)
	{
		ia32_vmx_entry_controls entry_ctrl;
		noir_vt_vmwrite64(guest_msr_ia32_efer,cvcpu->header.msrs.efer);
		// EFER.LMA must be identical to the "IA32e Mode Guest" bit in VM-Entry Controls.
		// This is a very awful point of Intel VT-x.
		noir_vt_vmread(vmentry_controls,&entry_ctrl.value);
		entry_ctrl.ia32e_mode_guest=noir_bt64(&cvcpu->header.msrs.efer,ia32_efer_lma);
		noir_vt_vmwrite(vmentry_controls,entry_ctrl.value);
		// Cache is refreshed. Mark it valid.
		cvcpu->header.state_cache.ef_valid=true;
	}
	// Load PAT MSR
	if(!cvcpu->header.state_cache.pa_valid)
	{
		noir_vt_vmwrite64(guest_msr_ia32_pat,cvcpu->header.msrs.pat);
		cvcpu->header.state_cache.pa_valid=true;
	}
	// Load MSRs for System Call (sysenter/sysexit)
	if(!cvcpu->header.state_cache.se_valid)
	{
		noir_vt_vmwrite(guest_msr_ia32_sysenter_cs,cvcpu->header.msrs.sysenter_cs);
		noir_vt_vmwrite(guest_msr_ia32_sysenter_esp,cvcpu->header.msrs.sysenter_esp);
		noir_vt_vmwrite(guest_msr_ia32_sysenter_eip,cvcpu->header.msrs.sysenter_eip);
		cvcpu->header.state_cache.se_valid=true;
	}
	// Load MSRs for System Call (syscall/sysret)
	if(!cvcpu->header.state_cache.sc_valid)
	{
		msr_auto[noir_vt_cvm_msr_auto_star].data=cvcpu->header.msrs.star;
		msr_auto[noir_vt_cvm_msr_auto_lstar].data=cvcpu->header.msrs.lstar;
		msr_auto[noir_vt_cvm_msr_auto_cstar].data=cvcpu->header.msrs.cstar;
		msr_auto[noir_vt_cvm_msr_auto_sfmask].data=cvcpu->header.msrs.sfmask;
		cvcpu->header.state_cache.sc_valid=true;
	}
	// Set the event injection
	if(!cvcpu->header.injected_event.attributes.valid)
		noir_vt_vmwrite(vmentry_interruption_information_field,0);
	else
	{
		switch(cvcpu->header.injected_event.attributes.type)
		{
			case ia32_external_interrupt:
			{
				// Virtualize RFLAGS.IF and CR8.TPR here. Intel VT-x lacks support for local APIC.
				// If the priority is not sufficiently high, wait until the guest sets a lower CR8.
				if(cvcpu->header.injected_event.attributes.priority>(cvcpu->header.crs.cr8&0xf))
				{
					if(!noir_bt(&cvcpu->header.rflags,ia32_rflags_if))
						noir_vt_inject_event((u8)cvcpu->header.injected_event.attributes.vector,ia32_external_interrupt,true,0,cvcpu->header.injected_event.error_code);
					else
					{
						// Set up an interrupt window to wait for the chance for injection.
						ia32_vmx_priproc_controls proc_ctrl1;
						noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl1.value);
						proc_ctrl1.interrupt_window_exiting=true;
						noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl1.value);
					}
				}
				break;
			}
			case ia32_non_maskable_interrupt:
			{
				ia32_vmx_interruptibility_state int_state;
				noir_vt_vmread(guest_interruptibility_state,&int_state.value);
				if(!int_state.blocking_by_nmi)
					noir_vt_inject_event(ia32_nmi_interrupt,ia32_non_maskable_interrupt,false,0,0);
				else
				{
					// Set up an NMI-window to wait for the chance for injection.
					ia32_vmx_priproc_controls proc_ctrl1;
					noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl1.value);
					proc_ctrl1.nmi_window_exiting=true;
					noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl1.value);
				}
				break;
			}
			default:
			{
				noir_vt_vmwrite(vmentry_interruption_information_field,cvcpu->header.injected_event.attributes.value);
				noir_vt_vmwrite(vmentry_exception_error_code,cvcpu->header.injected_event.error_code);
				break;
			}
		}
	}
}

void nvc_vt_dump_vcpu_state(noir_vt_custom_vcpu_p vcpu)
{
	ia32_vmx_msr_auto_p msr_auto=(ia32_vmx_msr_auto_p)vcpu->msr_auto.virt;
	// If the state is marked invalid, do not dump from VMCS in that
	// the state is changed by layered hypervisor.
	if(vcpu->header.state_cache.cr_valid)
	{
		noir_vt_vmread(guest_cr0,&vcpu->header.crs.cr0);
		noir_vt_vmread(guest_cr3,&vcpu->header.crs.cr3);
		noir_vt_vmread(guest_cr4,&vcpu->header.crs.cr4);
		vcpu->header.crs.cr4&=~ia32_cr4_vmxe_bit;
	}
	if(vcpu->header.state_cache.dr_valid)
		noir_vt_vmread(guest_dr7,&vcpu->header.drs.dr7);
	if(vcpu->header.state_cache.sr_valid)
	{
		u32 cs_ar,ds_ar,es_ar,ss_ar;
		noir_vt_vmread(guest_cs_selector,&vcpu->header.seg.cs.selector);
		noir_vt_vmread(guest_ds_selector,&vcpu->header.seg.ds.selector);
		noir_vt_vmread(guest_es_selector,&vcpu->header.seg.es.selector);
		noir_vt_vmread(guest_ss_selector,&vcpu->header.seg.ss.selector);
		noir_vt_vmread(guest_cs_access_rights,&cs_ar);
		noir_vt_vmread(guest_ds_access_rights,&ds_ar);
		noir_vt_vmread(guest_es_access_rights,&es_ar);
		noir_vt_vmread(guest_ss_access_rights,&ss_ar);
		vcpu->header.seg.cs.attrib=(u16)cs_ar;
		vcpu->header.seg.ds.attrib=(u16)ds_ar;
		vcpu->header.seg.es.attrib=(u16)es_ar;
		vcpu->header.seg.ss.attrib=(u16)ss_ar;
		noir_vt_vmread(guest_cs_limit,&vcpu->header.seg.cs.limit);
		noir_vt_vmread(guest_ds_limit,&vcpu->header.seg.ds.limit);
		noir_vt_vmread(guest_es_limit,&vcpu->header.seg.es.limit);
		noir_vt_vmread(guest_ss_limit,&vcpu->header.seg.ss.limit);
		noir_vt_vmread(guest_cs_base,&vcpu->header.seg.cs.base);
		noir_vt_vmread(guest_ds_base,&vcpu->header.seg.ds.base);
		noir_vt_vmread(guest_es_base,&vcpu->header.seg.es.base);
		noir_vt_vmread(guest_ss_base,&vcpu->header.seg.ss.base);
	}
	if(vcpu->header.state_cache.fg_valid)
	{
		u32 fs_ar,gs_ar;
		noir_vt_vmread(guest_fs_selector,&vcpu->header.seg.fs.selector);
		noir_vt_vmread(guest_gs_selector,&vcpu->header.seg.gs.selector);
		noir_vt_vmread(guest_fs_access_rights,&fs_ar);
		noir_vt_vmread(guest_gs_access_rights,&gs_ar);
		vcpu->header.seg.fs.attrib=(u16)fs_ar;
		vcpu->header.seg.gs.attrib=(u16)gs_ar;
		noir_vt_vmread(guest_fs_limit,&vcpu->header.seg.fs.limit);
		noir_vt_vmread(guest_gs_limit,&vcpu->header.seg.gs.limit);
		noir_vt_vmread(guest_fs_base,&vcpu->header.seg.fs.base);
		noir_vt_vmread(guest_gs_base,&vcpu->header.seg.gs.base);
		vcpu->header.msrs.gsswap=msr_auto[noir_vt_cvm_msr_auto_gsswap].data;
	}
	if(vcpu->header.state_cache.dt_valid)
	{
		noir_vt_vmread(guest_gdtr_limit,&vcpu->header.seg.gdtr.limit);
		noir_vt_vmread(guest_idtr_limit,&vcpu->header.seg.idtr.limit);
		noir_vt_vmread(guest_gdtr_base,&vcpu->header.seg.gdtr.base);
		noir_vt_vmread(guest_idtr_base,&vcpu->header.seg.idtr.base);
	}
	if(vcpu->header.state_cache.lt_valid)
	{
		u32 tr_ar,ldtr_ar;
		noir_vt_vmread(guest_tr_selector,&vcpu->header.seg.tr.selector);
		noir_vt_vmread(guest_ldtr_selector,&vcpu->header.seg.ldtr.selector);
		noir_vt_vmread(guest_tr_access_rights,&tr_ar);
		noir_vt_vmread(guest_ldtr_access_rights,&ldtr_ar);
		vcpu->header.seg.tr.attrib=(u16)tr_ar;
		vcpu->header.seg.ldtr.attrib=(u16)ldtr_ar;
		noir_vt_vmread(guest_tr_limit,&vcpu->header.seg.tr.limit);
		noir_vt_vmread(guest_ldtr_limit,&vcpu->header.seg.ldtr.limit);
		noir_vt_vmread(guest_tr_base,&vcpu->header.seg.tr.base);
		noir_vt_vmread(guest_ldtr_base,&vcpu->header.seg.ldtr.base);
	}
	if(vcpu->header.state_cache.sc_valid)
	{
		vcpu->header.msrs.star=msr_auto[noir_vt_cvm_msr_auto_star].data;
		vcpu->header.msrs.lstar=msr_auto[noir_vt_cvm_msr_auto_lstar].data;
		vcpu->header.msrs.cstar=msr_auto[noir_vt_cvm_msr_auto_cstar].data;
		vcpu->header.msrs.sfmask=msr_auto[noir_vt_cvm_msr_auto_sfmask].data;
	}
	if(vcpu->header.state_cache.se_valid)
	{
		noir_vt_vmread(guest_msr_ia32_sysenter_cs,&vcpu->header.msrs.sysenter_cs);
		noir_vt_vmread(guest_msr_ia32_sysenter_esp,&vcpu->header.msrs.sysenter_esp);
		noir_vt_vmread(guest_msr_ia32_sysenter_eip,&vcpu->header.msrs.sysenter_eip);
	}
}

void static nvc_vt_initialize_cvm_pin_based_controls(bool true_msr)
{
	ia32_vmx_pinbased_ctrl_msr pin_ctrl_msr;
	ia32_vmx_pinbased_controls pin_ctrl;
	// Read Capability MSR.
	if(true_msr)
		pin_ctrl_msr.value=noir_rdmsr(ia32_vmx_true_pinbased_ctrl);
	else
		pin_ctrl_msr.value=noir_rdmsr(ia32_vmx_pinbased_ctrl);
	// Setup necessary fields.
	pin_ctrl.value=0;
	// We should intercept external interrupts and non-maskable interrupts.
	pin_ctrl.external_interrupt_exiting=1;
	pin_ctrl.nmi_exiting=1;
	pin_ctrl.virtual_nmi=1;
	// Filter unsupported fields.
	pin_ctrl.value|=pin_ctrl_msr.allowed0_settings.value;
	pin_ctrl.value&=pin_ctrl_msr.allowed1_settings.value;
	// Write to VMCS.
	noir_vt_vmwrite(pin_based_vm_execution_controls,pin_ctrl.value);
}

void static nvc_vt_initialize_cvm_proc_based_controls(bool true_msr)
{
	ia32_vmx_priproc_controls proc_ctrl1;
	ia32_vmx_2ndproc_controls proc_ctrl2;
	ia32_vmx_priproc_ctrl_msr proc_ctrl1_msr;
	ia32_vmx_2ndproc_ctrl_msr proc_ctrl2_msr;
	// Read Capability MSR.
	if(true_msr)
		proc_ctrl1_msr.value=noir_rdmsr(ia32_vmx_true_priproc_ctrl);
	else
		proc_ctrl1_msr.value=noir_rdmsr(ia32_vmx_priproc_ctrl);
	proc_ctrl2_msr.value=noir_rdmsr(ia32_vmx_2ndproc_ctrl);
	// Setup Primary Processor-Based VM-Execution Control Fields...
	proc_ctrl1.value=0;
	proc_ctrl1.hlt_exiting=1;					// The hlt instruction must be intercepted.
	proc_ctrl1.cr8_load_exiting=1;				// The Mov-From-CR8 must be intercepted.
	proc_ctrl1.cr8_store_exiting=1;				// The Mov-To-CR8 must be intercepted.
	proc_ctrl1.unconditional_io_exiting=1;		// The I/O instructions must be intercepted.
	proc_ctrl1.use_msr_bitmap=1;				// MSRs can be conditionally intercepted.
	proc_ctrl1.activate_secondary_controls=1;	// Secondary Controls must be activated.
	// Setup Secondary Processor-Based VM-Execution Control Fields...
	proc_ctrl2.value=0;
	proc_ctrl2.enable_ept=1;					// Required for Memory Virtualization.
	proc_ctrl2.enable_rdtscp=1;					// Must be set to allow rdtscp instruction.
	proc_ctrl2.enable_vpid=1;					// Reduce TLB-Flushing on Context-Switching.
	proc_ctrl2.unrestricted_guest=1;			// Required for Real-Mode Guests.
	proc_ctrl2.enable_invpcid=1;				// Must be set to allow invpcid instruction.
	proc_ctrl2.enable_xsaves_xrstors=1;			// Must be set to allow xsaves/xrstors instructions.
	proc_ctrl2.enable_user_wait_and_pause=1;	// Must be set to allow certain user TSX instructions.
	// Filter unsupported fields.
	proc_ctrl1.value|=proc_ctrl1_msr.allowed0_settings.value;
	proc_ctrl1.value&=proc_ctrl1_msr.allowed1_settings.value;
	proc_ctrl2.value|=proc_ctrl2_msr.allowed0_settings.value;
	proc_ctrl2.value&=proc_ctrl2_msr.allowed1_settings.value;
	// Write to VMCS.
	noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl1.value);
	noir_vt_vmwrite(secondary_processor_based_vm_execution_controls,proc_ctrl2.value);
}

void static nvc_vt_initialize_cvm_vmexit_controls(bool true_msr)
{
	ia32_vmx_exit_controls exit_ctrl;
	ia32_vmx_exit_ctrl_msr exit_ctrl_msr;
	// Read Capability MSR.
	if(true_msr)
		exit_ctrl_msr.value=noir_rdmsr(ia32_vmx_true_exit_ctrl);
	else
		exit_ctrl_msr.value=noir_rdmsr(ia32_vmx_exit_ctrl);
	// Setup necessary fields.
	exit_ctrl.value=0;
	exit_ctrl.save_debug_controls=1;
#if defined(_amd64)
	exit_ctrl.host_address_space_size=1;
#endif
	exit_ctrl.save_ia32_pat=1;
	exit_ctrl.load_ia32_pat=1;
	exit_ctrl.save_ia32_efer=1;
	exit_ctrl.load_ia32_efer=1;
	// Filter unwanted fields.
	exit_ctrl.value|=exit_ctrl_msr.allowed0_settings.value;
	exit_ctrl.value&=exit_ctrl_msr.allowed1_settings.value;
	// Write to VMCS.
	noir_vt_vmwrite(vmexit_controls,exit_ctrl.value);
}

void static nvc_vt_initialize_cvm_vmentry_controls(bool true_msr)
{
	ia32_vmx_entry_controls entry_ctrl;
	ia32_vmx_entry_ctrl_msr entry_ctrl_msr;
	// Read Capability MSR.
	if(true_msr)
		entry_ctrl_msr.value=noir_rdmsr(ia32_vmx_true_entry_ctrl);
	else
		entry_ctrl_msr.value=noir_rdmsr(ia32_vmx_entry_ctrl);
	// Setup necessary fields.
	entry_ctrl.value=0;
	entry_ctrl.load_debug_controls=1;
	entry_ctrl.load_ia32_pat=1;
	entry_ctrl.load_ia32_efer=1;
	// Filter unwanted fields.
	entry_ctrl.value|=entry_ctrl_msr.allowed0_settings.value;
	entry_ctrl.value&=entry_ctrl_msr.allowed1_settings.value;
	// Write to VMCS.
	noir_vt_vmwrite(vmentry_controls,entry_ctrl.value);
}

void static nvc_vt_initialize_cvm_host_state(noir_vt_vcpu_p vcpu)
{
	noir_vt_initial_stack_p loader_stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	// Read the processor state.
	noir_processor_state pstate;
	noir_save_processor_state(&pstate);
	// Host State Area - Segment Selectors
	noir_vt_vmwrite(host_cs_selector,pstate.cs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_ds_selector,pstate.ds.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_es_selector,pstate.es.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_fs_selector,pstate.fs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_gs_selector,pstate.gs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_ss_selector,pstate.ss.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_tr_selector,pstate.tr.selector & selector_rplti_mask);
	// Host State Area - Segment Bases
	noir_vt_vmwrite(host_fs_base,(ulong_ptr)pstate.fs.base);
	noir_vt_vmwrite(host_gs_base,(ulong_ptr)pstate.gs.base);
	noir_vt_vmwrite(host_tr_base,(ulong_ptr)pstate.tr.base);
	// Host State Area - Descriptor Tables
	noir_vt_vmwrite(host_gdtr_base,(ulong_ptr)pstate.gdtr.base);
	noir_vt_vmwrite(host_idtr_base,(ulong_ptr)pstate.idtr.base);
	// Host State Area - Control Registers
	noir_vt_vmwrite(host_cr0,pstate.cr0);
	noir_vt_vmwrite(host_cr3,system_cr3);	// We should use the system page table.
	noir_vt_vmwrite(host_cr4,pstate.cr4);
	noir_vt_vmwrite(host_msr_ia32_efer,pstate.efer);
	noir_vt_vmwrite(host_msr_ia32_pat,pstate.pat);
	// Host State Area - Stack Pointer, Instruction Pointer
	noir_vt_vmwrite(host_rsp,(ulong_ptr)loader_stack);
	noir_vt_vmwrite(host_rip,(ulong_ptr)nvc_vt_exit_handler_a);
}

void static nvc_vt_initialize_cvm_msr_auto_list(noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Note that MSRs to be loaded on VM-Exit is stored in Host vCPU.
	ia32_vmx_msr_auto_p auto_list=(ia32_vmx_msr_auto_p)cvcpu->msr_auto.virt;
	// Initialize the MSR-Auto List.
	auto_list[noir_vt_cvm_msr_auto_star].index=ia32_star;
	auto_list[noir_vt_cvm_msr_auto_lstar].index=ia32_lstar;
	auto_list[noir_vt_cvm_msr_auto_cstar].index=ia32_cstar;
	auto_list[noir_vt_cvm_msr_auto_sfmask].index=ia32_fmask;
	auto_list[noir_vt_cvm_msr_auto_gsswap].index=ia32_kernel_gs_base;
	// Write to VMCS.
	noir_vt_vmwrite64(vmentry_msr_load_address,cvcpu->msr_auto.phys);
	noir_vt_vmwrite64(vmexit_msr_load_address,vcpu->msr_auto.phys+0xC00);
	noir_vt_vmwrite64(vmexit_msr_store_address,cvcpu->msr_auto.phys);
	noir_vt_vmwrite(vmexit_msr_store_count,noir_vt_cvm_msr_auto_max);
	noir_vt_vmwrite(vmexit_msr_load_count,noir_vt_cvm_msr_auto_max);
	noir_vt_vmwrite(vmentry_msr_load_count,noir_vt_cvm_msr_auto_max);
}

void nvc_vt_initialize_cvm_vmcs(noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	ia32_vmx_basic_msr vt_basic_msr;
	u8 vst;
	vt_basic_msr.value=noir_rdmsr(ia32_vmx_basic);
	// Initialize the state of VMCS.
	*(u32*)cvcpu->vmcs.virt=(u32)vt_basic_msr.revision_id;
	vst=noir_vt_vmclear(&cvcpu->vmcs.phys);
	if(vst==vmx_success)
	{
		// Switch to vCPU of the Guest in order to initialize it.
		vst=noir_vt_vmptrld(&cvcpu->vmcs.phys);
		if(vst==vmx_success)
		{
			// Initialize Control Fields...
			nvc_vt_initialize_cvm_pin_based_controls(vt_basic_msr.use_true_msr);
			nvc_vt_initialize_cvm_proc_based_controls(vt_basic_msr.use_true_msr);
			nvc_vt_initialize_cvm_vmexit_controls(vt_basic_msr.use_true_msr);
			nvc_vt_initialize_cvm_vmentry_controls(vt_basic_msr.use_true_msr);
			nvc_vt_initialize_cvm_host_state(vcpu);
			// Miscellaneous stuff...
			nvc_vt_initialize_cvm_msr_auto_list(vcpu,cvcpu);
			noir_vt_vmwrite64(address_of_msr_bitmap,cvcpu->vm->msr_bitmap.phys);
			noir_vt_vmwrite64(ept_pointer,cvcpu->vm->eptm.eptp.phys);
			noir_vt_vmwrite64(vmcs_link_pointer,maxu64);
			noir_vt_vmwrite(virtual_processor_identifier,cvcpu->vm->vpid);
			noir_vt_vmwrite(cr0_guest_host_mask,0);
			noir_vt_vmwrite(cr4_guest_host_mask,ia32_cr4_vmxe_bit);
			noir_vt_vmwrite(guest_interruptibility_state,0);
			noir_vt_vmwrite(guest_activity_state,guest_is_active);
			// Switch back to Host vCPU.
			noir_vt_vmptrld(&vcpu->vmcs.phys);
		}
	}
}

void nvc_vt_set_guest_vcpu_options(noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	ia32_vmx_priproc_controls proc_ctrl1;
	// Load VMCS for CVM.
	noir_vt_vmptrld(&cvcpu->vmcs.phys);
	// Read Primary Processor-Based VM Execution Controls
	noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl1.value);
	// Set interception vectors according to the options.
	if(cvcpu->header.vcpu_options.intercept_exceptions)
		noir_vt_vmwrite(exception_bitmap,(1<<ia32_machine_check)|cvcpu->header.exception_bitmap);
	else
		noir_vt_vmwrite(exception_bitmap,1<<ia32_machine_check);
	// CR3
	proc_ctrl1.cr3_load_exiting=cvcpu->header.vcpu_options.intercept_cr3;
	proc_ctrl1.cr3_store_exiting=cvcpu->header.vcpu_options.intercept_cr3;
	// Debug Registers
	proc_ctrl1.mov_dr_exiting=cvcpu->header.vcpu_options.intercept_drx;
	// MSR Interceptions
	proc_ctrl1.use_msr_bitmap=!cvcpu->header.vcpu_options.intercept_msr;
	// Write to VMCS.
	noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl1.value);
	// Load Host's VMCS.
	noir_vt_vmptrld(&vcpu->vmcs.phys);
}

#if !defined(_hv_type1)
noir_status nvc_vtc_run_vcpu(noir_vt_custom_vcpu_p vcpu)
{
	noir_status st=noir_success;
	noir_acquire_reslock_shared(vcpu->vm->header.vcpu_list_lock);
	// Abort execution if rescission is specified.
	if(noir_locked_btr64(&vcpu->special_state,63))
		vcpu->header.exit_context.intercept_code=cv_rescission;
	else
		noir_vt_vmcall(noir_vt_run_custom_vcpu,(ulong_ptr)vcpu);
	noir_release_reslock(vcpu->vm->header.vcpu_list_lock);
	return st;
}

noir_status nvc_vtc_rescind_vcpu(noir_vt_custom_vcpu_p vcpu)
{
	noir_status st=noir_success;
	noir_acquire_reslock_shared(vcpu->vm->header.vcpu_list_lock);
	st=noir_locked_bts64(&vcpu->special_state,63)?noir_already_rescinded:noir_success;
	noir_release_reslock(vcpu->vm->header.vcpu_list_lock);
	return st;
}

u32 nvc_vtc_get_vm_asid(noir_vt_custom_vm_p vm)
{
	return vm->vpid;
}

noir_vt_custom_vcpu_p nvc_vtc_reference_vcpu(noir_vt_custom_vm_p vm,u32 vcpu_id)
{
	return vm->vcpu[vcpu_id];
}

u16 nvc_vtc_alloc_vpid()
{
	u32 asid;
	noir_acquire_reslock_exclusive(hvm_p->tlb_tagging.vpid_pool_lock);
	asid=noir_find_clear_bit(hvm_p->tlb_tagging.vpid_pool,hvm_p->tlb_tagging.limit);
	if(asid!=0xffffffff)
	{
		noir_set_bitmap(hvm_p->tlb_tagging.vpid_pool,asid);
		asid+=hvm_p->tlb_tagging.start;
	}
	noir_release_reslock(hvm_p->tlb_tagging.vpid_pool_lock);
	return (u32)asid;
}

void static nvc_vtc_set_pte_entry(ia32_ept_pte_p entry,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	entry->value=0;
	// Protection attributes...
	entry->read=map_attrib.present;
	entry->write=map_attrib.write;
	entry->execute=map_attrib.execute;
	// Caching attributes...
	entry->memory_type=map_attrib.caching;
	// Address translation...
	entry->page_offset=page_4kb_count(hpa);
}

void static nvc_vtc_set_pde_entry(ia32_ept_pde_p entry,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	entry->value=0;
	if(map_attrib.psize!=1)
	{
		entry->read=entry->write=entry->execute=1;
		entry->pte_offset=page_4kb_count(hpa);
	}
	else
	{
		ia32_ept_large_pde_p large_pde=(ia32_ept_large_pde_p)entry;
		// Protection attributes...
		large_pde->read=map_attrib.present;
		large_pde->write=map_attrib.write;
		large_pde->execute=map_attrib.execute;
		// Caching attributes...
		large_pde->memory_type=map_attrib.caching;
		// Address translation...
		large_pde->page_offset=page_2mb_count(hpa);
		large_pde->large_pde=1;
	}
}

void static nvc_vtc_set_pdpte_entry(ia32_ept_pdpte_p entry,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	entry->value=0;
	if(map_attrib.psize!=2)
	{
		entry->read=entry->write=entry->execute=1;
		entry->pde_offset=page_4kb_count(hpa);
	}
	else
	{
		ia32_ept_huge_pdpte_p huge_pdpte=(ia32_ept_huge_pdpte_p)entry;
		// Protection attributes...
		huge_pdpte->read=map_attrib.present;
		huge_pdpte->write=map_attrib.write;
		huge_pdpte->execute=map_attrib.execute;
		// Caching attributes...
		huge_pdpte->memory_type=map_attrib.caching;
		// Address translation...
		huge_pdpte->page_offset=page_1gb_count(hpa);
		huge_pdpte->huge_pdpte=1;
	}
}

void static nvc_vtc_set_pml4e_entry(ia32_ept_pml4e_p entry,u64 hpa)
{
	entry->value=0;
	entry->read=entry->write=entry->execute=1;
	entry->pdpte_offset=page_4kb_count(hpa);
}

noir_status static nvc_vtc_create_1gb_page_map(noir_vt_custom_ept_manager_p ept_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_insufficient_resources;
	noir_ept_pdpte_descriptor_p pdpte_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pdpte_descriptor));
	if(pdpte_p)
	{
		pdpte_p->virt=noir_alloc_contd_memory(page_size);
		if(pdpte_p->virt==null)
			noir_free_nonpg_memory(pdpte_p);
		else
		{
			ia32_addr_translator gpa_t;
			gpa_t.value=gpa;
			// Setup PDPTE descriptor.
			pdpte_p->phys=noir_get_physical_address(pdpte_p->virt);
			pdpte_p->gpa_start=page_512gb_base(gpa);
			// Do mapping - this level.
			nvc_vtc_set_pdpte_entry(&pdpte_p->virt[gpa_t.pdpte_offset],hpa,map_attrib);
			// Do mapping - prior level.
			// Note that PML4E is already described.
			nvc_vtc_set_pml4e_entry(&ept_manager->eptp.virt[gpa_t.pml4e_offset],pdpte_p->phys);
			// Add to the linked list.
			if(ept_manager->pdpte.head)
				ept_manager->pdpte.tail->next=pdpte_p;
			else
				ept_manager->pdpte.head=pdpte_p;
			ept_manager->pdpte.tail=pdpte_p;
			st=noir_success;
		}
	}
	return st;
}

noir_status static nvc_vtc_create_2mb_page_map(noir_vt_custom_ept_manager_p ept_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_insufficient_resources;
	noir_ept_pde_descriptor_p pde_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pde_descriptor));
	if(pde_p)
	{
		pde_p->virt=noir_alloc_contd_memory(page_size);
		if(pde_p->virt==null)
			noir_free_nonpg_memory(pde_p);
		else
		{
			noir_ept_pdpte_descriptor_p cur=ept_manager->pdpte.head;
			ia32_addr_translator gpa_t;
			gpa_t.value=gpa;
			// Setup PDE descriptor
			pde_p->phys=noir_get_physical_address(pde_p->virt);
			pde_p->gpa_start=page_1gb_base(gpa);
			// Do mapping
			nvc_vtc_set_pde_entry(&pde_p->virt[gpa_t.pte_offset],hpa,map_attrib);
			// Add to the linked list.
			if(ept_manager->pde.head)
				ept_manager->pde.tail->next=pde_p;
			else
				ept_manager->pde.head=pde_p;
			ept_manager->pde.tail=pde_p;
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
				st=nvc_vtc_create_1gb_page_map(ept_manager,gpa,0,null_map);
				if(st==noir_success)cur=ept_manager->pdpte.tail;
			}
			if(cur)
			{
				nvc_vtc_set_pdpte_entry(&cur->virt[gpa_t.pde_offset],pde_p->phys,map_attrib);
				st=noir_success;
			}
		}
	}
	return st;
}

noir_status static nvc_vtc_create_4kb_page_map(noir_vt_custom_ept_manager_p ept_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_insufficient_resources;
	noir_ept_pte_descriptor_p pte_p=noir_alloc_nonpg_memory(sizeof(noir_ept_pte_descriptor));
	if(pte_p)
	{
		pte_p->virt=noir_alloc_contd_memory(page_size);
		if(pte_p->virt==null)
			noir_free_nonpg_memory(pte_p);
		else
		{
			noir_ept_pde_descriptor_p cur=ept_manager->pde.head;
			ia32_addr_translator gpa_t;
			gpa_t.value=gpa;
			// Setup PTE descriptor
			pte_p->phys=noir_get_physical_address(pte_p->virt);
			pte_p->gpa_start=page_2mb_base(gpa);
			// Do mapping
			nvc_vtc_set_pte_entry(&pte_p->virt[gpa_t.pte_offset],0,map_attrib);
			// Add to the linked list
			if(ept_manager->pte.head)
				ept_manager->pte.tail->next=pte_p;
			else
				ept_manager->pte.head=pte_p;
			ept_manager->pte.tail=pte_p;
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
				st=nvc_vtc_create_2mb_page_map(ept_manager,gpa,0,null_map);
				if(st==noir_success)cur=ept_manager->pde.tail;
			}
			if(cur)
			{
				nvc_vtc_set_pde_entry(&cur->virt[gpa_t.pde_offset],pte_p->phys,map_attrib);
				st=noir_success;
			}
		}
	}
	return st;
}

noir_status static nvc_vtc_set_page_map(noir_vt_custom_ept_manager_p npt_manager,u64 gpa,u64 hpa,noir_cvm_mapping_attributes map_attrib)
{
	noir_status st=noir_unsuccessful;
	ia32_addr_translator gpa_t;
	gpa_t.value=gpa;
	switch(map_attrib.psize)
	{
		case 0:
		{
			// Search for existing PTEs.
			noir_ept_pte_descriptor_p cur=npt_manager->pte.head;
			while(cur)
			{
				if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_2mb_size)break;
				cur=cur->next;
			}
			if(!cur)
			{
				noir_cvm_mapping_attributes null_map={0};
				// This 2MiB page is not described yet.
				st=nvc_vtc_create_4kb_page_map(npt_manager,gpa,0,null_map);
				if(st==noir_success)cur=npt_manager->pte.tail;
			}
			if(cur)
			{
				nvc_vtc_set_pte_entry(&cur->virt[gpa_t.pte_offset],hpa,map_attrib);
				st=noir_success;
			}
			break;
		}
		default:
		{
			// By default, large-page mapping is not implemented.
			st=noir_not_implemented;
			break;
		}
	}
	return st;
}

noir_status nvc_vtc_set_mapping(noir_vt_custom_vm_p virtual_machine,noir_cvm_address_mapping_p mapping_info)
{
	noir_status st=noir_unsuccessful;
	ia32_addr_translator gpa;
	u32 increment[4]={page_4kb_shift,page_2mb_shift,page_1gb_shift,page_512gb_shift};
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
				st=nvc_vtc_set_page_map(&virtual_machine->eptm,gpa,hpa,mapping_info->attributes);
			}
		}
		if(st!=noir_success)break;
	}
	return st;
}

void nvc_vtc_release_vcpu(noir_vt_custom_vcpu_p virtual_processor)
{
	if(virtual_processor)
	{
		// Release VMCS
		if(virtual_processor->vmcs.virt)
			noir_free_contd_memory(virtual_processor->vmcs.virt,page_size);
		// Release MSR-Auto List.
		if(virtual_processor->msr_auto.virt)
			noir_free_contd_memory(virtual_processor->msr_auto.virt,page_size);
		// Release Extended State.
		if(virtual_processor->header.xsave_area)
			noir_free_contd_memory(virtual_processor->header.xsave_area,page_size);
		noir_free_nonpg_memory(virtual_processor);
	}
}

noir_status nvc_vtc_create_vcpu(noir_vt_custom_vcpu_p *virtual_processor,noir_vt_custom_vm_p virtual_machine,u32 vcpu_id)
{
	noir_status st=noir_invalid_parameter;
	if(virtual_machine && virtual_processor)
	{
		st=noir_vcpu_already_created;
		noir_acquire_reslock_exclusive(virtual_machine->header.vcpu_list_lock);
		if(virtual_machine->vcpu[vcpu_id]==null)
		{
			noir_vt_custom_vcpu_p vcpu=noir_alloc_nonpg_memory(sizeof(noir_vt_custom_vcpu));
			if(vcpu)
			{
				*virtual_processor=vcpu;
				vcpu->vmcs.virt=noir_alloc_contd_memory(page_size);
				if(vcpu->vmcs.virt)
					vcpu->vmcs.phys=noir_get_physical_address(vcpu->vmcs.virt);
				else
					goto alloc_failure;
				vcpu->msr_auto.virt=noir_alloc_contd_memory(page_size);
				if(vcpu->msr_auto.virt)
					vcpu->msr_auto.phys=noir_get_physical_address(vcpu->msr_auto.virt);
				else
					goto alloc_failure;
				// Allocate XSAVE State Area
				vcpu->header.xsave_area=noir_alloc_contd_memory(hvm_p->xfeat.supported_size_max);
				if(vcpu->header.xsave_area==null)goto alloc_failure;
				// Set the parent VM.
				vcpu->vm=virtual_machine;
				virtual_machine->vcpu[vcpu_id]=vcpu;
				// vCPU basic info
				vcpu->vcpu_id=vcpu_id;
				vcpu->proc_id=0xffffffff;
				// Initialize VMCS via a hypercall.
				noir_vt_vmcall(noir_vt_init_custom_vmcs,(ulong_ptr)vcpu);
				st=noir_success;
			}
		}
		noir_release_reslock(virtual_machine->header.vcpu_list_lock);
	}
	return st;
alloc_failure:
	nvc_vtc_release_vcpu(*virtual_processor);
	noir_release_reslock(virtual_machine->header.vcpu_list_lock);
	return noir_insufficient_resources;
}

void nvc_vtc_release_vm(noir_vt_custom_vm_p virtual_machine)
{
	if(virtual_machine)
	{
		if(virtual_machine->msr_bitmap.virt)
			noir_free_contd_memory(virtual_machine->msr_bitmap.virt,page_size);
		// Release vCPU List...
		noir_acquire_reslock_exclusive(virtual_machine->header.vcpu_list_lock);
		if(virtual_machine->vcpu)
		{
			// Traverse vCPU List and free them...
			for(u32 i=0;i<255;i++)
				if(virtual_machine->vcpu[i])
					nvc_vtc_release_vcpu(virtual_machine->vcpu[i]);
			noir_free_nonpg_memory(virtual_machine->vcpu);
		}
		noir_release_reslock(virtual_machine->header.vcpu_list_lock);
		// Release Extended Paging Structure...
		if(virtual_machine->eptm.eptp.virt)
			noir_free_contd_memory(virtual_machine->eptm.eptp.virt,page_size);
		if(virtual_machine->eptm.pdpte.head)
		{
			noir_ept_pdpte_descriptor_p cur=virtual_machine->eptm.pdpte.head;
			while(cur)
			{
				noir_ept_pdpte_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		if(virtual_machine->eptm.pde.head)
		{
			noir_ept_pde_descriptor_p cur=virtual_machine->eptm.pde.head;
			while(cur)
			{
				noir_ept_pde_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		if(virtual_machine->eptm.pte.head)
		{
			noir_ept_pte_descriptor_p cur=virtual_machine->eptm.pte.head;
			while(cur)
			{
				noir_ept_pte_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		// Free VPID.
		noir_acquire_reslock_exclusive(hvm_p->tlb_tagging.vpid_pool_lock);
		noir_reset_bitmap(hvm_p->tlb_tagging.vpid_pool,virtual_machine->vpid-hvm_p->tlb_tagging.start);
		noir_release_reslock(hvm_p->tlb_tagging.vpid_pool_lock);
		// Free VM Structure.
		noir_free_nonpg_memory(virtual_machine);
	}
}

noir_status nvc_vtc_create_vm(noir_vt_custom_vm_p *virtual_machine)
{
	noir_status st=noir_invalid_parameter;
	if(virtual_machine)
	{
		noir_vt_custom_vm_p vm=noir_alloc_nonpg_memory(sizeof(noir_vt_custom_vm));
		st=noir_insufficient_resources;
		if(vm)
		{
			*virtual_machine=vm;
			// Allocate MSR Bitmap
			vm->msr_bitmap.virt=noir_alloc_contd_memory(page_size);
			if(vm->msr_bitmap.virt)
				vm->msr_bitmap.phys=noir_get_physical_address(vm->msr_bitmap.virt);
			else
				goto alloc_failure;
			// Allocate vCPU List
			vm->vcpu=noir_alloc_nonpg_memory(page_size);
			if(vm->vcpu==null)goto alloc_failure;
			// Initialize EPT Manager (with Top-Level Paging Structure)
			vm->eptm.eptp.virt=noir_alloc_contd_memory(page_size);
			if(vm->eptm.eptp.virt)
			{
				// Make EPTP Pointer
				ia32_ept_pointer eptp;
				eptp.value=noir_get_physical_address(vm->eptm.eptp.virt);
				eptp.memory_type=ia32_write_back;
				eptp.walk_length=3;
				vm->eptm.eptp.phys=eptp.value;
			}
			else
				goto alloc_failure;
			// Allocate VPID
			vm->vpid=nvc_vtc_alloc_vpid();
			if(vm->vpid==0xffffffff)goto alloc_failure;
			// Setup MSR Bitmap.
			noir_stosb(vm->msr_bitmap.virt,0xff,page_size);
			// Some MSRs are unnecessary to be intercepted. Rule them out from the bitmap.
			st=noir_success;
		}
	}
	return st;
alloc_failure:
	nvc_vtc_release_vm(*virtual_machine);
	*virtual_machine=null;
	return noir_insufficient_resources;
}

void nvc_vtc_finalize_cvm_module()
{
	if(noir_vm_list_lock)
		noir_finalize_reslock(noir_vm_list_lock);
}

noir_status nvc_vtc_initialize_cvm_module()
{
	noir_status st=noir_insufficient_resources;
	noir_vm_list_lock=noir_initialize_reslock();
	if(noir_vm_list_lock)
	{
		st=noir_success;
		hvm_p->idle_vm=&noir_idle_vm;
		noir_initialize_list_entry(&noir_idle_vm.active_vm_list);
	}
	return st;
}
#endif