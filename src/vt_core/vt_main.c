/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

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
					noir_free_contd_memory(vcpu->vmxon.virt,page_size);
				if(vcpu->vmcs.virt)
					noir_free_contd_memory(vcpu->vmcs.virt,page_size);
				if(vcpu->msr_auto.virt)
					noir_free_contd_memory(vcpu->msr_auto.virt,page_size);
				if(vcpu->nested_vcpu.vmcs_t.virt)
					noir_free_contd_memory(vcpu->nested_vcpu.vmcs_t.virt,page_size);
				if(vcpu->hv_stack)
					noir_free_nonpg_memory(vcpu->hv_stack);
				if(vcpu->cvm_state.xsave_area)
					noir_free_contd_memory(vcpu->cvm_state.xsave_area,page_size);
				nvc_ept_cleanup(vcpu->ept_manager);
			}
			noir_free_nonpg_memory(hvm->virtual_cpu);
		}
		if(rhvm)
		{
			if(rhvm->msr_bitmap.virt)noir_free_contd_memory(rhvm->msr_bitmap.virt,page_size);
			if(rhvm->io_bitmap_a.virt)noir_free_contd_memory(rhvm->io_bitmap_a.virt,page_size);
			if(rhvm->io_bitmap_b.virt)noir_free_contd_memory(rhvm->io_bitmap_b.virt,page_size);
		}
#if !defined(_hv_type1)
		if(hvm->tlb_tagging.vpid_pool_lock)
			noir_finalize_reslock(hvm->tlb_tagging.vpid_pool_lock);
		if(hvm->tlb_tagging.vpid_pool)
			noir_free_nonpg_memory(hvm->tlb_tagging.vpid_pool);
		nvc_vtc_finalize_cvm_module();
#endif
	}
}

void static nvc_vt_setup_io_hook_p(noir_vt_vcpu_p vcpu)
{
	noir_vt_vmwrite64(address_of_io_bitmap_a,vcpu->relative_hvm->io_bitmap_a.phys);
	noir_vt_vmwrite64(address_of_io_bitmap_b,vcpu->relative_hvm->io_bitmap_b.phys);
}

void static nvc_vt_setup_msr_hook_p(noir_vt_vcpu_p vcpu)
{
	u64 entry_load=vcpu->msr_auto.phys;
	u64 exit_load=vcpu->msr_auto.phys+0x400;
	u64 exit_store=vcpu->msr_auto.phys+0x800;
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
		noir_vt_vmwrite(vmexit_msr_store_count,1);
#else
		noir_vt_vmwrite(guest_msr_ia32_sysenter_eip,(ulong_ptr)noir_system_call);
		noir_vt_vmwrite(host_msr_ia32_sysenter_eip,orig_system_call);
#endif
		if(vcpu->enabled_feature & noir_vt_kva_shadow_presence)
		{
			u32 excp_bitmap=1<<ia32_page_fault;
			ia32_page_fault_error_code pfec_mask,pfec_match;
			// If the syscall hook is enabled, #PF must be intercepted.
			noir_vt_vmwrite(exception_bitmap,excp_bitmap);
			// Setup Page-Fault Mask and Match to reduce VM-Exits.
			// Intercept execution only.
			pfec_mask.value=0;
			pfec_mask.user=1;		// We want to filter if the #PF comes from user mode.
			pfec_mask.execute=1;	// We want to filter if the #PF is due to execution.
			pfec_match.value=0;
			pfec_match.execute=1;
			noir_vt_vmwrite(page_fault_error_code_mask,pfec_mask.value);
			noir_vt_vmwrite(page_fault_error_code_match,pfec_match.value);
		}
	}
#endif
}

void static nvc_vt_setup_msr_auto_list(noir_vt_vcpu_p vcpu,noir_processor_state_p state_p)
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
	ia32_vmx_msr_auto_p entry_load=(ia32_vmx_msr_auto_p)((ulong_ptr)vcpu->msr_auto.virt+0);
	ia32_vmx_msr_auto_p exit_load=(ia32_vmx_msr_auto_p)((ulong_ptr)vcpu->msr_auto.virt+0x400);
	ia32_vmx_msr_auto_p exit_store=(ia32_vmx_msr_auto_p)((ulong_ptr)vcpu->msr_auto.virt+0x800);
	ia32_vmx_msr_auto_p cvexit_load=(ia32_vmx_msr_auto_p)((ulong_ptr)vcpu->msr_auto.virt+0xC00);
	unref_var(exit_store);
	// Setup custom MSR-Auto list.
