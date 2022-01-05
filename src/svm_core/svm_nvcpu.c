/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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
#include "svm_def.h"

i32 static cdecl nvc_svmn_compare_vmcb_node_callback(const void* a,const void* b)
{
	noir_svm_nested_vcpu_node_p node1=(noir_svm_nested_vcpu_node_p)a;
	noir_svm_nested_vcpu_node_p node2=(noir_svm_nested_vcpu_node_p)b;
	if(node1->vmcb_c.phys>=node2->vmcb_c.phys+page_size)
		return 1;
	else if(node1->vmcb_c.phys<node2->vmcb_c.phys)
		return -1;
	return 0;
}

void nvc_svmn_insert_nested_vmcb(noir_svm_vcpu_p vcpu,memory_descriptor_p vmcb)
{
	// Insert to cached nested VMCB list.
	if(vcpu->nested_hvm.nested_vmcb[noir_svm_cached_nested_vmcb-1].vmcb_c.phys==0xffffffffffffffff)
	{
		// There is vacancy, insert to the end.
		vcpu->nested_hvm.nested_vmcb[noir_svm_cached_nested_vmcb-1].vmcb_c=*vmcb;
	}
	else
	{
		// Possible optimization: use multi-dimensional comparison to select cache entry replacements.
		u64 last_tsc=maxu64;
		u32 last_index=0;
		// Search for least-recent-used VMCB entry.
		for(u32 i=0;i<noir_svm_cached_nested_vmcb;i++)
		{
			if(vcpu->nested_hvm.nested_vmcb[i].last_tsc<last_tsc)
			{
				last_tsc=vcpu->nested_hvm.nested_vmcb[i].last_tsc;
				last_index=i;
			}
		}
		// Replace the entry.
		vcpu->nested_hvm.nested_vmcb[last_index].vmcb_c=*vmcb;
		// Clear the cache.
		noir_svm_vmwrite32(vcpu->nested_hvm.nested_vmcb[last_index].vmcb_t.virt,vmcb_clean_bits,0);
	}
	// Sort the list.
	noir_qsort(&vcpu->nested_hvm.nested_vmcb,noir_svm_cached_nested_vmcb,sizeof(noir_svm_nested_vcpu_node),nvc_svmn_compare_vmcb_node_callback);
}

void nvc_svmn_set_gif(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,void* target_vmcb)
{
	void* vmcb=vcpu->vmcb.virt;
	// De-intercept Non-Maskable Interrupts.
	noir_svm_vmcb_btr32(vmcb,intercept_instruction1,nvc_svm_intercept_vector1_nmi);
	// De-intercept #MC and #DB Exceptions.
	noir_svm_vmcb_btr32(vmcb,intercept_exceptions,amd64_debug_exception);
	noir_svm_vmcb_btr32(vmcb,intercept_exceptions,amd64_machine_check);
	// Reset global physical interrupt masking in Local APIC Control.
	noir_svm_vmcb_btr32(vmcb,avic_control,nvc_svm_avic_control_vintr_mask);
	// Mark the relevant fields in VMCB as dirty.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_interception);
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_tpr);
	// Release the interrupts.
	if(vcpu->nested_hvm.pending_mc)
	{
		vcpu->nested_hvm.pending_mc=0;
		noir_svm_inject_event(target_vmcb,amd64_machine_check,amd64_fault_trap_exception,false,true,0);
	}
	else
	{
		if(vcpu->nested_hvm.pending_init)
		{
			vcpu->nested_hvm.pending_init=0;
			if(vcpu->nested_hvm.r_init)
				noir_svm_inject_event(target_vmcb,amd64_security_exception,amd64_fault_trap_exception,true,true,amd64_sx_init_redirection);
			else
				nvc_svm_emulate_init_signal(gpr_state,target_vmcb,vcpu->cpuid_fms);
		}
		else
		{
			if(vcpu->nested_hvm.pending_db)
			{
				vcpu->nested_hvm.pending_db=0;
				noir_svm_inject_event(target_vmcb,amd64_debug_exception,amd64_fault_trap_exception,false,true,0);
			}
			else
			{
				if(vcpu->nested_hvm.pending_nmi)
				{
					vcpu->nested_hvm.pending_nmi=0;
					noir_svm_inject_event(target_vmcb,amd64_nmi_interrupt,amd64_non_maskable_interrupt,false,true,0);
				}
			}
		}
	}
}

void nvc_svmn_clear_gif(noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// Intercept Non-Maskable Interrupts.
	noir_svm_vmcb_bts32(vmcb,intercept_instruction1,nvc_svm_intercept_vector1_nmi);
	// Intercept #MC and #DB Exceptions.
	noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_debug_exception);
	noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_machine_check);
	// Set global physical interrupt masking in Local APIC Control.
	noir_svm_vmcb_bts32(vmcb,avic_control,nvc_svm_avic_control_vintr_mask);
	// Enable Masking of Physical Interrupts.
	noir_cli();
	// Mark the relevant fields in VMCB as dirty.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_interception);
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_tpr);
}

