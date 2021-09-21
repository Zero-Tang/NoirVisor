/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the basic driver of Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_main.c
*/

#include <nvdef.h>
#include <nvstatus.h>
#include <nvbdk.h>
#include <vt_intrin.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include "vt_vmcs.h"
#include "vt_def.h"
#include "vt_ept.h"

bool nvc_is_vt_supported()
{
	u32 c;
	noir_cpuid(1,0,null,null,&c,null);
	if(noir_bt(&c,ia32_cpuid_vmx))
	{
		bool basic_supported=true;
		ia32_vmx_basic_msr vt_basic;
		vt_basic.value=noir_rdmsr(ia32_vmx_basic);
		if(vt_basic.memory_type==ia32_write_back)		// Make sure CPU supports Write-Back VMCS.
		{
			ia32_vmx_priproc_ctrl_msr prictrl_msr;
			if(vt_basic.use_true_msr)
				prictrl_msr.value=noir_rdmsr(ia32_vmx_true_priproc_ctrl);
			else
				prictrl_msr.value=noir_rdmsr(ia32_vmx_priproc_ctrl);
			// Support of MSR-Bitmap is essential.
			basic_supported&=prictrl_msr.allowed1_settings.use_msr_bitmap;
		}
		return basic_supported;
	}
	return false;
}

bool nvc_is_vmcs_shadowing_supported()
{
	ia32_vmx_basic_msr vt_basic;
	ia32_vmx_priproc_ctrl_msr prictrl_msr;
	vt_basic.value=noir_rdmsr(ia32_vmx_basic);
	if(vt_basic.use_true_msr)
		prictrl_msr.value=noir_rdmsr(ia32_vmx_true_priproc_ctrl);
	else
		prictrl_msr.value=noir_rdmsr(ia32_vmx_priproc_ctrl);
	if(prictrl_msr.allowed1_settings.activate_secondary_controls)
	{
		ia32_vmx_2ndproc_ctrl_msr secctrl_msr;
		secctrl_msr.value=noir_rdmsr(ia32_vmx_2ndproc_ctrl);
		return secctrl_msr.allowed1_settings.vmcs_shadowing;
	}
	return false;
}

bool nvc_is_ept_supported()
{
	ia32_vmx_ept_vpid_cap_msr ev_cap;
	ev_cap.value=noir_rdmsr(ia32_vmx_ept_vpid_cap);
	if(ev_cap.support_wb_ept)
	{
		/*
			We require following supportability of Intel EPT:
			Execute-Only Translation - This is used for stealth inline hook.
			2MB-paging - This is used for reducing memory consumption.
			(It can be better that processor supports 1GB-paging. However,
			 VMware does not support emulating 1GB-paging for Intel EPT.)
			Support invept Instruction - This is used for invalidating EPT TLB.
		*/
		bool ept_support_req=true;
		ept_support_req&=ev_cap.support_exec_only_translation;
		ept_support_req&=ev_cap.support_2mb_paging;
		ept_support_req&=ev_cap.support_invept;
		ept_support_req&=ev_cap.support_single_context_invept;
		ept_support_req&=ev_cap.support_all_context_invept;
		return ept_support_req;
	}
	return false;
}

bool nvc_is_vt_enabled()
{
	ia32_feature_control_msr feat_ctrl;
	feat_ctrl.value=noir_rdmsr(ia32_feature_control);
	if(feat_ctrl.lock)return feat_ctrl.vmx_enabled_outside_smx;
	// Albeit we may set to enable VMX here if this MSR is not locked,
	// It seems to be the firmware's job to do so.
	return false;
}

u32 nvc_vt_get_avail_vpid()
{
	// Please note that in Intel VT-x, the VPID is a 16-bit field.
	// Plus, there is no direct MSR reporting this limit.
	// Therefore, let's say it implies a VPID limit of 65536.
	return 0x10000;
}

void static nvc_vt_cleanup(noir_hypervisor_p hvm)
{
	if(hvm)
	{
		noir_vt_hvm_p rhvm=hvm->relative_hvm;
		if(hvm->virtual_cpu)
		{
			u32 i=0;
			for(noir_vt_vcpu_p vcpu=hvm->virtual_cpu;i<hvm->cpu_count;vcpu=&hvm->virtual_cpu[++i])
			{
				if(vcpu->vmxon.virt)
					noir_free_contd_memory(vcpu->vmxon.virt);
				if(vcpu->vmcs.virt)
					noir_free_contd_memory(vcpu->vmcs.virt);
				if(vcpu->nested_vcpu.vmcs_t.virt)
					noir_free_contd_memory(vcpu->nested_vcpu.vmcs_t.virt);
				if(vcpu->hv_stack)
					noir_free_nonpg_memory(vcpu->hv_stack);
				nvc_ept_cleanup(vcpu->ept_manager);
			}
			noir_free_nonpg_memory(hvm->virtual_cpu);
		}
		if(rhvm->msr_bitmap.virt)
			noir_free_contd_memory(rhvm->msr_bitmap.virt);
		if(rhvm->io_bitmap_a.virt)
			noir_free_contd_memory(rhvm->io_bitmap_a.virt);
		if(rhvm->io_bitmap_b.virt)
			noir_free_contd_memory(rhvm->io_bitmap_b.virt);
		if(rhvm->msr_auto_list.virt)
			noir_free_contd_memory(rhvm->msr_auto_list.virt);
	}
	// nvc_vt_teardown_exit_handlers();
}

