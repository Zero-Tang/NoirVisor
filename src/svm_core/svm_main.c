/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the basic driver of AMD-V.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_main.c
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

void nvc_query_svm_capability()
{
	noir_svm_hvm_p rhvm=hvm_p->relative_hvm;
	noir_cpuid(amd64_cpuid_ext_svm_features,0,null,&rhvm->virt_cap.asid_limit,null,&rhvm->virt_cap.capabilities);
	noir_cpuid(amd64_cpuid_ext_mem_crypting,0,&rhvm->sev_cap.capabilities,&rhvm->sev_cap.mem_virt_cap,&rhvm->sev_cap.simultaneous,&rhvm->sev_cap.minimum_asid);
}

bool nvc_is_svm_supported()
{
	u32 a,b,c,d;
	noir_cpuid(amd64_cpuid_ext_proc_feature,0,null,null,&c,null);
	if(noir_bt(&c,amd64_cpuid_svm))
	{
		bool basic_supported=true;
		noir_cpuid(amd64_cpuid_ext_svm_features,0,&a,&b,&c,&d);
		// At least one ASID should be available.
		basic_supported&=(b>0);
		// Decode Assists is the required feature.
		basic_supported&=noir_bt(&d,amd64_cpuid_decoder);
		// Next RIP Saving is the required feature.
		basic_supported&=noir_bt(&d,amd64_cpuid_nrips);
		return basic_supported;
	}
	return false;
}

bool nvc_is_npt_supported()
{
	u32 a,b,c,d;
	bool npt_support=true;
	noir_cpuid(amd64_cpuid_ext_svm_features,0,&a,&b,&c,&d);
	npt_support&=noir_bt(&d,amd64_cpuid_npt);
	npt_support&=noir_bt(&d,amd64_cpuid_vmcb_clean);
	return npt_support;
}

bool nvc_is_acnested_svm_supported()
{
	u32 a,b,c,d;
	bool acnv_support=true;
	noir_cpuid(amd64_cpuid_ext_svm_features,0,&a,&b,&c,&d);
	acnv_support&=noir_bt(&d,amd64_cpuid_vmlsvirt);
	acnv_support&=noir_bt(&d,amd64_cpuid_vgif);
	return acnv_support;
}

u32 nvc_svm_get_avail_asid()
{
	u32 count;
	noir_cpuid(amd64_cpuid_ext_svm_features,0,null,&count,null,null);
	return count;
}

bool nvc_is_svm_disabled()
{
	u64 vmcr=noir_rdmsr(amd64_vmcr);
	return noir_bt(&vmcr,amd64_vmcr_svmdis);
}

u8 nvc_svm_enable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	efer|=amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
	efer=noir_rdmsr(amd64_efer);
	if(noir_bt(&efer,amd64_efer_svme)==true)
	{
		u64 vmcr=noir_rdmsr(amd64_vmcr);
		// Block and Disable A20M
		// Not sure about the reason, but
		// Intel blocks A20M in vmxon.
		noir_bts(&vmcr,amd64_vmcr_disa20m);
		// Redirect INIT Signal in that AMD-V's
		// INIT Interception doesn't make sense.
		// Intel VT-x would block INIT Signal when
		// the processor is in VMX Root Operation.
		noir_bts(&vmcr,amd64_vmcr_r_init);
		noir_wrmsr(amd64_vmcr,vmcr);
		return noir_virt_trans;
	}
	return noir_virt_off;
}

u8 nvc_svm_disable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	// Disable SVM
	efer&=~amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
	efer=noir_rdmsr(amd64_efer);
	if(noir_bt(&efer,amd64_efer_svme)==false)
	{
		u64 vmcr=noir_rdmsr(amd64_vmcr);
		// Unblock and Enable A20M
		// Not sure about the reason, but
		// Intel unblocks A20M in vmxoff.
		noir_btr(&vmcr,amd64_vmcr_disa20m);
		// Stop redirecting INIT Signals.
		noir_bts(&vmcr,amd64_vmcr_r_init);
		noir_wrmsr(amd64_vmcr,vmcr);
		return noir_virt_off;
	}
	return noir_virt_trans;
}