#if !defined(_hv_type1)
	if(vcpu->enabled_feature & noir_vt_syscall_hook)
	{
#if defined(_amd64)
		entry_load[0].index=ia32_lstar;
		entry_load[0].data=(ulong_ptr)noir_system_call;
		exit_load[0].index=ia32_lstar;
		exit_load[0].data=orig_system_call;
		exit_store[0].index=ia32_lstar;
#endif
	}
#else
	unref_var(entry_load);
	unref_var(exit_load);
#endif
	// Setup MSR-Auto List for CVM.
	cvexit_load[noir_vt_cvm_msr_auto_star].index=ia32_star;
	cvexit_load[noir_vt_cvm_msr_auto_star].data=state_p->star;
	cvexit_load[noir_vt_cvm_msr_auto_lstar].index=ia32_lstar;
	cvexit_load[noir_vt_cvm_msr_auto_lstar].data=state_p->lstar;
	cvexit_load[noir_vt_cvm_msr_auto_cstar].index=ia32_cstar;
	cvexit_load[noir_vt_cvm_msr_auto_cstar].data=state_p->cstar;
	cvexit_load[noir_vt_cvm_msr_auto_sfmask].index=ia32_fmask;
	cvexit_load[noir_vt_cvm_msr_auto_sfmask].data=state_p->sfmask;
	cvexit_load[noir_vt_cvm_msr_auto_gsswap].index=ia32_kernel_gs_base;
	cvexit_load[noir_vt_cvm_msr_auto_gsswap].data=state_p->gsswap;
}

void static nvc_vt_setup_io_hook(noir_hypervisor_p hvm)
{
	void* bitmap_a=hvm->relative_hvm->io_bitmap_a.virt;
	void* bitmap_b=hvm->relative_hvm->io_bitmap_b.virt;
	unref_var(bitmap_a);
	unref_var(bitmap_b);
}

void static nvc_vt_setup_msr_hook(noir_hypervisor_p hvm)
{
	void* read_bitmap_low=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0);
	void* read_bitmap_high=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0x400);
	void* write_bitmap_low=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0x800);
	void* write_bitmap_high=(void*)((ulong_ptr)hvm->relative_hvm->msr_bitmap.virt+0xC00);
	// Setup custom MSR-Interception.
#if !defined(_hv_type1)
	if(hvm->options.stealth_msr_hook)
	{
#if defined(_amd64)
		noir_set_bitmap(read_bitmap_high,ia32_lstar-0xC0000000);	// Hide MSR Hook
		noir_set_bitmap(write_bitmap_high,ia32_lstar-0xC0000000);	// Mask MSR Hook
#else
		noir_set_bitmap(read_bitmap_low,ia32_sysenter_eip);			// Hide MSR Hook
		noir_set_bitmap(write_bitmap_low,ia32_sysenter_eip);		// Mask MSR Hook
#endif
	}
#else
	unref_var(read_bitmap_high);
#endif
	// Setup Microcode-Updater MSR Hook. Microcode update should be intercepted in
	// that if it is not intercepted, processor may result in undefined behavior.
	noir_set_bitmap(write_bitmap_low,ia32_bios_updt_trig);
	// Setup MTRR Write-Hook.
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base0);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask0);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base1);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask1);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base2);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask2);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base3);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask3);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base4);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask4);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base5);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask5);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base6);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask6);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base7);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask7);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base8);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask8);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_base9);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_phys_mask9);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix64k_00000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix16k_80000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix16k_a0000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_c0000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_c8000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_d0000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_d8000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_e0000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_e8000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_f0000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_fix4k_f8000);
	noir_set_bitmap(write_bitmap_low,ia32_mtrr_def_type);
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