void static nvc_vt_setup_msr_hook_p(noir_vt_vcpu_p vcpu)
{
	u64 entry_load=vcpu->relative_hvm->msr_auto_list.phys;
	u64 exit_load=vcpu->relative_hvm->msr_auto_list.phys+0x400;
	u64 exit_store=vcpu->relative_hvm->msr_auto_list.phys+0x800;
	noir_vt_vmwrite64(vmentry_msr_load_address,entry_load);
	noir_vt_vmwrite64(vmexit_msr_load_address,exit_load);
	noir_vt_vmwrite64(vmexit_msr_store_address,exit_store);
	noir_vt_vmwrite64(address_of_msr_bitmap,vcpu->relative_hvm->msr_bitmap.phys);
#if !defined(_hv_type1)
	if(vcpu->enabled_feature & noir_vt_syscall_hook)
	{
#if defined(_amd64)
		noir_vt_vmwrite(vmentry_msr_load_count,1);
		noir_vt_vmwrite(vmexit_msr_load_count,1);
#else
		noir_vt_vmwrite(guest_msr_ia32_sysenter_eip,(ulong_ptr)noir_system_call);
		noir_vt_vmwrite(host_msr_ia32_sysenter_eip,orig_system_call);
#endif
	}
#endif
}

void static nvc_vt_setup_msr_auto_list(noir_hypervisor_p hvm)
{
	/*
		For MSR-Auto list, we have following convention:
		MSR-Auto list is a contiguous 4KB memory region.
		1st 1KB is for MSR-load at VM-Entry.
		2nd 1KB is for MSR-load at VM-Exit.
		3rd 1KB is for MSR-store at VM-Exit.
		4th 1KB is reserved and MBZ.
		Note that this is not defined by Intel Architecture.
	*/
	ia32_vmx_msr_auto_p entry_load=(ia32_vmx_msr_auto_p)((ulong_ptr)hvm->relative_hvm->msr_auto_list.virt+0);
	ia32_vmx_msr_auto_p exit_load=(ia32_vmx_msr_auto_p)((ulong_ptr)hvm->relative_hvm->msr_auto_list.virt+0x400);
	ia32_vmx_msr_auto_p exit_store=(ia32_vmx_msr_auto_p)((ulong_ptr)hvm->relative_hvm->msr_auto_list.virt+0x800);
	unref_var(exit_store);
	// Setup custom MSR-Auto list.
#if !defined(_hv_type1)
	if(hvm->options.stealth_msr_hook)
	{
#if defined(_amd64)
		entry_load[0].index=ia32_lstar;
		entry_load[0].data=(ulong_ptr)noir_system_call;
		exit_load[0].index=ia32_lstar;
		exit_load[0].data=orig_system_call;
#endif
	}
#else
	unref_var(entry_load);
	unref_var(exit_load);
#endif
}

void static nvc_vt_setup_msr_hook(noir_hypervisor_p hvm)
{
	void* read_bitmap_low=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0);
	void* read_bitmap_high=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0x400);
	void* write_bitmap_low=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0x800);
	void* write_bitmap_high=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0xC00);
	unref_var(write_bitmap_low);
	unref_var(write_bitmap_high);
	// Setup custom MSR-Interception.
#if !defined(_hv_type1)
	if(hvm->options.stealth_msr_hook)
	{
#if defined(_amd64)
		noir_set_bitmap(read_bitmap_high,ia32_lstar-0xC0000000);	// Hide MSR Hook
		noir_set_bitmap(write_bitmap_high,ia32_lstar-0xC0000000);	// Mask MSR Hook
#else
		noir_set_bitmap(read_bitmap_low,ia32_sysenter_eip);			// Hide MSR Hook
		noir_set_bitmap(write_bitmap_high,ia32_sysenter_eip);		// Mask MSR Hook
#endif
	}
#else
	unref_var(read_bitmap_high);