void static nvc_svm_setup_msr_hook(noir_hypervisor_p hvm_p)
{
	void* bitmap1=(void*)((ulong_ptr)hvm_p->relative_hvm->msrpm.virt+0);
	void* bitmap2=(void*)((ulong_ptr)hvm_p->relative_hvm->msrpm.virt+0x800);
	void* bitmap3=(void*)((ulong_ptr)hvm_p->relative_hvm->msrpm.virt+0x1000);
	// Setup basic interceptions to MSRs that may interfere with SVM normal operations.
	// This is also for nested virtualization.
	noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_efer,0));
	noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_efer,1));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_vmcr,0));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_vmcr,1));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_hsave_pa,0));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_hsave_pa,1));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_svm_key,0));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_svm_key,1));
	if(hvm_p->options.stealth_msr_hook)
	{
	// Setup custom MSR-Interception if enabled.
#if defined(_amd64)
		unref_var(bitmap1);
		noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_lstar,0));			// Hide MSR Hook
		noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_lstar,1));			// Mask MSR Hook
#else
		noir_set_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_eip,0));		// Hide MSR Hook
		noir_set_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_eip,1));		// Mask MSR Hook
#endif
	}
}

void static nvc_svm_setup_virtual_msr(noir_svm_vcpu_p vcpu)
{
	noir_svm_virtual_msr_p vmsr=&vcpu->virtual_msr;
#if defined(_amd64)
	vmsr->lstar=(u64)orig_system_call;
#else
	vmsr->sysenter_eip=(u64)orig_system_call;
#endif
}

void static nvc_svm_setup_control_area(noir_svm_vcpu_p vcpu)
{
	nvc_svm_cra_intercept crx_intercept;
	nvc_svm_instruction_intercept1 list1;
	nvc_svm_instruction_intercept2 list2;
	u32 exception_bitmap=0;
	u32 listex=0;
	u32 a,b,c,d;
	noir_cpuid(amd64_cpuid_ext_svm_features,0,&a,&b,&c,&d);
	// Basic features...
	crx_intercept.value=0;
	list1.value=0;
	list1.intercept_cpuid=1;
	list1.intercept_invlpga=1;
	list1.intercept_msr=1;
	list1.intercept_shutdown=1;
	list2.value=0;
	list2.intercept_vmrun=1;		// The vmrun should always be intercepted as required by AMD.
	list2.intercept_vmmcall=1;
	// SVM instructions must be intercepted when EFER.SVME is cleared.
	list2.intercept_vmload=1;
	list2.intercept_vmsave=1;
	list2.intercept_stgi=1;
	list2.intercept_clgi=1;
	list2.intercept_skinit=1;
	// Extended Feature: NPIEP
	if(hvm_p->options.cpuid_hv_presence)
	{
		u32 c7;
		// If NPIEP is enabled, it is necessary to observe the supportability of UMIP.
		noir_cpuid(amd64_cpuid_std_struct_extid,0,null,null,&c7,null);
		// Bit 2 of ECX indicates UMIP support. Intercept writes to CR4 to optimize NPIEP.
		if(noir_bt(&c7,2))crx_intercept.write.cr4=1;
		// If there is no UMIP support, don't intercept writes to CR4 in that no optimizations can be made.
	}
	noir_bts(&listex,amd64_security_exception);			// Intercept Security Exceptions...
	// Policy: Enable as most features as possible.
	// Save them to vCPU.
	if(d & amd64_cpuid_vmcb_clean_bit)
		vcpu->enabled_feature|=noir_svm_vmcb_caching;
	if(d & amd64_cpuid_npt_bit)
		vcpu->enabled_feature|=noir_svm_nested_paging;
	if(d & amd64_cpuid_flush_asid_bit)
		vcpu->enabled_feature|=noir_svm_flush_by_asid;
	if(d & amd64_cpuid_vgif_bit)
		vcpu->enabled_feature|=noir_svm_virtual_gif;
	if(d & amd64_cpuid_vmlsvirt_bit)
		vcpu->enabled_feature|=noir_svm_virtualized_vmls;
	// Setup Memory Virtualization
	if(vcpu->enabled_feature & noir_svm_nested_paging)
	{
		// Enable NPT
		noir_npt_manager_p nptm=(noir_npt_manager_p)vcpu->primary_nptm;
		nvc_svm_npt_control npt_ctrl;
		npt_ctrl.value=0;
		npt_ctrl.enable_npt=1;
		noir_svm_vmwrite64(vcpu->vmcb.virt,npt_control,npt_ctrl.value);
#if defined(_hv_type1)
		// Update the NPT by APIC
		nvc_npt_setup_apic_shadowing(vcpu);
#endif
		// Write NPT CR3
		noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
	}
	// Solution of MSR-Hook with KVA-shadow mechanism: Intercept #PF exceptions.
	if((vcpu->enabled_feature & noir_svm_syscall_hook) && (vcpu->enabled_feature & noir_svm_kva_shadow_present))
		noir_bts(&exception_bitmap,amd64_page_fault);
	// Write to VMCB.
	noir_svm_vmwrite32(vcpu->vmcb.virt,intercept_access_cr,crx_intercept.value);
	noir_svm_vmwrite32(vcpu->vmcb.virt,intercept_exceptions,exception_bitmap);
	noir_svm_vmwrite32(vcpu->vmcb.virt,intercept_instruction1,list1.value);
	noir_svm_vmwrite16(vcpu->vmcb.virt,intercept_instruction2,list2.value);
}