void static nvc_vt_setup_guest_state_area(noir_processor_state_p state_p,ulong_ptr gsp)
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
#if defined(_hv_type1)
	noir_vt_vmwrite(guest_tr_access_rights,0x808B);	// Hardcode the access rights for TSS.
#else
	noir_vt_vmwrite(guest_tr_access_rights,vt_attrib(state_p->tr.selector,state_p->tr.attrib));
#endif
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
	// Guest State Area - Processor Trace
	if(hvm_p->options.hide_from_pt)
		noir_vt_vmwrite(guest_msr_ia32_rtit_ctrl,noir_rdmsr(ia32_rtit_ctrl));
	// VMCS Link Pointer - Essential for Accelerated VMX Nesting
	noir_vt_vmwrite64(vmcs_link_pointer,0xffffffffffffffff);
	// Guest State Area - Flags, Stack Pointer, Instruction Pointer
	noir_vt_vmwrite(guest_rsp,gsp);
	noir_vt_vmwrite(guest_rip,(ulong_ptr)nvc_vt_guest_start);
	noir_vt_vmwrite(guest_rflags,2);		// That the essential bit is set is fine.
}

void static nvc_vt_setup_host_descriptor_tables(noir_vt_vcpu_p vcpu,noir_processor_state_p state)
{
	// Copy IDT and GDT.
	noir_copy_memory(vcpu->idt_buffer,(void*)state->idtr.base,state->idtr.limit+1);
	noir_copy_memory(vcpu->gdt_buffer,(void*)state->gdtr.base,state->gdtr.limit+1);
#if defined(_hv_type1)
	// In Type-I, create the TSS.
	noir_tss64_p tss_base=(noir_tss64_p)vcpu->tss_buffer;
	u16 tr_sel=(u16)(state->gdtr.limit+1);
	// GDT Entry for TSS.
	noir_segment_descriptor_p tr_seg=(noir_segment_descriptor_p)((ulong_ptr)vcpu->gdt_buffer+tr_sel);
	tr_seg->limit_lo=0xffff;
	tr_seg->base_lo=(u16)vcpu->tss_buffer;
	tr_seg->base_mid1=(u8)((ulong_ptr)vcpu->tss_buffer>>16);
	tr_seg->base_mid2=(u8)((ulong_ptr)vcpu->tss_buffer>>24);
#if defined(_amd64)
	tr_seg->base_hi=(u32)((u64)vcpu->tss_buffer>>32);
	tr_seg->reserved=0;
#endif
	tr_seg->attributes.value=0;
	tr_seg->attributes.type=0x9;
	tr_seg->attributes.present=true;
	tss_base->iomap_base=sizeof(noir_tss64);
#else
	// In Type-II, copy the TSS.
	u16 tr_sel=state->tr.selector&selector_rplti_mask;
	noir_tss64_p tss_base=(noir_tss64_p)state->tr.base;
	noir_copy_memory(vcpu->tss_buffer,(void*)state->tr.base,state->tr.limit+1);
#endif
	// Load into VMCS.
	noir_vt_vmwrite(host_tr_selector,tr_sel);
	noir_vt_vmwrite(host_tr_base,tss_base);
	noir_vt_vmwrite(host_gdtr_base,(ulong_ptr)vcpu->gdt_buffer);
	noir_vt_vmwrite(host_idtr_base,(ulong_ptr)vcpu->idt_buffer);
}