#endif
	// Setup Microcode-Updater MSR Hook. Microcode update should be intercepted in
	// that if it is not intercepted, processor may result in undefined behavior.
	noir_set_bitmap(write_bitmap_low,ia32_bios_updt_trig);
	// Setup Nested Virtualization MSR Read-Hook.
	noir_set_bitmap(read_bitmap_low,ia32_vmx_basic);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_pinbased_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_priproc_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_exit_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_entry_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_misc);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_cr0_fixed0);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_cr0_fixed1);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_cr4_fixed0);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_cr4_fixed1);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_vmcs_enum);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_2ndproc_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_ept_vpid_cap);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_true_pinbased_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_true_priproc_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_true_exit_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_true_entry_ctrl);
	noir_set_bitmap(read_bitmap_low,ia32_vmx_vmfunc);
	// No need for MSR Write-Hook. Processor automatically fails them.
}

void static nvc_vt_setup_virtual_msr(noir_vt_vcpu_p vcpu)
{
	noir_vt_virtual_msr_p vmsr=&vcpu->virtual_msr;
#if !defined(_hv_type1)
#if defined(_amd64)
	vmsr->lstar=(u64)orig_system_call;
#else
	vmsr->sysenter_eip=(u64)orig_system_call;
#endif
#else
	unref_var(vmsr);
#endif
}

u8 static nvc_vt_enable(u64* vmxon_phys)
{
	u64 cr0=noir_readcr0();
	u64 cr4=noir_readcr4();
	cr0|=noir_rdmsr(ia32_vmx_cr0_fixed0);
	cr0&=noir_rdmsr(ia32_vmx_cr0_fixed1);
	cr4|=noir_rdmsr(ia32_vmx_cr4_fixed0);
	cr4&=noir_rdmsr(ia32_vmx_cr4_fixed1);
	// Note that CR4.VMXE is included in MSR info.
	noir_writecr0(cr0);
	noir_writecr4(cr4);
	return noir_vt_vmxon(vmxon_phys);
}

u8 static nvc_vt_disable()
{
	u64 cr4=noir_readcr4();
	noir_vt_vmxoff();
	noir_btr((u32*)&cr4,ia32_cr4_vmxe);
	noir_writecr4(cr4);		// Only CR4.VMXE is essential to be cleared.
	return noir_virt_off;
}

void static nvc_vt_setup_guest_state_area(noir_processor_state_p state_p,ulong_ptr gsp,ulong_ptr gip)
{
	// Guest State Area - CS Segment
	noir_vt_vmwrite(guest_cs_selector,state_p->cs.selector);
	noir_vt_vmwrite(guest_cs_limit,state_p->cs.limit);
	noir_vt_vmwrite(guest_cs_access_rights,vt_attrib(state_p->cs.selector,state_p->cs.attrib));
	noir_vt_vmwrite(guest_cs_base,(ulong_ptr)state_p->cs.base);
	// Guest State Area - DS Segment
	noir_vt_vmwrite(guest_ds_selector,state_p->ds.selector);
	noir_vt_vmwrite(guest_ds_limit,state_p->ds.limit);
	noir_vt_vmwrite(guest_ds_access_rights,vt_attrib(state_p->ds.selector,state_p->ds.attrib));
	noir_vt_vmwrite(guest_ds_base,(ulong_ptr)state_p->ds.base);
	// Guest State Area - ES Segment
	noir_vt_vmwrite(guest_es_selector,state_p->es.selector);
	noir_vt_vmwrite(guest_es_limit,state_p->es.limit);
	noir_vt_vmwrite(guest_es_access_rights,vt_attrib(state_p->es.selector,state_p->es.attrib));
	noir_vt_vmwrite(guest_es_base,(ulong_ptr)state_p->es.base);
	// Guest State Area - FS Segment
	noir_vt_vmwrite(guest_fs_selector,state_p->fs.selector);
	noir_vt_vmwrite(guest_fs_limit,state_p->fs.limit);
	noir_vt_vmwrite(guest_fs_access_rights,vt_attrib(state_p->fs.selector,state_p->fs.attrib));
	noir_vt_vmwrite(guest_fs_base,(ulong_ptr)state_p->fs.base);
	// Guest State Area - GS Segment
	noir_vt_vmwrite(guest_gs_selector,state_p->gs.selector);
	noir_vt_vmwrite(guest_gs_limit,state_p->gs.limit);
	noir_vt_vmwrite(guest_gs_access_rights,vt_attrib(state_p->gs.selector,state_p->gs.attrib));
	noir_vt_vmwrite(guest_gs_base,(ulong_ptr)state_p->gs.base);
	// Guest State Area - SS Segment
	noir_vt_vmwrite(guest_ss_selector,state_p->ss.selector);
	noir_vt_vmwrite(guest_ss_limit,state_p->ss.limit);
	noir_vt_vmwrite(guest_ss_access_rights,vt_attrib(state_p->ss.selector,state_p->ss.attrib));
	noir_vt_vmwrite(guest_ss_base,(ulong_ptr)state_p->ss.base);
	// Guest State Area - Task Register
	noir_vt_vmwrite(guest_tr_selector,state_p->tr.selector);
	noir_vt_vmwrite(guest_tr_limit,state_p->tr.limit);
	noir_vt_vmwrite(guest_tr_access_rights,vt_attrib(state_p->tr.selector,state_p->tr.attrib));
	noir_vt_vmwrite(guest_tr_base,(ulong_ptr)state_p->tr.base);
	// Guest State Area - Local Descriptor Table Register
	noir_vt_vmwrite(guest_ldtr_selector,state_p->ldtr.selector);
	noir_vt_vmwrite(guest_ldtr_limit,state_p->ldtr.limit);
	noir_vt_vmwrite(guest_ldtr_access_rights,vt_attrib(state_p->ldtr.selector,state_p->ldtr.attrib));
	noir_vt_vmwrite(guest_ldtr_base,(ulong_ptr)state_p->ldtr.base);
	// Guest State Area - IDTR and GDTR
	noir_vt_vmwrite(guest_gdtr_base,(ulong_ptr)state_p->gdtr.base);
	noir_vt_vmwrite(guest_idtr_base,(ulong_ptr)state_p->idtr.base);
	noir_vt_vmwrite(guest_gdtr_limit,state_p->gdtr.limit);
	noir_vt_vmwrite(guest_idtr_limit,state_p->idtr.limit);
	// Guest State Area - Control Registers
	noir_vt_vmwrite(guest_cr0,state_p->cr0);
	noir_vt_vmwrite(guest_cr3,state_p->cr3);
	noir_vt_vmwrite(guest_cr4,state_p->cr4);
	noir_vt_vmwrite(cr0_read_shadow,state_p->cr0);
	noir_vt_vmwrite64(cr4_read_shadow,state_p->cr4 & ~ia32_cr4_vmxe_bit);
	noir_vt_vmwrite(guest_msr_ia32_efer,state_p->efer);
	// Guest State Area - Debug Controls
	noir_vt_vmwrite(guest_dr7,state_p->dr7);
	noir_vt_vmwrite64(guest_msr_ia32_debug_ctrl,state_p->debug_ctrl);
	// VMCS Link Pointer - Essential for Accelerated VMX Nesting
	noir_vt_vmwrite64(vmcs_link_pointer,0xffffffffffffffff);
	// Guest State Area - Flags, Stack Pointer, Instruction Pointer
	noir_vt_vmwrite(guest_rsp,gsp);
	noir_vt_vmwrite(guest_rip,gip);
	noir_vt_vmwrite(guest_rflags,2);		// That the essential bit is set is fine.
}