void nvc_svm_setup_host_idt(ulong_ptr idtr_base)
{
	noir_gate_descriptor_p idt_base=(noir_gate_descriptor_p)idtr_base;
	// Current implementation of NoirVisor only has to take care of NMI.
	idt_base[amd64_nmi_interrupt].offset_lo=(u16)((u64)nvc_svm_host_nmi_handler&0xFFFF);
	idt_base[amd64_nmi_interrupt].offset_mid=(u16)((u64)nvc_svm_host_nmi_handler>>16);
	idt_base[amd64_nmi_interrupt].offset_hi=(u32)((u64)nvc_svm_host_nmi_handler>>32);
#if defined(_hv_type1)
	// If NoirVisor is to be loaded as Type-I hypervisor, exception handlers must be prepared for debugging purpose.
#endif
}

void nvc_svm_load_host_processor_state(noir_svm_vcpu_p vcpu)
{
	noir_processor_state state;
	descriptor_register idtr,gdtr;
	noir_get_prebuilt_host_processor_state(&state);
	// Use vmsave instruction to save effort.
	noir_svm_vmsave((ulong_ptr)vcpu->hvmcb.phys);
	// Load IDTR and GDTR. They are not indicated in VMCB during vmsave.
	idtr.limit=(u16)state.idtr.limit;
	gdtr.limit=(u16)state.gdtr.limit;
	idtr.base=state.idtr.base;
	gdtr.base=state.gdtr.base;
	// IDT requires some modification.
	nvc_svm_setup_host_idt(idtr.base);
	// Load them into processor.
	noir_lidt(&idtr);
	noir_lgdt(&gdtr);
	// Load GS Segment.
	noir_svm_vmwrite16(vcpu->hvmcb.virt,guest_gs_selector,state.gs.selector);
	noir_svm_vmwrite16(vcpu->hvmcb.virt,guest_gs_attrib,svm_attrib(state.gs.attrib));
	noir_svm_vmwrite32(vcpu->hvmcb.virt,guest_gs_limit,state.gs.limit);
	noir_svm_vmwrite(vcpu->hvmcb.virt,guest_gs_base,state.gs.base);
	// Load Task State Segment.
	noir_svm_vmwrite16(vcpu->hvmcb.virt,guest_tr_selector,state.tr.selector);
	noir_svm_vmwrite16(vcpu->hvmcb.virt,guest_tr_attrib,svm_attrib(state.tr.attrib));
	noir_svm_vmwrite32(vcpu->hvmcb.virt,guest_tr_limit,state.tr.limit);
	noir_svm_vmwrite(vcpu->hvmcb.virt,guest_tr_base,state.tr.base);
	// Load Host Control Registers.
	noir_writecr3(state.cr3);
	noir_writecr4(state.cr4|amd64_cr4_osfxsr_bit|amd64_cr4_osxsave_bit);
}