void static nvc_vt_setup_host_state_area(noir_vt_vcpu_p vcpu,noir_processor_state_p state)
{
	// Setup stack for Exit Handler.
	noir_vt_initial_stack_p stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	stack->vcpu=vcpu;
	stack->proc_id=noir_get_current_processor();
	// Host State Area - Descriptor Tables
	nvc_vt_setup_host_descriptor_tables(vcpu,state);
	// Host State Area - Segment Selectors
	noir_vt_vmwrite(host_cs_selector,state->cs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_ds_selector,state->ds.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_es_selector,state->es.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_fs_selector,state->fs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_gs_selector,state->gs.selector & selector_rplti_mask);
	noir_vt_vmwrite(host_ss_selector,state->ss.selector & selector_rplti_mask);
	// Host State Area - Segment Bases
	noir_vt_vmwrite(host_fs_base,(ulong_ptr)state->fs.base);
	noir_vt_vmwrite(host_gs_base,(ulong_ptr)state->gs.base);
	// Host State Area - Control Registers
	state->cr0|=noir_rdmsr(ia32_vmx_cr0_fixed0);
	state->cr0&=noir_rdmsr(ia32_vmx_cr0_fixed1);
	state->cr4|=noir_rdmsr(ia32_vmx_cr4_fixed0);
	state->cr4&=noir_rdmsr(ia32_vmx_cr4_fixed1);
	noir_btr(&state->cr4,ia32_cr4_cet);			// Turn off CET in host mode.
	noir_vt_vmwrite(host_cr0,state->cr0);
	noir_vt_vmwrite(host_cr3,system_cr3);
	// noir_vt_vmwrite(host_cr3,hvm_p->host_memmap.hcr3.phys);
	noir_vt_vmwrite(host_cr4,state->cr4);
	noir_vt_vmwrite(host_msr_ia32_efer,state->efer);
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
	proc_ctrl.use_io_bitmap=1;		// Intercept exclusive I/O operations.
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
		// If you wish to turn off EPT, you must turn off Unrestricted Guest as well.
		proc_ctrl2.unrestricted_guest=1;	// Allows the guest to enter unpaged mode.
		proc_ctrl2.enable_invpcid=1;
		proc_ctrl2.enable_xsaves_xrstors=1;
		if(hvm_p->options.hide_from_pt)		// Don't allow Intel Processor Trace to bypass EPT.
			proc_ctrl2.use_gpa_for_intel_pt=1;
		proc_ctrl2.enable_user_wait_and_pause=1;
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
	exit_ctrl.save_debug_controls=1;
#if defined(_amd64)
	// This field should be set if NoirVisor is in 64-Bit mode.
	exit_ctrl.host_address_space_size=1;
#endif
	// EFER is to be saved and loaded on exit.
	exit_ctrl.load_ia32_efer=exit_ctrl.save_ia32_efer=1;
	if(hvm_p->options.hide_from_pt)
	{
		exit_ctrl.conceal_vmexit_from_pt=1;
		exit_ctrl.clear_ia32_rtit_ctrl=1;
	}
	// In order to prevent the guest from recording the LBR in host, clear LBR_CTRL.
	exit_ctrl.clear_ia32_lbr_ctrl=1;
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
	entry_ctrl.load_debug_controls=1;
#if defined(_amd64)
	// This field should be set if NoirVisor is in 64-Bit mode.
	entry_ctrl.ia32e_mode_guest=1;
#endif
	entry_ctrl.load_ia32_efer=1;		// EFER is to be loaded on entry.
	if(hvm_p->options.hide_from_pt)
	{
		entry_ctrl.conceal_vmentry_from_pt=1;
		entry_ctrl.load_ia32_rtit_ctrl=1;
	}
	// In order to prevent the guest from recording LBR in host, virtualize LBR_CTRL.
	entry_ctrl.load_ia32_lbr_ctrl=1;
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
		noir_vt_vmwrite64(ept_pointer,vcpu->ept_manager->eptp.phys.value);
	// It is required to setup memory types on a per-processor basis.
	nvc_ept_update_by_mtrr(vcpu->ept_manager);
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
	noir_vt_vmwrite(cr0_guest_host_mask,ia32_cr0_cd_bit);			// Monitor CD flags
	noir_vt_vmwrite(cr4_guest_host_mask,ia32_cr4_vmxe_bit);			// Monitor VMXE flags
}