void static nvc_vt_setup_host_state_area(noir_vt_vcpu_p vcpu,noir_processor_state_p state_p)
{
	// Setup stack for Exit Handler.
	noir_vt_initial_stack_p stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	stack->vcpu=vcpu;
	stack->proc_id=noir_get_current_processor();
	// Host State Area - Segment Selectors
	noir_vt_vmwrite(host_cs_selector,state_p->cs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_ds_selector,state_p->ds.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_es_selector,state_p->es.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_fs_selector,state_p->fs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_gs_selector,state_p->gs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_ss_selector,state_p->ss.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_tr_selector,state_p->tr.selector & selector_rplti_mask);
	// Host State Area - Segment Bases
	noir_vt_vmwrite(host_fs_base,(ulong_ptr)state_p->fs.base);
	noir_vt_vmwrite(host_gs_base,(ulong_ptr)state_p->gs.base);
	noir_vt_vmwrite(host_tr_base,(ulong_ptr)state_p->tr.base);
	// Host State Area - Descriptor Tables
	noir_vt_vmwrite(host_gdtr_base,(ulong_ptr)state_p->gdtr.base);
	noir_vt_vmwrite(host_idtr_base,(ulong_ptr)state_p->idtr.base);
	// Host State Area - Control Registers
	noir_vt_vmwrite(host_cr0,state_p->cr0);
	noir_vt_vmwrite(host_cr3,system_cr3);	// We should use the system page table.
	noir_vt_vmwrite(host_cr4,state_p->cr4);
	noir_vt_vmwrite(host_msr_ia32_efer,state_p->efer);
	// Host State Area - Stack Pointer, Instruction Pointer
	noir_vt_vmwrite(host_rsp,(ulong_ptr)stack);
	noir_vt_vmwrite(host_rip,(ulong_ptr)nvc_vt_exit_handler_a);
}