// This procedure is to be called on vmrun interception.
void nvc_svmn_synchronize_to_l2t_vmcb(noir_svm_nested_vcpu_node_p nvcpu)
{
	// Synchronize the L2C VMCB to L2T VMCB.
	// First of all, synchronize all states not cached by clean bits.
	noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_rax,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_rax));
	noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_rsp,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_rsp));
	noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_rip,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_rip));
	noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_rflags,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_rflags));
	noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_interrupt,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_interrupt));
	noir_svm_vmwrite64(nvcpu->vmcb_t.virt,event_injection,noir_svm_vmread64(nvcpu->vmcb_c.virt,event_injection));
	noir_svm_vmwrite8(nvcpu->vmcb_t.virt,tlb_control,noir_svm_vmread8(nvcpu->vmcb_c.virt,tlb_control));
	noir_svm_vmwrite8(nvcpu->vmcb_t.virt,guest_cpl,noir_svm_vmread8(nvcpu->vmcb_c.virt,guest_cpl));
	// Then, traverse the clean bits to determine what fields in VMCBs are supposed be synchronized.
	// Bit 0: Interception Vectors, TSC Offsetting, and Pause-Filter.
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_interception))
	{
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,intercept_access_cr,noir_svm_vmread32(nvcpu->vmcb_c.virt,intercept_access_cr));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,intercept_access_dr,noir_svm_vmread32(nvcpu->vmcb_c.virt,intercept_access_dr));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,intercept_exceptions,noir_svm_vmread32(nvcpu->vmcb_c.virt,intercept_exceptions));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,intercept_instruction1,noir_svm_vmread32(nvcpu->vmcb_c.virt,intercept_instruction1));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,intercept_instruction2,noir_svm_vmread32(nvcpu->vmcb_c.virt,intercept_instruction2));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,intercept_instruction3,noir_svm_vmread32(nvcpu->vmcb_c.virt,intercept_instruction3));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,tsc_offset,noir_svm_vmread64(nvcpu->vmcb_c.virt,tsc_offset));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,pause_filter_count,noir_svm_vmread16(nvcpu->vmcb_c.virt,pause_filter_count));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,pause_filter_threshold,noir_svm_vmread16(nvcpu->vmcb_c.virt,pause_filter_threshold));
	}
	// Bit 1: IOPM and MSRPM
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_iomsrpm))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,iopm_physical_address,noir_svm_vmread64(nvcpu->vmcb_c.virt,iopm_physical_address));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,msrpm_physical_address,noir_svm_vmread64(nvcpu->vmcb_c.virt,msrpm_physical_address));
	}
	// Bit 2: ASID
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_asid))
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,guest_asid,noir_svm_vmread32(nvcpu->vmcb_c.virt,guest_asid)+1);
	// Bit 3: AVIC Control
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_tpr))
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,avic_control,noir_svm_vmread64(nvcpu->vmcb_c.virt,avic_control));
	// Bit 4: NPT
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_npt))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_pat,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_pat));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,npt_control,noir_svm_vmread64(nvcpu->vmcb_c.virt,npt_control));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,npt_cr3,noir_svm_vmread64(nvcpu->vmcb_c.virt,npt_cr3));
	}
	// Bit 5: Control Registers
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_control_reg))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_cr0,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_cr0));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_cr3,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_cr3));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_cr4,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_cr4));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_efer,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_efer));
	}
	// Bit 6: Debug Registers
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_debug_reg))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_dr6,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_dr6));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_dr7,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_dr7));
	}
	// Bit 7: IDTR & GDTR
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_idt_gdt))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_idtr_base,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_idtr_base));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_gdtr_base,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_gdtr_base));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_idtr_limit,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_idtr_limit));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_gdtr_limit,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_gdtr_limit));
	}
	// Bit 8: Segment Registers
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_segment_reg))
	{
		// Selectors
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_cs_selector,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_cs_selector));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_ds_selector,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_ds_selector));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_es_selector,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_es_selector));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_ss_selector,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_ss_selector));
		// Attributes
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_cs_attrib,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_cs_attrib));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_ds_attrib,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_ds_attrib));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_es_attrib,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_es_attrib));
		noir_svm_vmwrite16(nvcpu->vmcb_t.virt,guest_ss_attrib,noir_svm_vmread16(nvcpu->vmcb_c.virt,guest_ss_attrib));
		// Limits
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,guest_cs_limit,noir_svm_vmread32(nvcpu->vmcb_c.virt,guest_cs_limit));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,guest_ds_limit,noir_svm_vmread32(nvcpu->vmcb_c.virt,guest_ds_limit));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,guest_es_limit,noir_svm_vmread32(nvcpu->vmcb_c.virt,guest_es_limit));
		noir_svm_vmwrite32(nvcpu->vmcb_t.virt,guest_ss_limit,noir_svm_vmread32(nvcpu->vmcb_c.virt,guest_ss_limit));
		// Bases
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_cs_base,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_cs_base));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_ds_base,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_ds_base));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_es_base,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_es_base));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_ss_base,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_ss_base));
	}
	// Bit 9: CR2 Register
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_cr2))
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_cr2,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_cr2));
	// Bit 10: Last-Branch Record
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_lbr))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,lbr_virtualization_control,noir_svm_vmread64(nvcpu->vmcb_c.virt,lbr_virtualization_control));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_debug_ctrl,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_debug_ctrl));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_last_branch_from,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_last_branch_from));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_last_branch_to,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_last_branch_to));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_last_exception_from,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_last_exception_from));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_last_exception_to,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_last_exception_to));
	}
	// Bit 11: Advanced Virtual Interrupt Controller
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_avic))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,avic_apic_bar,noir_svm_vmread64(nvcpu->vmcb_c.virt,avic_apic_bar));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,avic_backing_page_pointer,noir_svm_vmread64(nvcpu->vmcb_c.virt,avic_backing_page_pointer));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,avic_logical_table_pointer,noir_svm_vmread64(nvcpu->vmcb_c.virt,avic_logical_table_pointer));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,avic_physical_table_pointer,noir_svm_vmread64(nvcpu->vmcb_c.virt,avic_physical_table_pointer));
	}
	// Bit 12: Shadow Stacks
	if(!noir_svm_vmcb_bt32(nvcpu->vmcb_t.virt,vmcb_clean_bits,noir_svm_clean_cet))
	{
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_s_cet,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_s_cet));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_ssp,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_ssp));
		noir_svm_vmwrite64(nvcpu->vmcb_t.virt,guest_isst,noir_svm_vmread64(nvcpu->vmcb_c.virt,guest_isst));
	}
}