// This function has context of cleared GIF.
ulong_ptr nvc_svm_subvert_processor_i(noir_svm_vcpu_p vcpu,ulong_ptr gsp)
{
	// Save Processor State
	noir_processor_state state;
	noir_save_processor_state(&state);
	// Setup Host State
	nvc_svm_load_host_processor_state(vcpu);
	// Setup State-Save Area
	// Save Segment State - CS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_cs_selector,state.cs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_cs_attrib,svm_attrib(state.cs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_cs_limit,state.cs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cs_base,state.cs.base);
	// Save Segment State - DS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ds_selector,state.ds.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ds_attrib,svm_attrib(state.ds.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ds_limit,state.ds.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ds_base,state.ds.base);
	// Save Segment State - ES
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_es_selector,state.es.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_es_attrib,svm_attrib(state.es.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_es_limit,state.es.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_es_base,state.es.base);
	// Save Segment State - FS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_fs_selector,state.fs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_fs_attrib,svm_attrib(state.fs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_fs_limit,state.fs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_fs_base,state.fs.base);
	// Save Segment State - GS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_gs_selector,state.gs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_gs_attrib,svm_attrib(state.gs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_gs_limit,state.gs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_gs_base,state.gs.base);
	// Save Segment State - SS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ss_selector,state.ss.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ss_attrib,svm_attrib(state.ss.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ss_limit,state.ss.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ss_base,state.ss.base);
	// Save Segment State - TR
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_tr_selector,state.tr.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_tr_attrib,svm_attrib(state.tr.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_tr_limit,state.tr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_tr_base,state.tr.base);
	// Save GDTR and IDTR
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_gdtr_limit,state.gdtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_gdtr_base,state.gdtr.base);
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_idtr_limit,state.idtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_idtr_base,state.idtr.base);
	// Save Segment State - LDTR
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ldtr_selector,state.ldtr.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ldtr_attrib,svm_attrib(state.ldtr.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ldtr_limit,state.ldtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ldtr_base,state.ldtr.base);
	// Save Control Registers
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr0,state.cr0);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr2,state.cr2);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr3,state.cr3);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr4,state.cr4);
#if defined(_amd64)
	// Save Task Priority Register (CR8)
	noir_svm_vmwrite8(vcpu->vmcb.virt,avic_control,(u8)state.cr8&0xf);
#endif
	// Save Debug Registers
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_dr6,state.dr6);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_dr7,state.dr7);
	// Save RFlags, RSP and RIP
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rflags,2);	// Fixed bit should be set.
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rsp,gsp);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rip,(ulong_ptr)nvc_svm_guest_start);
	// Save Processor Hidden State
	noir_svm_vmsave((ulong_ptr)vcpu->vmcb.phys);
	// Save Model Specific Registers.
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_pat,state.pat);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_efer,state.efer);
#if defined(_amd64)
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_star,state.star);
#if !defined(_hv_type1)
	if(vcpu->enabled_feature & noir_svm_syscall_hook)
		noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,(u64)noir_system_call);
	else