void static nvc_vt_setup_pinbased_controls(bool true_msr)
{
	ia32_vmx_pinbased_controls pin_ctrl;
	// Read MSR to confirm supported capabilities.
	ia32_vmx_pinbased_ctrl_msr pin_ctrl_msr;
	if(true_msr)
		pin_ctrl_msr.value=noir_rdmsr(ia32_vmx_true_pinbased_ctrl);
	else
		pin_ctrl_msr.value=noir_rdmsr(ia32_vmx_pinbased_ctrl);
	// Setup Pin-Based VM-Execution Controls
	pin_ctrl.value=0;
	// Filter unsupported fields.
	pin_ctrl.value|=pin_ctrl_msr.allowed0_settings.value;
	pin_ctrl.value&=pin_ctrl_msr.allowed1_settings.value;
	// Write to VMCS.
	noir_vt_vmwrite(pin_based_vm_execution_controls,pin_ctrl.value);
}

void static nvc_vt_setup_procbased_controls(bool true_msr)
{
	ia32_vmx_priproc_controls proc_ctrl;
	// Read MSR to confirm supported capabilities.
	ia32_vmx_priproc_ctrl_msr proc_ctrl_msr;
	if(true_msr)
		proc_ctrl_msr.value=noir_rdmsr(ia32_vmx_true_priproc_ctrl);
	else
		proc_ctrl_msr.value=noir_rdmsr(ia32_vmx_priproc_ctrl);
	// Setup Primary Processor-Based VM-Execution Controls
	proc_ctrl.value=0;
	proc_ctrl.use_msr_bitmap=1;		// Essential feature for hiding MSR-Hook.
	proc_ctrl.activate_secondary_controls=1;
	// Filter unsupported fields.
	proc_ctrl.value|=proc_ctrl_msr.allowed0_settings.value;
	proc_ctrl.value&=proc_ctrl_msr.allowed1_settings.value;
	if(!proc_ctrl.use_msr_bitmap)
		nv_dprintf("MSR-Hook Hiding is not supported!\n");
	// Write to VMCS.
	noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl.value);
	// Secondary Processor-Based VM-Execution Controls
	if(proc_ctrl.activate_secondary_controls)
	{
		ia32_vmx_2ndproc_controls proc_ctrl2;
		// Read MSR to confirm supported capabilities.
		ia32_vmx_2ndproc_ctrl_msr proc_ctrl2_msr;
		proc_ctrl2_msr.value=noir_rdmsr(ia32_vmx_2ndproc_ctrl);
		// Setup Secondary Processor-Based VM-Execution Controls
		proc_ctrl2.value=0;
		proc_ctrl2.enable_ept=1;			// Use Intel EPT.
		proc_ctrl2.enable_rdtscp=1;
		proc_ctrl2.enable_vpid=1;			// Avoid unnecessary TLB flushing with VPID
		proc_ctrl2.unrestricted_guest=1;	// Allows the guest to enter unpaged mode.
		proc_ctrl2.enable_invpcid=1;
		proc_ctrl2.enable_xsaves_xrstors=1;
		// Filter unsupported fields.
		proc_ctrl2.value|=proc_ctrl2_msr.allowed0_settings.value;
		proc_ctrl2.value&=proc_ctrl2_msr.allowed1_settings.value;
		// Write to VMCS.
		noir_vt_vmwrite(secondary_processor_based_vm_execution_controls,proc_ctrl2.value);
	}
}

void static nvc_vt_setup_vmexit_controls(bool true_msr)
{
	ia32_vmx_exit_controls exit_ctrl;
	// Read MSR to confirm supported capabilities.
	ia32_vmx_exit_ctrl_msr exit_ctrl_msr;
	if(true_msr)
		exit_ctrl_msr.value=noir_rdmsr(ia32_vmx_true_exit_ctrl);
	else
		exit_ctrl_msr.value=noir_rdmsr(ia32_vmx_exit_ctrl);
	// Setup VM-Exit Controls
	exit_ctrl.value=0;
#if defined(_amd64)
	// This field should be set if NoirVisor is in 64-Bit mode.
	exit_ctrl.host_address_space_size=1;
#endif
	// EFER is to be saved and loaded on exit.
	exit_ctrl.load_ia32_efer=exit_ctrl.save_ia32_efer=1;
	// Filter unsupported fields.
	exit_ctrl.value|=exit_ctrl_msr.allowed0_settings.value;
	exit_ctrl.value&=exit_ctrl_msr.allowed1_settings.value;
	// Write to VMCS.
	noir_vt_vmwrite(vmexit_controls,exit_ctrl.value);
}

void static nvc_vt_setup_vmentry_controls(bool true_msr)
{
	ia32_vmx_entry_controls entry_ctrl;
	// Read MSR to confirm supported capabilities.
	ia32_vmx_entry_ctrl_msr entry_ctrl_msr;
	if(true_msr)
		entry_ctrl_msr.value=noir_rdmsr(ia32_vmx_true_entry_ctrl);
	else
		entry_ctrl_msr.value=noir_rdmsr(ia32_vmx_entry_ctrl);
	// Setup VM-Exit Controls
	entry_ctrl.value=0;
#if defined(_amd64)
	// This field should be set if NoirVisor is in 64-Bit mode.
	entry_ctrl.ia32e_mode_guest=1;
#endif
	entry_ctrl.load_ia32_efer=1;		// EFER is to be loaded on entry.
	// Filter unsupported fields.
	entry_ctrl.value|=entry_ctrl_msr.allowed0_settings.value;
	entry_ctrl.value&=entry_ctrl_msr.allowed1_settings.value;
	// Write to VMCS.
	noir_vt_vmwrite(vmentry_controls,entry_ctrl.value);
}

