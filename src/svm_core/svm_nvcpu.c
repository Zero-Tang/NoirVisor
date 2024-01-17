/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the infrastructure for nested virtualization of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_nvcpu.c
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
#include "svm_npt.h"
#include "svm_exit.h"
#include "svm_nvcpu.h"
#include "svm_def.h"

void noir_hvcode nvc_svm_clear_nested_gif(noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// Disallow guest rflags.if to control interrupt masking.
	noir_svm_vmcb_bts32(vmcb,avic_control,nvc_svm_avic_control_vintr_mask);
	// Invalidate VMCB Cache.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_tpr);
	// Force masking physical interrupts.
	noir_cli();
	// FIXME: Mask physical NMIs by intercepting them.
}

void noir_hvcode nvc_svm_set_nested_gif(noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// Allow guest rflags.if to control interrupt masking.
	noir_svm_vmcb_btr32(vmcb,avic_control,nvc_svm_avic_control_vintr_mask);
	// Invalidate VMCB Cache.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_tpr);
	// FIXME: Unmask physical NMIs.
}

void noir_hvcode nvc_svm_switch_from_nested_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	// Deliver VM-Exit into the nested hypervisor!
	noir_svm_initial_stack_p loader_stack=noir_svm_get_loader_stack(vcpu->hv_stack);
	noir_svm_nested_vcpu_node_p nvcpu=loader_stack->nested_vcpu;
	void* vmcb_t=nvcpu->vmcb_t.virt;
	void* vmcb_c=nvcpu->vmcb_c.virt;
	void* vmcb_l1=vcpu->vmcb.virt;
	// Copy VM-Exit Information.
	noir_svm_vmcopy64(vmcb_c,vmcb_t,exit_code);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,exit_info1);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,exit_info2);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,exit_interrupt_info);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,next_rip);
	noir_svm_vmcopy8(vmcb_c,vmcb_t,number_of_bytes_fetched);
	for(u32 i=0;i<15;i++)noir_svm_vmcopy8(vmcb_c,vmcb_t,guest_instruction_bytes+i);
	// Copy Nested vCPU State.
	// Unfortunately, there's no cached-state mechanism we can utilize here.
	// We have to be dumb and just synchronize everything.
	noir_svm_vmcopy64(vmcb_c,vmcb_t,avic_control);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_interrupt);
	// Copy ES Segment.
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_es_selector);
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_es_attrib);
	noir_svm_vmcopy32(vmcb_c,vmcb_t,guest_es_limit);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_es_base);
	// Copy CS Segment.
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_cs_selector);
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_cs_attrib);
	noir_svm_vmcopy32(vmcb_c,vmcb_t,guest_cs_limit);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_cs_base);
	// Copy SS Segment.
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_ss_selector);
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_ss_attrib);
	noir_svm_vmcopy32(vmcb_c,vmcb_t,guest_ss_limit);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_ss_base);
	// Copy DS Segment.
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_ds_selector);
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_ds_attrib);
	noir_svm_vmcopy32(vmcb_c,vmcb_t,guest_ds_limit);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_ds_base);
	// Copy GDT Register.
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_gdtr_limit);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_gdtr_base);
	// Copy IDT Register.
	noir_svm_vmcopy16(vmcb_c,vmcb_t,guest_idtr_limit);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_gdtr_base);
	// Copy whatever else.
	noir_svm_vmcopy8(vmcb_c,vmcb_t,guest_cpl);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_efer);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_cr4);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_cr3);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_cr0);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_dr7);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_dr6);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_rflags);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_rip);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_rsp);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_s_cet);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_ssp);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_isst);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_rax);
	noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_cr2);
	// We may optionally copy some states due to the switches.
	if(noir_svm_vmcb_bt32(vmcb_t,npt_control,nvc_svm_npt_control_npt))
		noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_pat);
	if(noir_svm_vmcb_bt32(vmcb_t,lbr_virtualization_control,nvc_svm_lbr_virt_control_lbr))
	{
		noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_debug_ctrl);
		noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_last_branch_from);
		noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_last_branch_to);
		noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_last_exception_from);
		noir_svm_vmcopy64(vmcb_c,vmcb_t,guest_last_exception_to);
	}
	// Note that states covered by vmload/vmsave must be copied to L1 VMCB.
	// Copy FS Segment
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_fs_selector);
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_fs_attrib);
	noir_svm_vmcopy32(vmcb_l1,vmcb_t,guest_fs_limit);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_fs_base);
	// Copy GS Segment
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_gs_selector);
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_gs_attrib);
	noir_svm_vmcopy32(vmcb_l1,vmcb_t,guest_gs_limit);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_gs_base);
	// Copy Task Register
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_tr_selector);
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_tr_attrib);
	noir_svm_vmcopy32(vmcb_l1,vmcb_t,guest_tr_limit);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_tr_base);
	// Copy Local Descriptor Table Register
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_ldtr_selector);
	noir_svm_vmcopy16(vmcb_l1,vmcb_t,guest_ldtr_attrib);
	noir_svm_vmcopy32(vmcb_l1,vmcb_t,guest_ldtr_limit);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_ldtr_base);
	// Copy Model-Specific Registers
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_sysenter_cs);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_sysenter_esp);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_sysenter_eip);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_star);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_lstar);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_cstar);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_sfmask);
	noir_svm_vmcopy64(vmcb_l1,vmcb_t,guest_kernel_gs_base);
	// Emulate cleared GIF for nested hypervisor.
	// nvc_svm_clear_nested_gif(vcpu);
	// Note: Clearing rflags.if is an unsafe implementation to emulate cleared GIF!
	// This approach is only viable on bluepill-like hypervisors like SimpleSvm.
	noir_svm_vmcb_btr32(vmcb_l1,guest_rflags,amd64_rflags_if);
	// Switch to the nested hypervisor (L1).
	loader_stack->guest_vmcb_pa=vcpu->vmcb.phys;
}