u8 nvc_vt_subvert_processor_i(noir_vt_vcpu_p vcpu,void* reserved,ulong_ptr gsp)
{
	ia32_vmx_basic_msr vt_basic;
	u8 vst=0;
	noir_processor_state state;
	noir_save_processor_state(&state);
	// Issue a sequence of vmwrite instructions to setup VMCS.
	vt_basic.value=noir_rdmsr(ia32_vmx_basic);
	nvc_vt_setup_available_features(vcpu);
	nvc_vt_setup_control_area(vt_basic.use_true_msr);
	nvc_vt_setup_guest_state_area(&state,gsp);
	nvc_vt_setup_host_state_area(vcpu,&state);
	nvc_vt_setup_memory_virtualization(vcpu);
	nvc_vt_setup_msr_auto_list(vcpu,&state);
	nvc_vt_setup_msr_hook_p(vcpu);
	nvc_vt_setup_io_hook_p(vcpu);
	nvc_vt_setup_virtual_msr(vcpu);
	vcpu->status=noir_virt_on;
	// Everything are done, perform subversion.
	nv_dprintf("Launching vCPU...\n");
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
	nvd_printf("Processor %d entered subversion routine!\n",processor_id);
	nvc_vt_subvert_processor(&vcpu[processor_id]);
}