void static nvc_vt_setup_memory_virtualization(noir_vt_vcpu_p vcpu)
{
	if(vcpu->enabled_feature & noir_vt_vpid_tagged_tlb)
		noir_vt_vmwrite(virtual_processor_identifier,1);
	if(vcpu->enabled_feature & noir_vt_extended_paging)
	{
		noir_ept_manager_p eptm=(noir_ept_manager_p)vcpu->ept_manager;
		noir_vt_vmwrite64(ept_pointer,eptm->eptp.phys.value);
	}
}

void static nvc_vt_setup_available_features(noir_vt_vcpu_p vcpu)
{
	ia32_vmx_2ndproc_ctrl_msr proc2_cap;
	ia32_vmx_ept_vpid_cap_msr ev_cap;
	proc2_cap.value=noir_rdmsr(ia32_vmx_2ndproc_ctrl);
	ev_cap.value=noir_rdmsr(ia32_vmx_ept_vpid_cap);
	// Check if Intel EPT can be enabled.
	if(proc2_cap.allowed1_settings.enable_ept)
	{
		/*
			We require following supportability of Intel EPT:
			Write-Back EPT Paging Structure - We are allocating stuff cached in this way.
			2MB-paging - This is used for reducing memory consumption.
			(It can be better that processor supports 1GB-paging. However,
			 VMware does not support emulating 1GB-paging for Intel EPT.)
			Support invept Instruction - This is used for invalidating EPT TLB.
		*/
		bool ept_support_req=true;
		ept_support_req&=ev_cap.support_wb_ept;
		ept_support_req&=ev_cap.support_2mb_paging;
		ept_support_req&=ev_cap.support_invept;
		ept_support_req&=ev_cap.support_single_context_invept;
		ept_support_req&=ev_cap.support_all_context_invept;
		if(ept_support_req)
		{
			vcpu->enabled_feature|=noir_vt_extended_paging;
#if !defined(_hv_type1)
			// Execute-Only Translation should be supported in order to do stealth inline hook via EPT.
			if(ev_cap.support_exec_only_translation)vcpu->enabled_feature|=noir_vt_ept_with_hooks;
#endif
		}
	}
	// Check if Virtual Processor Identifier can be enabled.
	if(proc2_cap.allowed1_settings.enable_vpid)
	{
		bool vpid_support_req=true;
		vpid_support_req&=ev_cap.support_invvpid;
		vpid_support_req&=ev_cap.support_sc_invvpid;
		vpid_support_req&=ev_cap.support_ac_invvpid;
		if(vpid_support_req)vcpu->enabled_feature|=noir_vt_vpid_tagged_tlb;
	}
	// Check if VMCS Shadowing can be enabled.
	if(proc2_cap.allowed1_settings.vmcs_shadowing)
		vcpu->enabled_feature|=noir_vt_vmcs_shadowing;
}

void static nvc_vt_setup_control_area(bool true_msr)
{
	nvc_vt_setup_pinbased_controls(true_msr);
	nvc_vt_setup_procbased_controls(true_msr);
	nvc_vt_setup_vmexit_controls(true_msr);
	nvc_vt_setup_vmentry_controls(true_msr);
	noir_vt_vmwrite(cr0_guest_host_mask,ia32_cr0_pe|ia32_cr0_ne|ia32_cr0_pg);		// Monitor PE, NE and PG flags
	noir_vt_vmwrite(cr4_guest_host_mask,ia32_cr4_umip|ia32_cr4_vmxe);				// Monitor UMIP and VMXE flags
}