#endif
		noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,state.lstar);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_cstar,state.cstar);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_sfmask,state.sfmask);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_kernel_gs_base,state.gsswap);
#else
	if(vcpu->enabled_feature & noir_svm_syscall_hook)
		noir_svm_vmwrite32(vcpu->vmcb.virt,guest_sysenter_eip,(u32)noir_system_call);
	else
		noir_svm_vmwrite32(vcpu->vmcb.virt,guest_sysenter_eip,state.sysenter_eip);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_sysenter_cs,state.sysenter_cs);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_sysenter_esp,state.sysenter_esp);
#endif
	// Setup Control Area
	nvc_svm_setup_control_area(vcpu);
	// Setup IOPM and MSRPM.
	noir_svm_vmwrite64(vcpu->vmcb.virt,iopm_physical_address,vcpu->relative_hvm->iopm.phys);
	noir_svm_vmwrite64(vcpu->vmcb.virt,msrpm_physical_address,vcpu->relative_hvm->msrpm.phys);
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_asid,1);		// ASID must be non-zero.
	// We will assign a guest asid other than 1 as we are nesting a hypervisor.
	// Load Partial Guest State by vmload and continue subversion.
	noir_svm_vmload((ulong_ptr)vcpu->vmcb.phys);
	// "Return" puts the value onto rax register.
	return (ulong_ptr)vcpu->vmcb.phys;
}

void static nvc_svm_subvert_processor(noir_svm_vcpu_p vcpu)
{
	vcpu->status=nvc_svm_enable();
	if(vcpu->status==noir_virt_trans)
	{
		ulong_ptr estimated_stack=(ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_svm_initial_stack);
		noir_svm_initial_stack_p stack=(noir_svm_initial_stack_p)(estimated_stack&0xFFFFFFFFFFFFFFF0);
		// Initialize Hypervisor Context Stack.
		stack->guest_vmcb_pa=vcpu->vmcb.phys;
		stack->host_vmcb_pa=vcpu->hvmcb.phys;
		stack->proc_id=vcpu->proc_id;
		stack->vcpu=vcpu;
		stack->custom_vcpu=&nvc_svm_idle_cvcpu;
		noir_wrmsr(amd64_hsave_pa,vcpu->hsave.phys);
		nvc_svm_setup_virtual_msr(vcpu);
		vcpu->mshvcpu.root_vcpu=(void*)vcpu;
		// Cache the Family-Model-Stepping Information for INIT Signal Emulation.
		noir_cpuid(amd64_cpuid_std_proc_feature,0,&vcpu->cpuid_fms,null,null,null);
		vcpu->status=nvc_svm_subvert_processor_a(stack);
		nv_dprintf("Processor %u Subversion Status: %u\n",vcpu->proc_id,vcpu->status);
	}
}

void static nvc_svm_subvert_processor_thunk(void* context,u32 processor_id)
{
	noir_svm_vcpu_p vcpu=(noir_svm_vcpu_p)context;
	vcpu[processor_id].proc_id=processor_id;
	nvc_svm_subvert_processor(&vcpu[processor_id]);
}