// This function builds an identity map for NoirVisor, as Type-II hypervisor, to access physical addresses.
bool nvc_vt_build_host_page_table(noir_hypervisor_p hvm_p)
{
	void* scr3_virt=noir_find_virt_by_phys(page_base(system_cr3));
	nvd_printf("System CR3 Virtual Address: 0x%p\tPhysical Address: 0x%p\n",scr3_virt,system_cr3);
	if(scr3_virt)
	{
		hvm_p->host_memmap.hcr3.virt=noir_alloc_contd_memory(page_size);
		if(hvm_p->host_memmap.hcr3.virt)
		{
			hvm_p->host_memmap.hcr3.phys=noir_get_physical_address(hvm_p->host_memmap.hcr3.virt);
			// Copy the PML4Es from system CR3.
			noir_copy_memory(hvm_p->host_memmap.hcr3.virt,scr3_virt,page_size);
			// Allocate the PDPTE page for the first PML4E.
			hvm_p->host_memmap.pdpt.virt=noir_alloc_contd_memory(page_size);
			if(hvm_p->host_memmap.pdpt.virt)
			{
				ia32_pml4e_p pml4e_p=(ia32_pml4e_p)hvm_p->host_memmap.hcr3.virt;
				ia32_huge_pdpte_p pdpte_p=(ia32_huge_pdpte_p)hvm_p->host_memmap.pdpt.virt;
				for(u32 i=0;i<512;i++)
				{
					// Set up identity mappings.
					pdpte_p[i].value=0;
					pdpte_p[i].present=1;
					pdpte_p[i].write=1;
					pdpte_p[i].huge_pdpte=1;		// Use 1GiB Huge Page.
					pdpte_p[i].page_base=i;
				}
				// The first PML4E points to PDPTEs.
				hvm_p->host_memmap.pdpt.phys=noir_get_physical_address(hvm_p->host_memmap.pdpt.virt);
				// Load to first PML4E.
				pml4e_p->value=page_base(hvm_p->host_memmap.pdpt.phys);
				pml4e_p->present=1;
				pml4e_p->write=1;
				nvd_printf("Host CR3: 0x%llX\n",hvm_p->host_memmap.hcr3.phys);
				return true;
			}
		}
	}
	return false;
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
	// Query Extended State Enumeration - Useful for xsetbv handler, CVM scheduler, etc.
	noir_cpuid(ia32_cpuid_std_pestate_enum,0,&hvm_p->xfeat.support_mask.low,&hvm_p->xfeat.enabled_size_max,&hvm_p->xfeat.supported_size_max,&hvm_p->xfeat.support_mask.high);
	hvm->cpu_count=noir_get_processor_count();
	hvm->relative_hvm=(noir_vt_hvm_p)hvm->reserved;
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
			vcpu->msr_auto.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->msr_auto.virt)
				vcpu->msr_auto.phys=noir_get_physical_address(vcpu->msr_auto.virt);
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
			vcpu->cvm_state.xsave_area=noir_alloc_contd_memory(hvm->xfeat.supported_size_max);
			if(vcpu->cvm_state.xsave_area==null)
				goto alloc_failure;
			if(hvm_p->options.stealth_msr_hook)
			{
				if(hvm_p->options.kva_shadow_presence)
				{
					nv_dprintf("KVA Shadow is present in the system!\n");
					vcpu->enabled_feature|=noir_vt_kva_shadow_presence;
				}
				vcpu->enabled_feature|=noir_vt_syscall_hook;
			}
			vcpu->relative_hvm=(noir_vt_hvm_p)hvm->reserved;
			vcpu->mshvcpu.root_vcpu=(void*)vcpu;
		}
	}
	hvm->relative_hvm->msr_bitmap.virt=noir_alloc_contd_memory(page_size);
	if(hvm->relative_hvm->msr_bitmap.virt)
		hvm->relative_hvm->msr_bitmap.phys=noir_get_physical_address(hvm->relative_hvm->msr_bitmap.virt);
	else
		goto alloc_failure;
	// Some I/O operations must be prohibited in that they are exclusive for hypervisors.
	hvm->relative_hvm->io_bitmap_a.virt=noir_alloc_contd_memory(page_size);
	hvm->relative_hvm->io_bitmap_b.virt=noir_alloc_contd_memory(page_size);
	if(hvm->relative_hvm->io_bitmap_a.virt && hvm->relative_hvm->io_bitmap_b.virt)
	{
		hvm->relative_hvm->io_bitmap_a.phys=noir_get_physical_address(hvm->relative_hvm->io_bitmap_a.virt);
		hvm->relative_hvm->io_bitmap_b.phys=noir_get_physical_address(hvm->relative_hvm->io_bitmap_b.virt);
	}
	hvm->options.tlfs_passthrough=noir_is_under_hvm();
	if(hvm->options.tlfs_passthrough && hvm->options.cpuid_hv_presence)
		nv_dprintf("Note: Hypervisor is detected! The cpuid presence will be in pass-through mode!\n");
	if(hvm->options.hide_from_pt)
	{
		u32 sextid_ebx;
		noir_cpuid(ia32_cpuid_std_struct_extid,0,null,&sextid_ebx,null,null);
		// Remove the "Hide from Processor Trace" if it is not supported at all.
		hvm->options.hide_from_pt=noir_bt(&sextid_ebx,ia32_cpuid_pt);
	}
	nvc_vt_set_mshv_handler(hvm->options.tlfs_passthrough?false:hvm_p->options.cpuid_hv_presence);
	hvm->relative_hvm->hvm_cpuid_leaf_max=nvc_mshv_build_cpuid_handlers();
	if(hvm->relative_hvm->hvm_cpuid_leaf_max==0)goto alloc_failure;
	if(hvm->virtual_cpu==null)goto alloc_failure;
	// Build Host CR3 in order to operate physical addresses directly.
	if(nvc_vt_build_host_page_table(hvm_p))
		nv_dprintf("Hypervisor's paging structure is initialized successfully!\n");
	else
		nv_dprintf("Failed to build hypervisor's paging structure...\n");
	nvc_vt_setup_msr_hook(hvm);
	nvc_vt_setup_io_hook(hvm);
	for(u32 i=0;i<hvm->cpu_count;i++)
		if(nvc_ept_protect_hypervisor(hvm,(noir_ept_manager_p)hvm->virtual_cpu[i].ept_manager)==false)
			goto alloc_failure;
#if !defined(_hv_type1)
	if(nvc_vtc_initialize_cvm_module()!=noir_success)goto alloc_failure;
	// Initialize VPID Pool for Customizable VMs.
	if(hvm->options.nested_virtualization)
	{
		// If nested virtualization is enabled, use 32768-2 only for CVM.
		hvm->tlb_tagging.vpid_pool=noir_alloc_nonpg_memory(page_size);
		hvm->tlb_tagging.limit=32766;
	}
	else
	{
		// Otherwise, all available VPIDs (65534) belong to CVM.
		hvm->tlb_tagging.vpid_pool=noir_alloc_nonpg_memory(page_size*2);
		hvm->tlb_tagging.limit=65534;
	}
	if(hvm->tlb_tagging.vpid_pool==null)goto alloc_failure;
	hvm->tlb_tagging.vpid_pool_lock=noir_initialize_reslock();
	if(hvm->tlb_tagging.vpid_pool_lock==null)goto alloc_failure;
	hvm->tlb_tagging.start=2;
#endif
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