u8 nvc_vt_subvert_processor_i(noir_vt_vcpu_p vcpu,void* reserved,ulong_ptr gsp,ulong_ptr gip)
{
	ia32_vmx_basic_msr vt_basic;
	u8 vst=0;
	noir_processor_state state;
	noir_save_processor_state(&state);
	// Issue a sequence of vmwrite instructions to setup VMCS.
	vt_basic.value=noir_rdmsr(ia32_vmx_basic);
	nvc_vt_setup_available_features(vcpu);
	nvc_vt_setup_control_area(vt_basic.use_true_msr);
	nvc_vt_setup_guest_state_area(&state,gsp,gip);
	nvc_vt_setup_host_state_area(vcpu,&state);
	nvc_vt_setup_memory_virtualization(vcpu);
	nvc_vt_setup_msr_hook_p(vcpu);
	nvc_vt_setup_virtual_msr(vcpu);
	vcpu->status=noir_virt_on;
	// Everything are done, perform subversion.
	vst=noir_vt_vmlaunch();
	nv_dprintf("Error while launching the Guest! VMX Status: %d\n",vst);
	if(vst==vmx_fail_valid)
	{
		u32 err_code;
		noir_vt_vmread(vm_instruction_error,&err_code);
		if(err_code<0x20)
			nv_dprintf("Failed at VM-Entry, Code=%d\t Reason: %s\n",err_code,vt_error_message[err_code]);
		else
			nv_dprintf("VM-Entry failed with invalid code %d!\n",err_code);
		vcpu->status=noir_virt_trans;
	}
	return vst;
}

void static nvc_vt_subvert_processor(noir_vt_vcpu_p vcpu)
{
	u8 vst=nvc_vt_enable(&vcpu->vmxon.phys);
	if(vst==vmx_success)
	{
		vcpu->status=noir_virt_trans;
		vst=noir_vt_vmclear(&vcpu->vmcs.phys);
		if(vst==vmx_success)
		{
			vst=noir_vt_vmptrld(&vcpu->vmcs.phys);
			if(vst==vmx_success)
			{
				// At this time, VMCS has been pointed successfully.
				// Start subverting.
				nv_dprintf("VMCS has been loaded to CPU successfully!\n");
				vst=nvc_vt_subvert_processor_a(vcpu);
				// Indicator other than zero means failure.
				nv_dprintf("Subversion Indicator: %d\n",vst);
				switch(vst)
				{
					case vmx_success:
					{
						nv_dprintf("Subversion is successful!\n");
						break;
					}
					case vmx_fail_valid:
					{
						u32 err_code;
						noir_vt_vmread(vm_instruction_error,&err_code);
						nv_dprintf("vmlaunch failed! %s\n",vt_error_message[err_code]);
						break;
					}
					case vmx_fail_invalid:
					{
						nv_dprintf("vmlaunch failed due to no VMCS is currently loaded!\n");
						break;
					}
					case 3:
					{
						nv_dprintf("vmlaunch failed due to invalid guest state!\n");
						break;
					}
					default:
					{
						nv_dprintf("Invalid Status: %d\n",vst);
						break;
					}
				}
			}
		}
	}
}

void static nvc_vt_subvert_processor_thunk(void* context,u32 processor_id)
{
	noir_vt_vcpu_p vcpu=(noir_vt_vcpu_p)context;
	ia32_vmx_basic_msr vt_basic;
	vt_basic.value=noir_rdmsr(ia32_vmx_basic);
	*(u32*)vcpu[processor_id].vmxon.virt=(u32)vt_basic.revision_id;
	*(u32*)vcpu[processor_id].vmcs.virt=(u32)vt_basic.revision_id;
	*(u32*)vcpu[processor_id].nested_vcpu.vmcs_t.virt=(u32)vt_basic.revision_id;
	nv_dprintf("Processor %d entered subversion routine!\n",processor_id);
	nvc_vt_subvert_processor(&vcpu[processor_id]);
}