void nvc_svm_cleanup(noir_hypervisor_p hvm_p)
{
	if(hvm_p->virtual_cpu)
	{
		for(u32 i=0;i<hvm_p->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm_p->virtual_cpu[i];
			if(vcpu->vmcb.virt)
				noir_free_contd_memory(vcpu->vmcb.virt,page_size);
			if(vcpu->hsave.virt)
				noir_free_contd_memory(vcpu->hsave.virt,page_size);
			if(vcpu->hvmcb.virt)
				noir_free_contd_memory(vcpu->hvmcb.virt,page_size);
#if defined(_hv_type1)
			if(vcpu->sapic.virt)
				noir_free_contd_memory(vcpu->sapic.virt,page_size);
#endif
			if(vcpu->hv_stack)
				noir_free_nonpg_memory(vcpu->hv_stack);
			if(vcpu->cvm_state.xsave_area)
				noir_free_contd_memory(vcpu->cvm_state.xsave_area,page_size);
			if(vcpu->primary_nptm)
				nvc_npt_cleanup(vcpu->primary_nptm);
#if !defined(_hv_type1)
			if(vcpu->secondary_nptm)
				nvc_npt_cleanup(vcpu->secondary_nptm);
#endif
			for(u32 j=0;j<noir_svm_cached_nested_vmcb;j++)
				if(vcpu->nested_hvm.nested_vmcb[j].vmcb_t.virt)
					noir_free_contd_memory(vcpu->nested_hvm.nested_vmcb[j].vmcb_t.virt,page_size);
		}
		noir_free_nonpg_memory(hvm_p->virtual_cpu);
	}
	if(hvm_p->relative_hvm->msrpm.virt)
		noir_free_contd_memory(hvm_p->relative_hvm->msrpm.virt,page_size*2);
	if(hvm_p->relative_hvm->iopm.virt)
		noir_free_contd_memory(hvm_p->relative_hvm->iopm.virt,page_size*3);
	if(hvm_p->relative_hvm->blank_page.virt)
		noir_free_contd_memory(hvm_p->relative_hvm->blank_page.virt,page_size);
#if !defined(_hv_type1)
	if(hvm_p->tlb_tagging.asid_pool_lock)
		noir_finalize_reslock(hvm_p->tlb_tagging.asid_pool_lock);
	if(hvm_p->tlb_tagging.asid_pool)
		noir_free_nonpg_memory(hvm_p->tlb_tagging.asid_pool);
#endif
}

noir_status nvc_svm_subvert_system(noir_hypervisor_p hvm_p)
{
	hvm_p->cpu_count=noir_get_processor_count();
	hvm_p->relative_hvm=(noir_svm_hvm_p)hvm_p->reserved;
	// Query available virtualization capabilities.
	nvc_query_svm_capability();
	// Query Extended State Enumeration - Useful for xsetbv handler, CVM scheduler, etc.
	noir_cpuid(amd64_cpuid_std_pestate_enum,0,&hvm_p->xfeat.support_mask.low,&hvm_p->xfeat.enabled_size_max,&hvm_p->xfeat.supported_size_max,&hvm_p->xfeat.support_mask.high);
	noir_cpuid(amd64_cpuid_std_pestate_enum,1,&hvm_p->xfeat.supported_instructions,null,&hvm_p->xfeat.supported_xss_bits,null);
	// Initialize vCPUs.
	hvm_p->virtual_cpu=noir_alloc_nonpg_memory(hvm_p->cpu_count*sizeof(noir_svm_vcpu));
	// Implementation of Generic Call might differ.
	// In subversion routine, it might not be allowed to allocate memory.
	// Thus allocate everything at this moment, even if it costs more on single processor core.
	if(hvm_p->virtual_cpu)
	{
		for(u32 i=0;i<hvm_p->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm_p->virtual_cpu[i];
			vcpu->vmcb.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->vmcb.virt)
				vcpu->vmcb.phys=noir_get_physical_address(vcpu->vmcb.virt);
			else
				goto alloc_failure;
			vcpu->hsave.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->hsave.virt)
				vcpu->hsave.phys=noir_get_physical_address(vcpu->hsave.virt);
			else
				goto alloc_failure;
			vcpu->hvmcb.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->hvmcb.virt)
				vcpu->hvmcb.phys=noir_get_physical_address(vcpu->hvmcb.virt);
			else
				goto alloc_failure;
			vcpu->hv_stack=noir_alloc_nonpg_memory(nvc_stack_size);
			if(vcpu->hv_stack==null)goto alloc_failure;
			vcpu->cvm_state.xsave_area=noir_alloc_contd_memory(hvm_p->xfeat.supported_size_max);
			if(vcpu->cvm_state.xsave_area==null)goto alloc_failure;
			vcpu->relative_hvm=(noir_svm_hvm_p)hvm_p->reserved;
			vcpu->primary_nptm=nvc_npt_build_identity_map();
			if(vcpu->primary_nptm==null)goto alloc_failure;