// This procedure is to be called on VM-Exit of nested vCPU.
void nvc_svmn_synchronize_to_l2c_vmcb(noir_svm_nested_vcpu_node_p nvcpu)
{
	// Synchronize the L2T VMCB to L2C VMCB.
	noir_svm_nested_vcpu_node nvcpu_temp;
	u32 prev_clean_bits=noir_svm_vmread32(nvcpu->vmcb_c.virt,vmcb_clean_bits);
	// Use a temporary, field-interexchanged variable to save effort.
	nvcpu_temp.vmcb_t.virt=nvcpu->vmcb_c.virt;
	nvcpu_temp.vmcb_c.virt=nvcpu->vmcb_t.virt;
	// Most fields are mandatory to be synchronized.
	noir_svm_vmwrite32(nvcpu->vmcb_c.virt,vmcb_clean_bits,noir_svm_nesting_vmcb_clean_bits);
	// Use the same synchronization function to reduce code size.
	nvc_svmn_synchronize_to_l2t_vmcb(&nvcpu_temp);
	// Restore VMCB clean bits.
	noir_svm_vmwrite32(nvcpu->vmcb_c.virt,vmcb_clean_bits,prev_clean_bits);
	// Restore ASID.
	noir_svm_vmwrite32(nvcpu->vmcb_c.virt,guest_asid,noir_svm_vmread32(nvcpu->vmcb_c.virt,guest_asid)-2);
	// In addition to guest state, copy exit information.
	noir_svm_vmwrite64(nvcpu->vmcb_c.virt,exit_code,noir_svm_vmread64(nvcpu->vmcb_c.virt,exit_code));
	noir_svm_vmwrite64(nvcpu->vmcb_c.virt,exit_info1,noir_svm_vmread64(nvcpu->vmcb_c.virt,exit_info1));
	noir_svm_vmwrite64(nvcpu->vmcb_c.virt,exit_info2,noir_svm_vmread64(nvcpu->vmcb_c.virt,exit_info2));
	noir_svm_vmwrite64(nvcpu->vmcb_c.virt,exit_interrupt_info,noir_svm_vmread64(nvcpu->vmcb_c.virt,exit_interrupt_info));
	for(u8 i=0;i<15;i++)
		noir_svm_vmwrite8(nvcpu->vmcb_c.virt,guest_instruction_bytes+i,noir_svm_vmread8(nvcpu->vmcb_t.virt,guest_instruction_bytes+i));
	noir_svm_vmwrite8(nvcpu->vmcb_c.virt,number_of_bytes_fetched,noir_svm_vmread8(nvcpu->vmcb_t.virt,number_of_bytes_fetched));
	// Do not forget to save the next RIP.
	noir_svm_vmwrite64(nvcpu->vmcb_c.virt,next_rip,noir_svm_vmread64(nvcpu->vmcb_t.virt,next_rip));
}