/*
  In NoirVisor, allocations of VMXON region and VMCS, etc. are performed in Single CPU.
  It is not Multi-CPU designed in two reasons:
  Performance consumption is not so significant.
  In Windows, this routine is expected to be executed in Passive IRQL. However, in the per-CPU
  routine, it is executed in DPC-Level (KeInsertQueueDpc) or IPI-Level (KeIpiGenericCall), where
  memory allocations are significantly restricted (DPC-Level) or even prohibited (IPI-Level).
*/
noir_status nvc_vt_subvert_system(noir_hypervisor_p hvm)
{
	hvm->cpu_count=noir_get_processor_count();
	hvm->virtual_cpu=noir_alloc_nonpg_memory(hvm->cpu_count*sizeof(noir_vt_vcpu));
	if(hvm->virtual_cpu)
	{
		for(u32 i=0;i<hvm->cpu_count;i++)
		{
			noir_vt_vcpu_p vcpu=&hvm->virtual_cpu[i];
			vcpu->vmcs.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->vmcs.virt)
				vcpu->vmcs.phys=noir_get_physical_address(vcpu->vmcs.virt);
			else
				goto alloc_failure;
			vcpu->vmxon.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->vmxon.virt)
				vcpu->vmxon.phys=noir_get_physical_address(vcpu->vmxon.virt);
			else
				goto alloc_failure;
			vcpu->nested_vcpu.vmcs_t.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->nested_vcpu.vmcs_t.virt)
				vcpu->nested_vcpu.vmcs_t.phys=noir_get_physical_address(vcpu->nested_vcpu.vmcs_t.virt);
			else
				goto alloc_failure;
			vcpu->hv_stack=noir_alloc_nonpg_memory(nvc_stack_size);
			if(vcpu->hv_stack==null)
				goto alloc_failure;
			vcpu->ept_manager=(void*)nvc_ept_build_identity_map();
			if(vcpu->ept_manager==null)
				goto alloc_failure;
			vcpu->relative_hvm=(noir_vt_hvm_p)hvm->reserved;
			vcpu->mshvcpu.root_vcpu=(void*)vcpu;
		}
	}
	hvm->relative_hvm=(noir_vt_hvm_p)hvm->reserved;
	hvm->relative_hvm->msr_bitmap.virt=noir_alloc_contd_memory(page_size);
	if(hvm->relative_hvm->msr_bitmap.virt)
		hvm->relative_hvm->msr_bitmap.phys=noir_get_physical_address(hvm->relative_hvm->msr_bitmap.virt);
	else
		goto alloc_failure;
	// At this time, we don't need to virtualize I/O instructions. So leave them blank.
	/*hvm->relative_hvm->io_bitmap_a.virt=noir_alloc_contd_memory(page_size);
	hvm->relative_hvm->io_bitmap_b.virt=noir_alloc_contd_memory(page_size);
	if(hvm->relative_hvm->io_bitmap_a.virt && hvm->relative_hvm->io_bitmap_b.virt)
	{
		hvm->relative_hvm->io_bitmap_a.phys=noir_get_physical_address(hvm->relative_hvm->io_bitmap_a.virt);
		hvm->relative_hvm->io_bitmap_b.phys=noir_get_physical_address(hvm->relative_hvm->io_bitmap_b.virt);
	}*/
	hvm->relative_hvm->msr_auto_list.virt=noir_alloc_contd_memory(page_size);
	if(hvm->relative_hvm->msr_auto_list.virt)
		hvm->relative_hvm->msr_auto_list.phys=noir_get_physical_address(hvm->relative_hvm->msr_auto_list.virt);
	else
		goto alloc_failure;
	// Query Extended State Enumeration - Useful for xsetbv handler, CVM scheduler, etc.
	noir_cpuid(ia32_cpuid_std_pestate_enum,0,&hvm_p->xfeat.support_mask.low,&hvm_p->xfeat.enabled_size_max,&hvm_p->xfeat.supported_size_max,&hvm_p->xfeat.support_mask.high);
	// if(nvc_vt_build_exit_handlers()==noir_insufficient_resources)goto alloc_failure;
	nvc_vt_set_mshv_handler(hvm_p->options.cpuid_hv_presence);
	hvm->relative_hvm->hvm_cpuid_leaf_max=nvc_mshv_build_cpuid_handlers();
	if(hvm->relative_hvm->hvm_cpuid_leaf_max==0)goto alloc_failure;
	if(hvm->virtual_cpu==null)goto alloc_failure;
	nvc_vt_setup_msr_hook(hvm);
	nvc_vt_setup_msr_auto_list(hvm);
	for(u32 i=0;i<hvm->cpu_count;i++)
		if(nvc_ept_protect_hypervisor(hvm,(noir_ept_manager_p)hvm->virtual_cpu[i].ept_manager)==false)
			goto alloc_failure;
	nv_dprintf("All allocations are done, start subversion!\n");
	noir_generic_call(nvc_vt_subvert_processor_thunk,hvm->virtual_cpu);
	return noir_success;
alloc_failure:
	nv_dprintf("Allocation failure!\n");
	nvc_vt_cleanup(hvm);
	return noir_insufficient_resources;
}

void static nvc_vt_restore_processor(noir_vt_vcpu_p vcpu)
{
	// If we are under VMX Non-Root Operation, trigger VM-Exit.
	if(vcpu->status==noir_virt_on)
		noir_vt_vmcall(noir_vt_callexit,(ulong_ptr)vcpu);
	// At this moment, we are now under VMX Root Operation.
	if(vcpu->status==noir_virt_trans)
		vcpu->status=nvc_vt_disable();
	else
		nv_dprintf("Failed to leave VMX Non-Root Operation!\n");
}

void static nvc_vt_restore_processor_thunk(void* context,u32 processor_id)
{
	noir_vt_vcpu_p vcpu=(noir_vt_vcpu_p)context;
	nv_dprintf("Processor %d entered restoration routine!\n",processor_id);
	nvc_vt_restore_processor(&vcpu[processor_id]);
}

void nvc_vt_restore_system(noir_hypervisor_p hvm)
{
	if(hvm->virtual_cpu)
	{
		noir_generic_call(nvc_vt_restore_processor_thunk,hvm->virtual_cpu);
		nvc_vt_cleanup(hvm);
		nvc_mshv_teardown_cpuid_handlers();
	}
}