#if !defined(_hv_type1)
			// Only Type-II Hypervisor would hook into guest.
			vcpu->secondary_nptm=nvc_npt_build_identity_map();
			if(vcpu->secondary_nptm==null)goto alloc_failure;
			if(hvm_p->options.stealth_inline_hook)
				nvc_npt_build_hook_mapping(vcpu);		// This feature does not have a good performance.
#else
			// Type-I Hypervisor must set up shadowed APIC page.
			vcpu->sapic.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->sapic.virt)
				vcpu->sapic.phys=noir_get_physical_address(vcpu->sapic.virt);
			else
				goto alloc_failure;
			if(nvc_npt_build_apic_shadowing(vcpu)==false)
				goto alloc_failure;
#endif
			if(hvm_p->options.nested_virtualization)
			{
				// Setup Nested Hypervisor
				for(u32 j=0;j<noir_svm_cached_nested_vmcb;j++)
				{
					vcpu->nested_hvm.nested_vmcb[j].vmcb_t.virt=noir_alloc_contd_memory(page_size);
					if(vcpu->nested_hvm.nested_vmcb[j].vmcb_t.virt==null)goto alloc_failure;
					vcpu->nested_hvm.nested_vmcb[j].vmcb_t.phys=noir_get_physical_address(vcpu->nested_hvm.nested_vmcb[j].vmcb_t.virt);
					vcpu->nested_hvm.nested_vmcb[j].vmcb_c.phys=0xffffffffffffffff;		// Use -1 to indicate unused VMCB.
				}
			}
			if(nvc_npt_initialize_ci(vcpu->primary_nptm)==false)goto alloc_failure;
#if !defined(_hv_type1)
			if(hvm_p->options.stealth_msr_hook)vcpu->enabled_feature|=noir_svm_syscall_hook;
			if(hvm_p->options.stealth_inline_hook)vcpu->enabled_feature|=noir_svm_npt_with_hooks;
			if(hvm_p->options.kva_shadow_presence)
			{
				vcpu->enabled_feature|=noir_svm_kva_shadow_present;
				nv_dprintf("Warning: KVA-Shadow is present! Stealth MSR-Hook on AMD Processors is untested in regards of KVA-Shadow!\n");
			}
#endif
		}
	}
	hvm_p->host_pat.value=noir_rdmsr(amd64_pat);
	hvm_p->relative_hvm->hvm_cpuid_leaf_max=nvc_mshv_build_cpuid_handlers();
	if(hvm_p->relative_hvm->hvm_cpuid_leaf_max==0)goto alloc_failure;
	hvm_p->relative_hvm->msrpm.virt=noir_alloc_contd_memory(2*page_size);
	if(hvm_p->relative_hvm->msrpm.virt)
		hvm_p->relative_hvm->msrpm.phys=noir_get_physical_address(hvm_p->relative_hvm->msrpm.virt);
	else
		goto alloc_failure;
	hvm_p->relative_hvm->iopm.virt=noir_alloc_contd_memory(3*page_size);
	if(hvm_p->relative_hvm->iopm.virt)
		hvm_p->relative_hvm->iopm.phys=noir_get_physical_address(hvm_p->relative_hvm->iopm.virt);
	else
		goto alloc_failure;