void noir_hvcode nvc_svm_switch_to_nested_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu_node)
{
	noir_svm_initial_stack_p loader_stack=noir_svm_get_loader_stack(vcpu->hv_stack);
	void* vmcb_t=nvcpu_node->vmcb_t.virt;
	void* vmcb_c=nvcpu_node->vmcb_c.virt;
	// Copy uncached states.
	for(u32 i=0;i<noir_svm_maximum_clean_state_bits;i++)
		if(!nvcpu_node->flags.clean || noir_svm_vmcb_bt32(vmcb_c,vmcb_clean_bits,i)==false)
			svm_clean_vmcb[i](vmcb_c,vmcb_t);
	// Special Treatments.
	if(noir_svm_vmcb_bt64(vmcb_t,avic_control,nvc_svm_avic_control_vintr_mask))
	{
		// If V_INTR_MASKING is enabled, Guest rflags.if must be copied to the host.
		if(noir_svm_vmcb_bt32(vcpu->vmcb.virt,guest_rflags,amd64_rflags_if))
			noir_sti();
		else
			noir_cli();
	}
	// Always-Uncached States.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,tlb_control);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_interrupt);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,event_injection);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_rflags);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_rip);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_rsp);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_rax);
	// Mark cache of this nested vCPU is clean.
	nvcpu_node->flags.clean=true;
	// No need to emulate set GIF because this is a new VMCB.
	// Finally, switch to the nested vCPU (L2).
	loader_stack->nested_vcpu=nvcpu_node;
	loader_stack->guest_vmcb_pa=nvcpu_node->vmcb_t.phys;
	// The context will go to the nested vCPU when vmrun is executed.
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_interceptions(void* vmcb_c,void* vmcb_t)
{
	// FIXME: Implement interception vectors used by NoirVisor in L1.
	// Cached States for Interceptions Vectors are invalidated.
	noir_svm_vmcopy32(vmcb_t,vmcb_c,intercept_access_cr);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,intercept_access_dr);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,intercept_exceptions);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,intercept_instruction1);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,intercept_instruction2);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,intercept_instruction3);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,pause_filter_threshold);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,pause_filter_count);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,tsc_offset);
	// Always-intercept options.
	noir_svm_vmcb_bts32(vmcb_t,intercept_instruction1,nvc_svm_intercept_vector1_shutdown);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_interception);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_iomsrpm(void* vmcb_c,void* vmcb_t)
{
	// FIXME: Emulate I/O and MSR Permission Map.
	// Cached States for I/O and MSR Permission Map are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,iopm_physical_address);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,msrpm_physical_address);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_iomsrpm);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_asid(void* vmcb_c,void* vmcb_t)
{
	// Cached States for ASID are invalidated.
	i32 asid=noir_svm_vmread32(vmcb_c,guest_asid);
	// Nested Hypervisor may trick us with negative ASID.
	noir_svm_vmwrite32(vmcb_t,guest_asid,asid<=0?0:asid+1);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_asid);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_tpr(void* vmcb_c,void* vmcb_t)
{
	// Cached States for Virtualized Local APIC are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,avic_control);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_tpr);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_np(void* vmcb_c,void* vmcb_t)
{
	// FIXME: Virtualize Nested Paging.
	// Cached States for Nested Paging are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,npt_control);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,npt_cr3);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_pat);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_npt);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_crx(void* vmcb_c,void* vmcb_t)
{
	// Cached States for Control Registers are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_cr0);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_cr3);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_cr4);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_efer);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_control_reg);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_drx(void* vmcb_c,void* vmcb_t)
{
	// Cached States for Debug Registers are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_dr6);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_dr7);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_debug_reg);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_dt(void* vmcb_c,void* vmcb_t)
{
	// Cached States for Descriptor Table Registers are invalidated.
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_gdtr_limit);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_gdtr_base);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_idtr_limit);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_idtr_base);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_idt_gdt);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_seg(void* vmcb_c,void* vmcb_t)
{
	// Cached States for Segment Registers are invalidated.
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_cs_selector);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_cs_attrib);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,guest_cs_limit);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_cs_base);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_ds_selector);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_ds_attrib);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,guest_ds_limit);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_ds_base);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_es_selector);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_es_attrib);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,guest_es_limit);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_es_base);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_ss_selector);
	noir_svm_vmcopy16(vmcb_t,vmcb_c,guest_ss_attrib);
	noir_svm_vmcopy32(vmcb_t,vmcb_c,guest_ss_limit);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_ss_base);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_segment_reg);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_cr2(void* vmcb_c,void* vmcb_t)
{
	// Cached State for CR2 Register is invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_cr2);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_cr2);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_lbr(void* vmcb_c,void* vmcb_t)
{
	// Cached States for LBR MSRs are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_debug_ctrl);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_last_branch_from);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_last_branch_to);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_last_exception_from);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_last_exception_to);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_lbr);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_avic(void* vmcb_c,void* vmcb_t)
{
	// Cached States for AVIC are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,avic_apic_bar);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,avic_backing_page_pointer);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,avic_logical_table_pointer);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,avic_physical_table_pointer);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_lbr);
}

void static noir_hvcode fastcall nvc_svm_clean_vmcb_cet(void* vmcb_c,void* vmcb_t)
{
	// Cached States for CET are invalidated.
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_s_cet);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_ssp);
	noir_svm_vmcopy64(vmcb_t,vmcb_c,guest_isst);
	// Clean the cache.
	noir_svm_vmcb_btr32(vmcb_t,vmcb_clean_bits,noir_svm_clean_cet);
}

noir_svm_nested_vcpu_node_p noir_hvcode nvc_svm_get_nested_vcpu_node(noir_svm_nested_vcpu_p nvcpu,u64 vmcb)
{
	// Hash the VMCB GPA into 4 bits.
	u8 hash=0xC;
	for(u32 i=12;i<64;i+=4)
	{
		u8 cur_value=(u8)((vmcb>>i)&0xF);
		hash^=cur_value;
	}
	// Use the hash as index to locate the node.
	return &nvcpu->node_pool[hash];
	// It is caller's responsibility to check hash collision!
}

/*
  Handling VM-Exits from Nested Guest...
*/

// FIXME: Implement handlers for interceptions not used by nested hypervisor!

// Default handler of VM-Exit.
void static fastcall nvc_svm_nvexit_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu)
{
	;
}

// Expected Intercept Code: 0x7F
void static fastcall nvc_svm_nvexit_shutdown_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu)
{
	nvd_printf("Shutdown is intercepted in nested guest!\n");
	noir_int3();
}