#if !defined(_hv_type1)
	// Initialize CVM Module.
	if(nvc_svmc_initialize_cvm_module()!=noir_success)goto alloc_failure;
	// If nested virtualization is disabled, reserved all available ASIDs to CVMs.
	// Otherwise, reserve half of ASIDs to CVMs.
	if(hvm_p->options.nested_virtualization)
	{
		u32 bitmap_size;
		hvm_p->tlb_tagging.start=hvm_p->relative_hvm->virt_cap.asid_limit>>1;
		hvm_p->tlb_tagging.limit=hvm_p->relative_hvm->virt_cap.asid_limit-hvm_p->tlb_tagging.start;
		bitmap_size=hvm_p->tlb_tagging.limit>>3;
		if(hvm_p->tlb_tagging.limit & 7)bitmap_size++;
		hvm_p->tlb_tagging.asid_pool=noir_alloc_nonpg_memory(bitmap_size);
	}
	else
	{
		u32 bitmap_size;
		hvm_p->tlb_tagging.start=2;
		hvm_p->tlb_tagging.limit=hvm_p->relative_hvm->virt_cap.asid_limit-2;
		bitmap_size=hvm_p->tlb_tagging.limit>>3;
		if(hvm_p->tlb_tagging.limit & 7)bitmap_size++;
		hvm_p->tlb_tagging.asid_pool=noir_alloc_nonpg_memory(bitmap_size);
	}
	nv_dprintf("Number of ASIDs reserved for Customizable VMs: %u\n",hvm_p->tlb_tagging.limit);
	if(hvm_p->tlb_tagging.asid_pool==null)goto alloc_failure;
	hvm_p->tlb_tagging.asid_pool_lock=noir_initialize_reslock();
	if(hvm_p->tlb_tagging.asid_pool_lock==null)goto alloc_failure;
#endif
	nvc_svm_setup_msr_hook(hvm_p);
	if(nvc_npt_protect_critical_hypervisor(hvm_p)==false)goto alloc_failure;
	if(hvm_p->virtual_cpu==null)goto alloc_failure;
	hvm_p->options.tlfs_passthrough=noir_is_under_hvm();
	if(hvm_p->options.tlfs_passthrough && hvm_p->options.cpuid_hv_presence)
		nv_dprintf("Note: Hypervisor is detected! The cpuid presence will be in pass-through mode!\n");
	nvc_svm_set_mshv_handler(hvm_p->options.tlfs_passthrough?false:hvm_p->options.cpuid_hv_presence);
	nv_dprintf("All allocations are done, start subversion!\n");
	noir_generic_call(nvc_svm_subvert_processor_thunk,hvm_p->virtual_cpu);
	nv_dprintf("Subversion completed!\n");
	return noir_success;
alloc_failure:
	nv_dprintf("Allocation failure!\n");
	noir_int3();
	nvc_svm_cleanup(hvm_p);
	return noir_insufficient_resources;
}

void static nvc_svm_restore_processor(noir_svm_vcpu_p vcpu)
{
	// Leave Guest Mode by vmmcall if we are in Guest Mode.
	if(vcpu->status==noir_virt_on)
		noir_svm_vmmcall(noir_svm_callexit,(ulong_ptr)vcpu);
	// Mark the processor is in "off" status as we are in Host Mode now.
	if(vcpu->status==noir_virt_trans)
		vcpu->status=nvc_svm_disable();
	// Things are problematic if vcpu status is not "off".
}

void static nvc_svm_restore_processor_thunk(void* context,u32 processor_id)
{
	noir_svm_vcpu_p vcpu=(noir_svm_vcpu_p)context;
	nv_dprintf("Processor %d entered restoration routine!\n",processor_id);
	nvc_svm_restore_processor(&vcpu[processor_id]);
}

void nvc_svm_restore_system(noir_hypervisor_p hvm_p)
{
	if(hvm_p->virtual_cpu)
	{
		noir_generic_call(nvc_svm_restore_processor_thunk,hvm_p->virtual_cpu);
		nvc_svm_cleanup(hvm_p);
#if !defined(_hv_type1)
		nvc_svmc_finalize_cvm_module();
#endif
		nvc_mshv_teardown_cpuid_handlers();
	}
}