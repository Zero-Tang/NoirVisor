/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the exit handler of Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_exit.c
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

void nvc_vt_dump_vmcs_guest_state()
{
	ulong_ptr v1,v2,v3,v4;
	nv_dprintf("Dumping VMCS Guest State Area...\n");
	noir_vt_vmread(guest_cs_selector,&v1);
	noir_vt_vmread(guest_cs_access_rights,&v2);
	noir_vt_vmread(guest_cs_limit,&v3);
	noir_vt_vmread(guest_cs_base,&v4);
	nv_dprintf("Guest CS Segment - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_ds_selector,&v1);
	noir_vt_vmread(guest_ds_access_rights,&v2);
	noir_vt_vmread(guest_ds_limit,&v3);
	noir_vt_vmread(guest_ds_base,&v4);
	nv_dprintf("Guest DS Segment - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_es_selector,&v1);
	noir_vt_vmread(guest_es_access_rights,&v2);
	noir_vt_vmread(guest_es_limit,&v3);
	noir_vt_vmread(guest_es_base,&v4);
	nv_dprintf("Guest ES Segment - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_fs_selector,&v1);
	noir_vt_vmread(guest_fs_access_rights,&v2);
	noir_vt_vmread(guest_fs_limit,&v3);
	noir_vt_vmread(guest_fs_base,&v4);
	nv_dprintf("Guest FS Segment - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_gs_selector,&v1);
	noir_vt_vmread(guest_gs_access_rights,&v2);
	noir_vt_vmread(guest_gs_limit,&v3);
	noir_vt_vmread(guest_gs_base,&v4);
	nv_dprintf("Guest GS Segment - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_ss_selector,&v1);
	noir_vt_vmread(guest_ss_access_rights,&v2);
	noir_vt_vmread(guest_ss_limit,&v3);
	noir_vt_vmread(guest_ss_base,&v4);
	nv_dprintf("Guest SS Segment - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_tr_selector,&v1);
	noir_vt_vmread(guest_tr_access_rights,&v2);
	noir_vt_vmread(guest_tr_limit,&v3);
	noir_vt_vmread(guest_tr_base,&v4);
	nv_dprintf("Guest Task Register - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_ldtr_selector,&v1);
	noir_vt_vmread(guest_ldtr_access_rights,&v2);
	noir_vt_vmread(guest_ldtr_limit,&v3);
	noir_vt_vmread(guest_ldtr_base,&v4);
	nv_dprintf("Guest LDT Register - Selector: 0x%X\t Access-Rights: 0x%X\t Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_gdtr_limit,&v1);
	noir_vt_vmread(guest_gdtr_base,&v2);
	noir_vt_vmread(guest_idtr_limit,&v3);
	noir_vt_vmread(guest_idtr_base,&v4);
	nv_dprintf("Guest GDTR Limit: 0x%X\t Base: 0x%p\t Guest IDTR Limit: 0x%X\t Base: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_cr0,&v1);
	noir_vt_vmread(guest_cr3,&v2);
	noir_vt_vmread(guest_cr4,&v3);
	noir_vt_vmread(guest_dr7,&v4);
	nv_dprintf("Guest Control & Debug Registers - CR0: 0x%X\t CR3: 0x%p\t CR4: 0x%X\t DR7: 0x%X\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_rsp,&v1);
	noir_vt_vmread(guest_rip,&v2);
	noir_vt_vmread(guest_rflags,&v3);
	noir_vt_vmread(guest_ssp,&v4);
	nv_dprintf("Guest Special GPRs: Rsp: 0x%p\t Rip: 0x%p\t RFlags:0x%X\t Ssp: 0x%p\n",v1,v2,v3,v4);
	noir_vt_vmread(guest_activity_state,&v1);
	noir_vt_vmread(guest_interruptibility_state,&v2);
	noir_vt_vmread(guest_pending_debug_exceptions,&v3);
	nv_dprintf("Guest Activity State: %u\t Interruptibility State: 0x%X\t Pending Debug Exceptions: 0x%X\n",v1,v2,v3);
	noir_vt_vmread(guest_msr_ia32_efer,&v1);
	noir_vt_vmread(guest_msr_ia32_pat,&v2);
	noir_vt_vmread(guest_msr_ia32_debug_ctrl,&v3);
	nv_dprintf("Guest EFER: 0x%llX\t PAT: 0x%llX\t Debug Control: 0x%llX\n",v1,v2,v3);
	noir_vt_vmread(vmcs_link_pointer,&v4);
	nv_dprintf("Guest VMCS Link Pointer: 0x%p\n",v4);
	noir_vt_vmread(guest_pdpte0,&v1);
	noir_vt_vmread(guest_pdpte1,&v2);
	noir_vt_vmread(guest_pdpte2,&v3);
	noir_vt_vmread(guest_pdpte3,&v4);
	nv_dprintf("Guest PDPTE0: 0x%llX\t PDPTE1: 0x%llX\t PDPTE2: 0x%llX\t PDPTE3: 0x%llX\n",v1,v2,v3,v4);
	noir_vt_vmread(pin_based_vm_execution_controls,&v1);
	noir_vt_vmread(primary_processor_based_vm_execution_controls,&v2);
	noir_vt_vmread(secondary_processor_based_vm_execution_controls,&v3);
	noir_vt_vmread(exception_bitmap,&v4);
	nv_dprintf("Pin-Based Controls: 0x%X\t Primary Controls: 0x%X\t Secondary Controls: 0x%X\t Exception Bitmap: 0x%X\n",v1,v2,v3,v4);
	noir_vt_vmread(vmexit_controls,&v1);
	noir_vt_vmread(vmentry_controls,&v2);
	nv_dprintf("VM-Exit Controls: 0x%X\t VM-Entry Controls: 0x%X\n",v1,v2);
	noir_vt_vmread(ept_pointer,&v3);
	nv_dprintf("EPT Pointer: 0x%llX\n",v3);
	noir_vt_vmread(vmentry_interruption_information_field,&v4);
	nv_dprintf("Event Injection: 0x%X\n",v4);
}

// This is the default exit-handler for unexpected VM-Exit.
// You might want to debug your code if this function is invoked.
void static fastcall nvc_vt_default_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	u32 exit_reason;
	noir_vt_vmread(vmexit_reason,&exit_reason);
	nv_dprintf("Unhandled VM-Exit!, Exit Reason: %s\n",vmx_exit_msg[exit_reason&0xFFFF]);
}

// Expected Exit Reason: 0
// If this handler is invoked, it should be #PF due to syscall under KVA-shadowing.
void static fastcall nvc_vt_exception_nmi_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	ia32_vmexit_interruption_information_field exit_int_info;
	noir_vt_vmread(vmexit_interruption_information,&exit_int_info.value);
	if(exit_int_info.valid)
	{
		switch(exit_int_info.vector)
		{
			case ia32_page_fault:
			{
#if !defined(_hv_type1)
				ulong_ptr gcr2;
				ia32_page_fault_error_code err_code;
				noir_vt_vmread(vmexit_interruption_error_code,&err_code.value);
				// Processor does not update CR2 on #PF Exit, but the faulting address is saved in Exit Qualification.
				noir_vt_vmread(vmexit_qualification,&gcr2);
				if(gcr2==(ulong_ptr)noir_system_call)
				{
					// ulong_ptr gcr4;
					u64 gcr3k=noir_get_current_process_cr3();
					// Switch to KVA-unshadowed CR3 of the process.
					noir_vt_vmwrite(guest_cr3,gcr3k);
					// noir_vt_vmread(guest_cr4,&gcr4);
					// Flush Guest TLB because the Guest CR3 is changed.
					if(vcpu->enabled_feature & noir_vt_vpid_tagged_tlb)
					{
						invvpid_descriptor ivd={0};
						ivd.vpid=1;
						// A write to CR3 does not invalidate global TLB entries.
						noir_vt_invvpid(vpid_sicrgb_invd,&ivd);
					}
				}
				else
				{
					// This page-fault is not expected. Forward to the guest.
					noir_vt_inject_event(ia32_page_fault,ia32_hardware_exception,true,0,err_code.value);
					// Update the CR2 for the Guest.
					noir_writecr2(gcr2);
				}
#endif
				break;
			}
			default:
			{
				nv_panicf("Unexpected exception is intercepted!\n");
				noir_int3();
			}
		}
	}
}

// Expected Exit Reason: 2
// If this handler is invoked, we should crash the system.
// This is VM-Exit of obligation.
void static fastcall nvc_vt_trifault_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	nv_panicf("Triple fault occured! System is crashed!\n");
}

// Expected Exit Reason: 3
// This is VM-Exit of obligation.
void static fastcall nvc_vt_init_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	vmx_segment_access_right code_ar,data_ar,ldtr_ar,task_ar;
	ia32_vmx_entry_controls entry_ctrl;
	ulong_ptr cr0;
	// General-Purpose Registers
	noir_vt_vmwrite(guest_rip,0xFFF0);
	noir_vt_vmwrite(guest_rsp,0);
	noir_vt_vmwrite(guest_rflags,2);
	noir_stosp(gpr_state,0,sizeof(void*)*2);
	gpr_state->rdx=vcpu->family_ext;
	// Control Registers
	noir_vt_vmread(guest_cr0,&cr0);
	cr0&=0x60000000;		// Bits CD & NW of CR0 are unchanged during INIT.
	cr0|=0x00000010;		// Bit ET of CR0 is always set.
	noir_vt_vmwrite(guest_cr0,cr0);
	noir_vt_vmwrite(cr0_read_shadow,0x60000010);
	noir_vt_vmwrite(guest_cr3,0);
	noir_vt_vmwrite(guest_cr4,ia32_cr4_vmxe_bit);
	noir_vt_vmwrite(cr4_read_shadow,0);
	noir_writecr2(0);
	noir_vt_vmwrite(guest_msr_ia32_efer,0);
	// Debug Registers
	noir_writedr0(0);
	noir_writedr1(0);
	noir_writedr2(0);
	noir_writedr3(0);
	noir_writedr6(0xFFFF0FF0);
	noir_vt_vmwrite(guest_dr7,0x400);
	// Segment Register Access Rights
	code_ar.value=data_ar.value=ldtr_ar.value=task_ar.value=0;
	code_ar.present=data_ar.present=ldtr_ar.present=task_ar.present=1;
	code_ar.descriptor_type=data_ar.descriptor_type=1;
	code_ar.segment_type=ia32_segment_code_rx_accessed;
	data_ar.segment_type=ia32_segment_data_rw_accessed;
	ldtr_ar.segment_type=ia32_segment_system_ldt;
	task_ar.segment_type=ia32_segment_system_16bit_tss_busy;
	// Segment Registers - CS
	noir_vt_vmwrite(guest_cs_selector,0xF000);
	noir_vt_vmwrite(guest_cs_access_rights,code_ar.value);
	noir_vt_vmwrite(guest_cs_limit,0xFFFF);
	noir_vt_vmwrite(guest_cs_base,0xFFFF0000);
	// Segment Registers - DS
	noir_vt_vmwrite(guest_ds_selector,0);
	noir_vt_vmwrite(guest_ds_access_rights,data_ar.value);
	noir_vt_vmwrite(guest_ds_limit,0xFFFF);
	noir_vt_vmwrite(guest_ds_base,0);
	// Segment Registers - ES
	noir_vt_vmwrite(guest_es_selector,0);
	noir_vt_vmwrite(guest_es_access_rights,data_ar.value);
	noir_vt_vmwrite(guest_es_limit,0xFFFF);
	noir_vt_vmwrite(guest_es_base,0);
	// Segment Registers - FS
	noir_vt_vmwrite(guest_gs_selector,0);
	noir_vt_vmwrite(guest_gs_access_rights,data_ar.value);
	noir_vt_vmwrite(guest_gs_limit,0xFFFF);
	noir_vt_vmwrite(guest_gs_base,0);
	// Segment Registers - GS
	noir_vt_vmwrite(guest_gs_selector,0);
	noir_vt_vmwrite(guest_gs_access_rights,data_ar.value);
	noir_vt_vmwrite(guest_gs_limit,0xFFFF);
	noir_vt_vmwrite(guest_gs_base,0);
	// Segment Registers - SS
	noir_vt_vmwrite(guest_ss_selector,0);
	noir_vt_vmwrite(guest_ss_access_rights,data_ar.value);
	noir_vt_vmwrite(guest_ss_limit,0xFFFF);
	noir_vt_vmwrite(guest_ss_base,0);
	// Segment Registers - LDTR
	noir_vt_vmwrite(guest_ldtr_selector,0);
	noir_vt_vmwrite(guest_ldtr_access_rights,ldtr_ar.value);
	noir_vt_vmwrite(guest_ldtr_limit,0xFFFF);
	noir_vt_vmwrite(guest_ldtr_base,0);
	// Segment Registers - TR
	noir_vt_vmwrite(guest_tr_selector,0);
	noir_vt_vmwrite(guest_tr_access_rights,task_ar.value);
	noir_vt_vmwrite(guest_tr_limit,0xFFFF);
	noir_vt_vmwrite(guest_tr_base,0);
	// IDTR & GDTR
	noir_vt_vmwrite(guest_gdtr_base,0);
	noir_vt_vmwrite(guest_idtr_base,0);
	noir_vt_vmwrite(guest_gdtr_limit,0xFFFF);
	noir_vt_vmwrite(guest_idtr_limit,0xFFFF);
	// VM-Entry Controls (Important: Guest is definitely not in IA-32e mode)
	noir_vt_vmread(vmentry_controls,&entry_ctrl.value);
	entry_ctrl.ia32e_mode_guest=0;
	noir_vt_vmwrite(vmentry_controls,entry_ctrl.value);
	// Invalidate TLBs associated with this vCPU in that paging is switched off.
	// There is no need to invalidate TLBs related to EPT.
	if(vcpu->enabled_feature & noir_vt_vpid_tagged_tlb)
	{
		invvpid_descriptor ivd;
		noir_vt_vmread(virtual_processor_identifier,&ivd.vpid);
		ivd.reserved[0]=ivd.reserved[1]=ivd.reserved[2]=0;
		ivd.linear_address=0;
		noir_vt_invvpid(vpid_single_invd,&ivd);
	}
	// Upon INIT, vCPU enters inactive state to wait for Startup-IPI.
	noir_vt_vmwrite(guest_activity_state,guest_is_waiting_for_sipi);
}

// Expected Exit Reason: 4
// This is VM-Exit of obligation.
void static fastcall nvc_vt_sipi_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	// The SIPI Vector is stored in Exit-Qualification.
	ulong_ptr vector;
	noir_vt_vmread(vmexit_qualification,&vector);
	// According to vector, set control-flow fields of vCPU.
	noir_vt_vmwrite(guest_cs_selector,vector<<8);
	noir_vt_vmwrite(guest_cs_base,vector<<12);
	noir_vt_vmwrite(guest_rip,0);
	// Startup-IPI is received, resume to active state.
	noir_vt_vmwrite(guest_activity_state,guest_is_active);
}

// Expected Exit Reason: 9
// This is not an expected VM-Exit. In handler, we should help to switch task.
// This is VM-Exit of obligation.
void static fastcall nvc_vt_task_switch_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	nv_dprintf("Task switch is intercepted!");
}

// Hypervisor-Present CPUID Handler
void static fastcall nvc_vt_cpuid_hvp_handler(u32 leaf,u32 subleaf,noir_cpuid_general_info_p info)
{
	const u32 leaf_class=noir_cpuid_class(leaf);
	const u32 leaf_func=noir_cpuid_index(leaf);
	if(leaf_class==hvm_leaf_index)
	{
		if(leaf_func<hvm_p->relative_hvm->hvm_cpuid_leaf_max)
			hvm_cpuid_handlers[leaf_func](info);
		else
			noir_stosd((u32*)&info,0,4);
	}
	else
	{
		noir_cpuid(leaf,subleaf,&info->eax,&info->ebx,&info->ecx,&info->edx);
		switch(leaf)
		{
			case ia32_cpuid_std_proc_feature:
			{
				noir_btr(&info->ecx,ia32_cpuid_vmx);
				noir_bts(&info->ecx,ia32_cpuid_hv_presence);
				break;
			}
		}
	}
}

// Hypervisor-Stealthy or Hypervisor-Passthrough CPUID Handler
void static fastcall nvc_vt_cpuid_hvs_handler(u32 leaf,u32 subleaf,noir_cpuid_general_info_p info)
{
	noir_cpuid(leaf,subleaf,&info->eax,&info->ebx,&info->ecx,&info->edx);
	switch(leaf)
	{
		case ia32_cpuid_std_proc_feature:
		{
			noir_btr(&info->ecx,ia32_cpuid_vmx);
			break;
		}
	}
}

// Expected Exit Reason: 10
// This is VM-Exit of obligation.
void static fastcall nvc_vt_cpuid_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	u32 ia=(u32)gpr_state->rax;
	u32 ic=(u32)gpr_state->rcx;
	noir_cpuid_general_info info;
	// Invoke handlers.
	nvcp_vt_cpuid_handler(ia,ic,&info);
	*(u32*)&gpr_state->rax=info.eax;
	*(u32*)&gpr_state->rbx=info.ebx;
	*(u32*)&gpr_state->rcx=info.ecx;
	*(u32*)&gpr_state->rdx=info.edx;
	// Finally, advance the instruction pointer.
	noir_vt_advance_rip();
}

// Expected Exit Reason: 11
// We should virtualize the SMX (Safer Mode Extension) in this handler.
// This is VM-Exit of obligation.
void static fastcall nvc_vt_getsec_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	nv_dprintf("SMX Virtualization is not supported!\n");
	noir_vt_advance_rip();
}

// Expected Exit Reason: 13
// This is VM-Exit of obligation.
void static fastcall nvc_vt_invd_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	// In Hyper-V, it invoked wbinvd at invd exit.
	nv_dprintf("The invd instruction is executed!\n");
	noir_wbinvd();
	noir_vt_advance_rip();
}

// Expected Exit Reason: 18
// This is VM-Exit of obligation.
void static fastcall nvc_vt_vmcall_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	ulong_ptr gip,gcr3;
	u32 index=(u32)gpr_state->rcx;
	noir_vt_vmread(guest_rip,&gip);
	noir_vt_vmread(guest_cr3,&gcr3);
	switch(index)
	{
		case noir_vt_callexit:
		{
			// We should verify that the vmcall is inside the NoirVisor's image before unloading.
			if(gip>=hvm_p->hv_image.base && gip<hvm_p->hv_image.base+hvm_p->hv_image.size)
			{
				noir_gpr_state_p saved_state=(noir_gpr_state_p)vcpu->hv_stack;
				ulong_ptr gsp,gflags,gcr4;
				descriptor_register idtr,gdtr;
				u32 inslen=3;		// By default, vmcall uses 3 bytes.
				noir_vt_vmread(guest_rsp,&gsp);
				noir_vt_vmread(guest_rflags,&gflags);
				noir_vt_vmread(guest_cr4,&gcr4);
				noir_vt_vmread(vmexit_instruction_length,&inslen);
				// We may allocate space from unused HV-Stack
				noir_movsp(saved_state,gpr_state,sizeof(void*)*2);
				saved_state->rax=gip+inslen;
				saved_state->rcx=gflags&0x3f7f96;		// Clear ZF and CF bits.
				saved_state->rdx=gsp;
				// Invalidate all TLBs associated with EPT and VPID.
				// If EPT/VPID is disabled, do not invalidate.
				if(vcpu->enabled_feature & noir_vt_extended_paging)
				{
					invept_descriptor ied;
					ied.eptp=0;
					ied.reserved=0;
					noir_vt_invept(ept_global_invd,&ied);
				}
				if(vcpu->enabled_feature & noir_vt_vpid_tagged_tlb)
				{
					invvpid_descriptor ivd;
					ivd.vpid=0;
					ivd.reserved[0]=ivd.reserved[1]=ivd.reserved[2]=0;
					ivd.linear_address=0;
					noir_vt_invvpid(vpid_global_invd,&ivd);
				}
				// Restore Host Control Registers.
				noir_writecr3(gcr3);
				noir_writecr4(gcr4);
				// Restore Host IDT.
				noir_vt_vmread(guest_idtr_limit,&idtr.limit);
				noir_vt_vmread(guest_idtr_base,&idtr.base);
				noir_lidt(&idtr);
				// Restore Host GDT.
				noir_vt_vmread(guest_gdtr_limit,&gdtr.limit);
				noir_vt_vmread(guest_gdtr_base,&gdtr.base);
				noir_lgdt(&gdtr);
				// Mark vCPU is in transition state
				vcpu->status=noir_virt_trans;
				nv_dprintf("Restoration completed!\n");
				nvc_vt_resume_without_entry(saved_state);
				// The code won't reach here.
				noir_vt_advance_rip();
			}
			break;
		}
		case noir_vt_init_custom_vmcs:
		{
			// For CVM hypercalls, the caller must be located in Layered Hypervisor.
			if(gip>=hvm_p->layered_hv_image.base && gip<hvm_p->layered_hv_image.base+hvm_p->layered_hv_image.size)
			{
#if defined(_hv_type1)
				// FIXME: Translate the GVA in the structure.
				noir_vt_custom_vcpu_p cvcpu=null;
#else
				noir_vt_custom_vcpu_p cvcpu=(noir_vt_custom_vcpu_p)gpr_state->rdx;
#endif
				nvc_vt_initialize_cvm_vmcs(vcpu,cvcpu);
				noir_vt_advance_rip();
			}
			break;
		}
		case noir_vt_run_custom_vcpu:
		{
			// For CVM hypercalls, the caller must be located in Layered Hypervisor.
			if(gip>=hvm_p->layered_hv_image.base && gip<hvm_p->layered_hv_image.base+hvm_p->layered_hv_image.size)
			{
#if defined(_hv_type1)
				// FIXME: Translate the GVA in the structure.
				noir_vt_custom_vcpu_p cvcpu=null;
#else
				noir_vt_custom_vcpu_p cvcpu=(noir_vt_custom_vcpu_p)gpr_state->rdx;
#endif
				noir_vt_advance_rip();
				nvc_vt_switch_to_guest_vcpu(gpr_state,vcpu,cvcpu);
			}
			break;
		}
		case noir_vt_dump_vcpu_vmcs:
		{
			// For CVM hypercalls, the caller must be located in Layered Hypervisor.
			if(gip>=hvm_p->layered_hv_image.base && gip<hvm_p->layered_hv_image.base+hvm_p->layered_hv_image.size)
			{
#if defined(_hv_type1)
				// FIXME: Translate the GVA in the structure.
				noir_vt_custom_vcpu_p cvcpu=null;
#else
				noir_vt_custom_vcpu_p cvcpu=(noir_vt_custom_vcpu_p)gpr_state->rdx;
#endif
				nvc_vt_dump_vcpu_state(cvcpu);
				noir_vt_advance_rip();
			}
			break;
		}
		case noir_vt_set_vcpu_options:
		{
			// For CVM hypercalls, the caller must be located in Layered Hypervisor.
			if(gip>=hvm_p->layered_hv_image.base && gip<hvm_p->layered_hv_image.base+hvm_p->layered_hv_image.size)
			{
#if defined(_hv_type1)
				// FIXME: Translate the GVA in the structure.
				noir_vt_custom_vcpu_p cvcpu=null;
#else
				noir_vt_custom_vcpu_p cvcpu=(noir_vt_custom_vcpu_p)gpr_state->rdx;
#endif
				nvc_vt_set_guest_vcpu_options(vcpu,cvcpu);
				noir_vt_advance_rip();
			}
			break;
		}
		default:
		{
			// Unexpected vmcall occured. This could be possible when NoirVisor is loaded as nested hypervisor.
			break;
		}
	}
}

ulong_ptr static nvc_vt_parse_vmx_pointer(noir_gpr_state_p gpr_state)
{
	ia32_vmexit_instruction_information info;
	ulong_ptr* gpr=(ulong_ptr*)gpr_state;
	long_ptr displacement;		// This is signed.
	ulong_ptr pointer,seg_base;
	u32 seg_base_field=guest_es_base;
	// Load Instruction Information.
	noir_vt_vmread(vmexit_instruction_information,(u32*)&info);
	// Get Displacement
	noir_vt_vmread(vmexit_qualification,(ulong_ptr*)&displacement);
	// Load Base Register.
	pointer=gpr[info.f5.base];
	// Load Index Register.
	pointer+=gpr[info.f5.base]<<info.f5.scaling;
	// Select the Segment.
	seg_base_field+=info.f5.segment<<1;
	// Get the segment base.
	noir_vt_vmread(seg_base_field,&seg_base);
	// Apply segment base.
	pointer+=seg_base;
	// Apply displacement - Final Linear Address.
	pointer+=displacement;
	return pointer;
}

// Expected Exit Reason: 19
// This is VM-Exit of obligation.
void static fastcall nvc_vt_vmclear_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	noir_vt_nested_vcpu_p nested_vcpu=&vcpu->nested_vcpu;
	// Check if Guest is under VMX Operation.
	if(noir_bt(&nested_vcpu->status,noir_nvt_vmxon))
	{
		ulong_ptr pointer=nvc_vt_parse_vmx_pointer(gpr_state);
		u64 vmcs_pa=*(u64*)pointer;
		if(vmcs_pa & 0xfff)
			noir_vt_vmfail(nested_vcpu,vmclear_with_invalid_pa);
		else
		{
			if(vmcs_pa==nested_vcpu->vmxon.phys)
				noir_vt_vmfail(nested_vcpu,vmclear_with_vmxon_region);
			else
			{
				noir_vt_nested_vmcs_header_p header=noir_find_virt_by_phys(vmcs_pa);
				header->clean_fields.active=0;
				header->clean_fields.launched=0;
				if(vmcs_pa==nested_vcpu->vmcs_c.phys)
				{
					nested_vcpu->vmcs_c.phys=maxu64;
					nested_vcpu->vmcs_c.virt=null;
				}
				noir_vt_vmsuccess();
			}
		}
		noir_vt_advance_rip();
		return;
	}
	noir_vt_inject_event(ia32_invalid_opcode,ia32_hardware_exception,false,0,0);
}

// Expected Exit Reason: 21
// This is VM-Exit of obligation.
void static fastcall nvc_vt_vmptrld_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	noir_vt_nested_vcpu_p nested_vcpu=&vcpu->nested_vcpu;
	// Check if Guest is under VMX Operation.
	if(noir_bt(&nested_vcpu->status,noir_nvt_vmxon))
	{
		ulong_ptr pointer=nvc_vt_parse_vmx_pointer(gpr_state);
		u64 vmcs_pa=*(u64*)pointer;
		if(vmcs_pa & 0xfff)
			noir_vt_vmfail(nested_vcpu,vmptrld_with_invalid_pa);
		else
		{
			if(vmcs_pa==nested_vcpu->vmxon.phys)
				noir_vt_vmfail(nested_vcpu,vmptrld_with_vmxon_region);
			else
			{
				noir_vt_nested_vmcs_header_p header=noir_find_virt_by_phys(vmcs_pa);
				u32 true_revision_id=(u32)vcpu->virtual_msr.vmx_msr[0];
				if(header->revision_id!=true_revision_id)
					noir_vt_vmfail(nested_vcpu,vmptrld_with_incorrect_revid);
				else
				{
					header->clean_fields.active=1;
					nested_vcpu->vmcs_c.phys=vmcs_pa;
					nested_vcpu->vmcs_c.virt=(void*)header;
					noir_vt_vmsuccess();
				}
			}
		}
		noir_vt_advance_rip();
		return;
	}
	noir_vt_inject_event(ia32_invalid_opcode,ia32_hardware_exception,false,0,0);
}

// Expected Exit Reason: 22
// This is VM-Exit of obligation.
void static fastcall nvc_vt_vmptrst_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	noir_vt_nested_vcpu_p nested_vcpu=&vcpu->nested_vcpu;
	if(noir_bt(&nested_vcpu->status,noir_nvt_vmxon))
	{
		ulong_ptr pointer=nvc_vt_parse_vmx_pointer(gpr_state);
		*(u64*)pointer=nested_vcpu->vmcs_c.phys;
		noir_vt_advance_rip();
		return;
	}
	noir_vt_inject_event(ia32_invalid_opcode,ia32_hardware_exception,false,0,0);
}

// Expected Exit Reason: 26
// This is VM-Exit of obligation.
void static fastcall nvc_vt_vmxoff_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	noir_vt_nested_vcpu_p nested_vcpu=&vcpu->nested_vcpu;
	// Check if VMXON has been executed. Plus, revoke vmxon.
	if(noir_btr(&nested_vcpu->status,noir_nvt_vmxon))
	{
		// Mark as success operation.
		noir_vt_vmsuccess();
		noir_vt_advance_rip();
	}
	else
	{
		// Inject a #UD exception.
		noir_vt_inject_event(ia32_invalid_opcode,ia32_hardware_exception,false,0,0);
	}
}

// Expected Exit Reason: 27
// This is VM-Exit of obligation.
void static fastcall nvc_vt_vmxon_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	noir_vt_nested_vcpu_p nested_vcpu=&vcpu->nested_vcpu;
	// Check if Nested VMX is enabled.
	if(noir_bt(&nested_vcpu->status,noir_nvt_vmxe))
	{
		// Check if VMXON has been executed.
		if(!noir_bt(&nested_vcpu->status,noir_nvt_vmxon))
		{
			ulong_ptr pointer=nvc_vt_parse_vmx_pointer(gpr_state);
			nv_dprintf("VMXON Region VA: 0x%p!\n",pointer);
			// Get the VMXON Region Physical Address.
			nested_vcpu->vmxon.phys=*(u64*)pointer;
			// Check if page-aligned.
			if((nested_vcpu->vmxon.phys & 0xfff)==0)
			{
				// Get the Virtual Address in System Space.
				nested_vcpu->vmxon.virt=noir_find_virt_by_phys(nested_vcpu->vmxon.phys);
				if(nested_vcpu->vmxon.virt)
				{
					// Get VMX Revision ID.
					u32* revision_id=(u32*)nested_vcpu->vmxon.virt;
					u32 true_revision_id=(u32)vcpu->virtual_msr.vmx_msr[0];
					// Check if Revision ID is correct.
					if(noir_bt(revision_id,31) || *revision_id!=true_revision_id)
						noir_vt_vmfail_invalid();
					else
					{
						// Revision ID is correct. Initialize Nested VMCS.
						nested_vcpu->vmcs_c.virt=null;
						nested_vcpu->vmcs_c.phys=maxu64;
						// Mark that the vCPU is after vmxon.
						noir_bts(&nested_vcpu->status,noir_nvt_vmxon);
						noir_vt_vmsuccess();
						// As we obtained the VMXON region pointer, we are
						// totally free to use the 4KB in guest.
					}
					return;
				}
			}
			noir_vt_vmfail_invalid();
		}
		else
		{
			// Mark as failure.
			noir_vt_vmfail(&vcpu->nested_vcpu,vmxon_in_vmx_root);
		}
		// We don't have to check whether vCPU is under VMX Non-Root Operation.
		// There will be another handler regarding the VM-Exit.
		noir_vt_advance_rip();
	}
	else
	{
		// Inject a #UD exception.
		noir_vt_inject_event(ia32_invalid_opcode,ia32_hardware_exception,false,0,0);
	}
}

// Expected Exit Reason: 28
// Filter unwanted behaviors.
// Besides, we need to virtualize CR4.VMXE and CR4.SMXE here.
void static fastcall nvc_vt_cr_access_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	// On default NoirVisor's setting of VMCS, this handler only traps writes to CR0 and CR4.
	ia32_cr_access_qualification info;
	bool advance_ip=true;
	noir_vt_vmread(guest_rsp,&gpr_state->rsp);
	noir_vt_vmread(vmexit_qualification,(ulong_ptr*)&info);
	switch(info.access_type)
	{
		case 0:
		{
			// Write to Control Register is intercepted!
			switch(info.cr_num)
			{
				ia32_vmentry_interruption_information_field fault_info;
				case 0:
				{
					ulong_ptr data=((ulong_ptr*)gpr_state)[info.gpr_num];
					ulong_ptr gcr0,cr0x;
					noir_vt_vmread(guest_cr0,&gcr0);
					// Use exclusive-or to check if CR0.CD bit is being changed.
					cr0x=gcr0^data;
					if(noir_bt((u32*)&cr0x,ia32_cr0_cd))
					{
						// The CR0.CD bit is being changed.
						if(noir_bt((u32*)&gcr0,ia32_cr0_cd)==0)
						{
							invept_descriptor ied;
							// Reset EPT entries.
							nvc_ept_update_by_mtrr(vcpu->ept_manager);
							// Flush EPT TLB due to the update.
							ied.eptp=vcpu->ept_manager->eptp.phys.value;
							ied.reserved=0;
							noir_vt_invept(ept_single_invd,&ied);
						}
					}
					// Finally, write to Guest CR0 field and CR0 Read-Shadow field.
					noir_vt_vmwrite(guest_cr0,data);
					noir_vt_vmwrite(cr0_read_shadow,data);
					// Set up fallback entries if failure on VM-Entry.
					vcpu->fallback.valid=true;
					vcpu->fallback.encoding=guest_cr0;
					vcpu->fallback.value=gcr0;
					noir_vt_vmread(guest_rip,&vcpu->fallback.rip);
					// Failure to write CR0 triggers #GP(0) fault.
					fault_info.value=0;
					fault_info.vector=ia32_general_protection;
					fault_info.type=ia32_hardware_exception;
					fault_info.deliver=true;
					fault_info.valid=true;
					vcpu->fallback.fault_info=fault_info.value;
					vcpu->fallback.error_code=0;
					break;
				}
				case 4:
				{
					ulong_ptr data=((ulong_ptr*)gpr_state)[info.gpr_num];
					ulong_ptr gcr4;
					noir_vt_vmread(guest_cr4,&gcr4);
					// If Guest attempts to write CR4.VMXE, mark vCPU as Nested-VMX-Enabled(Disabled).
					if(noir_bt((u32*)&data,ia32_cr4_vmxe))		// Enable VMX.
						noir_bts(&vcpu->nested_vcpu.status,noir_nvt_vmxe);
					else
					{
						// Disable VMX.
						if(noir_bt(&vcpu->nested_vcpu.status,noir_nvt_vmxon))
							noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
						else
							noir_btr(&vcpu->nested_vcpu.status,noir_nvt_vmxe);
					}
					// Finally, write to Guest CR4 field and CR4 Read-Shadow field.
					noir_vt_vmwrite(guest_cr4,data|ia32_cr4_vmxe_bit);
					noir_vt_vmwrite(cr4_read_shadow,data);
					// Set up fallback entries if failure on VM-Entry.
					vcpu->fallback.valid=true;
					vcpu->fallback.encoding=guest_cr4;
					vcpu->fallback.value=gcr4;
					noir_vt_vmread(guest_rip,&vcpu->fallback.rip);
					// Failure to write CR0 triggers #GP(0) fault.
					fault_info.value=0;
					fault_info.vector=ia32_general_protection;
					fault_info.type=ia32_hardware_exception;
					fault_info.deliver=true;
					fault_info.valid=true;
					vcpu->fallback.fault_info=fault_info.value;
					vcpu->fallback.error_code=0;
					break;
				}
				default:
				{
					nv_dprintf("Unexpected write to Control Register is intercepted!\n");
					break;
				}
			}
			break;
		}
		case 1:
		{
			// Read from Control Register is intercepted!
			nv_dprintf("Unexpected read from Control Register is intercepted!\n");
			break;
		}
		case 2:
		{
			// The clts instruction is intercepted!
			// By NoirVisor's default setting of VMCS, we won't go here.
			nv_dprintf("The clts instruction is intercepted! Check your CR0 Guest-Host Mask!\n");
			break;
		}
		case 3:
		{
			// Lmsw instruction is intercepted!
			nv_dprintf("The lmsw instruction is intercepted! Check your CR0 Guest-Host Mask!\n");
			break;
		}
	}
	if(advance_ip)noir_vt_advance_rip();
}

// Expected Exit Reason: 31
// This is the key feature of MSR-Hook Hiding.
void static fastcall nvc_vt_rdmsr_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	bool advance=true;
	u32 index=(u32)gpr_state->rcx;
	large_integer val;
	// Expected Case: Microsoft Synthetic MSR
	if(noir_is_synthetic_msr(index))
	{
		if(hvm_p->options.tlfs_passthrough)
			val.value=noir_rdmsr(index);
		else if(hvm_p->options.cpuid_hv_presence)
			val.value=nvc_mshv_rdmsr_handler(&vcpu->mshvcpu,index);
		else
			noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
	}
	else
	{
		// Expected Case: Query VMX MSR.
		switch(index)
		{
			case ia32_vmx_basic:
			case ia32_vmx_pinbased_ctrl:
			case ia32_vmx_priproc_ctrl:
			case ia32_vmx_exit_ctrl:
			case ia32_vmx_entry_ctrl:
			case ia32_vmx_misc:
			case ia32_vmx_cr0_fixed0:
			case ia32_vmx_cr0_fixed1:
			case ia32_vmx_cr4_fixed0:
			case ia32_vmx_cr4_fixed1:
			case ia32_vmx_vmcs_enum:
			case ia32_vmx_2ndproc_ctrl:
			case ia32_vmx_true_pinbased_ctrl:
			case ia32_vmx_true_priproc_ctrl:
			case ia32_vmx_true_exit_ctrl:
			case ia32_vmx_true_entry_ctrl:
			case ia32_vmx_vmfunc:
			{
				/*
				  u32 cur_proc=noir_get_current_processor();
				  noir_vt_vcpu_p vcpu=&hvm_p->virtual_cpu[cur_proc];
				  val.value=vcpu->nested_vcpu.vmx_msr[index-ia32_vmx_basic];
				*/
				// NoirVisor doesn't support Nested Intel VT-x right now.
				// Reading VMX Capability MSRs would cause #GP exception.
				noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
				advance=false;	// For exception, rip does not advance.
			}
			// Expected Case: Check MSR Hook.
#if !defined(_hv_type1)
#if defined(_amd64)
			case ia32_lstar:
			{
				ia32_vmx_msr_auto_p exit_store=(ia32_vmx_msr_auto_p)((ulong_ptr)vcpu->msr_auto.virt+0x800);
				if(exit_store->data==(u64)noir_system_call)
					val.value=orig_system_call;
				else
					val.value=exit_store->data;
				break;
			}
#else
			case ia32_sysenter_eip:
			{
				val.value=vcpu->virtual_msr.sysenter_eip;
				break;
			}
#endif
#endif
			default:
			{
				ulong_ptr gip;
				noir_vt_vmread(guest_rip,&gip);
				noir_int3();
				nv_dprintf("Unexpected rdmsr is intercepted! Index=0x%X, rip=0x%p\n",index,gip);
				val.value=noir_rdmsr(index);
				break;
			}
		}
	}
	// Put into GPRs. Clear high 32-bits of each register.
	gpr_state->rax=(ulong_ptr)val.low;
	gpr_state->rdx=(ulong_ptr)val.high;
	if(advance)noir_vt_advance_rip();
}

// Expected Exit Reason: 32
void static fastcall nvc_vt_wrmsr_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	u32 index=(u32)gpr_state->rcx;
	large_integer val;
	val.low=(u32)gpr_state->rax;
	val.high=(u32)gpr_state->rdx;
	// Expected Case: Microsoft Synthetic MSR
	if(noir_is_synthetic_msr(index))
	{
		if(hvm_p->options.tlfs_passthrough)
			noir_wrmsr(index,val.value);
		else if(hvm_p->options.cpuid_hv_presence)
			nvc_mshv_wrmsr_handler(&vcpu->mshvcpu,index,val.value);
		else
			noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
	}
	else
	{
		switch(index)
		{
			case ia32_bios_updt_trig:
			{
				// To update processor microcode, a virtual address is supplied to the MSR.
#if defined(_hv_type1)
				// If NoirVisor is running as a Type-I hypervisor, the GVA should be
				// translated to HPA, and copy the microcode data to host page-by-page.
				// Another alternative approach is to forbid any microcode update.
#else
				// If NoirVisor is running as a Type-II hypervisor,
				// there is nothing really should be going on here,
				// in that we may simply throw the value to MSR.
				noir_wrmsr(ia32_bios_updt_trig,val.value);
#endif
				break;
			}
			case ia32_mtrr_phys_base0:
			case ia32_mtrr_phys_mask0:
			case ia32_mtrr_phys_base1:
			case ia32_mtrr_phys_mask1:
			case ia32_mtrr_phys_base2:
			case ia32_mtrr_phys_mask2:
			case ia32_mtrr_phys_base3:
			case ia32_mtrr_phys_mask3:
			case ia32_mtrr_phys_base4:
			case ia32_mtrr_phys_mask4:
			case ia32_mtrr_phys_base5:
			case ia32_mtrr_phys_mask5:
			case ia32_mtrr_phys_base6:
			case ia32_mtrr_phys_mask6:
			case ia32_mtrr_phys_base7:
			case ia32_mtrr_phys_mask7:
			case ia32_mtrr_phys_base8:
			case ia32_mtrr_phys_mask8:
			case ia32_mtrr_phys_base9:
			case ia32_mtrr_phys_mask9:
			case ia32_mtrr_fix64k_00000:
			case ia32_mtrr_fix16k_80000:
			case ia32_mtrr_fix16k_a0000:
			case ia32_mtrr_fix4k_c0000:
			case ia32_mtrr_fix4k_c8000:
			case ia32_mtrr_fix4k_d0000:
			case ia32_mtrr_fix4k_d8000:
			case ia32_mtrr_fix4k_e0000:
			case ia32_mtrr_fix4k_e8000:
			case ia32_mtrr_fix4k_f0000:
			case ia32_mtrr_fix4k_f8000:
			case ia32_mtrr_def_type:
			{
				ulong_ptr gcr0;
				// Writes to MTRRs are intercepted.
				// Pass the value to the real MTRR.
				noir_wrmsr(index,val.value);
				// If CR0.CD is cleared, we should re-emulate MTRRs.
				// Otherwise, simply mark the MTRR is dirty.
				// Re-emulation would be done when CR0.CD is reset.
				noir_vt_vmread(guest_cr0,&gcr0);
				if(noir_bt((u32*)&gcr0,ia32_cr0_cd))
					vcpu->mtrr_dirty=1;
				else
				{
					invept_descriptor ied;
					// Reset EPT entries.
					nvc_ept_update_by_mtrr(vcpu->ept_manager);
					// Flush EPT TLB due to the update.
					ied.eptp=vcpu->ept_manager->eptp.phys.value;
					ied.reserved=0;
					noir_vt_invept(ept_single_invd,&ied);
				}
				break;
			}
#if !defined(_hv_type1)
#if defined(_amd64)
			case ia32_lstar:
			{
				ia32_vmx_msr_auto_p entry_load=(ia32_vmx_msr_auto_p)((ulong_ptr)vcpu->msr_auto.virt+0);
				nv_dprintf("Write to IA32-LSTAR MSR is intercepted!\n");
				vcpu->virtual_msr.lstar=val.value;
				if(val.value==orig_system_call)
					entry_load->data=(u64)noir_system_call;
				else
					entry_load->data=val.value;
				break;
			}
#else
			case ia32_sysenter_eip:
			{
				vcpu->virtual_msr.sysenter_eip=val.value;
				break;
			}
#endif
#endif
			default:
			{
				nv_dprintf("Unexpected wrmsr is intercepted! Index=0x%X\t Value=0x%08X`%08X\n",index,val.low,val.high);
				break;
			}
		}
		noir_wrmsr(index,val.value);
	}
	noir_vt_advance_rip();
}

// Expected Exit Reason: 33
// This is VM-Exit of obligation.
// You may want to debug your code if this handler is invoked.
void static fastcall nvc_vt_invalid_guest_state(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	if(vcpu->fallback.valid)
	{
		// It is possible to fallback.
		nv_dprintf("VM-Entry failed. Fallback is available. Restoring...\n");
		noir_vt_vmwrite64(vcpu->fallback.encoding,vcpu->fallback.value);
		noir_vt_vmwrite(guest_rip,vcpu->fallback.rip);
		// Fallback comes with event injection...
		noir_vt_vmwrite(vmentry_interruption_information_field,vcpu->fallback.fault_info);
		noir_vt_vmwrite(vmentry_exception_error_code,vcpu->fallback.error_code);
	}
	else
	{
		// There is no falling back, so check the guest state..
		nv_dprintf("VM-Entry failed with no available fallbacks! Check the Guest-State in VMCS!\n");
		nvc_vt_dump_vmcs_guest_state();
	}
}

// Expected Exit Reason: 34
// This is VM-Exit of obligation.
// You may want to debug your code if this handler is invoked.
void static fastcall nvc_vt_invalid_msr_loading(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	nv_dprintf("VM-Entry failed! Check the MSR-Auto list in VMCS!\n");
}

// Expected Exit Reason: 37
void static fastcall nvc_vt_monitor_trap_flag_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
#if !defined(_hv_type1)
	// Current implementation of NoirVisor would only use MTF to step over
	// instructions that accesses data in the same page to the instruction.
	noir_ept_manager_p eptm=vcpu->ept_manager;
	noir_hook_page_p nhp=&eptm->hook_pages[eptm->pending_hook_index];
	ia32_ept_pte_p pte_p=(ia32_ept_pte_p)nhp->pte_descriptor;
	invept_descriptor ied;
	ia32_vmx_priproc_controls proc_ctrl;
	// Revoke the read/write accesses and map to the hooked page.
	pte_p->read=pte_p->write=0;
	pte_p->page_offset=nhp->hook.phys>>page_shift;
	// Invalidate the TLBs in EPT.
	ied.reserved=0;
	ied.eptp=eptm->eptp.phys.value;
	noir_vt_invept(ept_single_invd,&ied);
	// We no longer have to step over the instructions. Disable MTF.
	noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl);
	proc_ctrl.monitor_trap_flag=0;
	noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl.value);
#endif
}

// Expected Exit Reason: 46
void static fastcall nvc_vt_access_gdtr_idtr_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	ia32_vmexit_instruction_information exit_info;
	long_ptr displacement;
	ulong_ptr *gpr_array=(ulong_ptr*)gpr_state;
	ulong_ptr pointer=0,seg_base=0,gcr0;
	vmx_segment_access_right cs_attrib;
	noir_vt_vmread(guest_cs_access_rights,&cs_attrib.value);
	noir_vt_vmread(vmexit_instruction_information,&exit_info.value);
	// Forge the pointer.
	noir_vt_vmread(vmexit_qualification,&displacement);
	if(!exit_info.f2.base_invalid)
	{
		ulong_ptr base;
		// Note: The rsp register is saved in VMCS.
		if(exit_info.f2.base==4)
			noir_vt_vmread(guest_rsp,&base);
		else
			base=gpr_array[exit_info.f2.base];
		pointer+=base;
	}
	// Note: The rsp register is impossible to be the index register of a memory operand.
	if(!exit_info.f2.index_invalid)
		pointer+=gpr_array[exit_info.f2.index]<<gpr_array[exit_info.f2.scaling];
	// Apply the segment base.
	noir_vt_vmread(guest_cr0,&gcr0);
#if defined(_hv_type1)
	if(gcr0 & ia32_cr0_pe_bit)
#endif
		noir_vt_vmread(guest_es_base+(exit_info.f2.segment<<1),&seg_base);
#if defined(_hv_type1)
	else
	{
		// In Real-Mode, segment base is selector being shifted left for four bits.
		noir_vt_vmread(guest_es_selector+(exit_info.f2.segment<<1),&seg_base);
		seg_base<<=4;
	}
#endif
	pointer+=seg_base;
	// Truncate the pointer if needed.
	if(exit_info.f2.address_size==1)
		pointer&=0xffffffff;
	else if(exit_info.f2.address_size==0)
		pointer&=0xffff;
	// FIXME: Check if the forged memory address is valid.
	// Pointer is completely forged. Perform emulation.
	if(exit_info.f2.is_loading)
	{
		// This is a load access.
		u16 limit=*(u16*)pointer;
		ulong_ptr base;
		if(cs_attrib.long_mode)
			base=(ulong_ptr)(*(u64*)(pointer+2));
		else
			base=(ulong_ptr)(*(u32*)(pointer+2));
		if(exit_info.f2.is_idtr_access)
		{
			noir_vt_vmwrite(guest_idtr_limit,limit);
			noir_vt_vmwrite(guest_idtr_base,base);
		}
		else
		{
			noir_vt_vmwrite(guest_gdtr_limit,limit);
			noir_vt_vmwrite(guest_gdtr_base,base);
		}
	}
	else
	{
		// This is a store access. Validate the exiting CPL.
		// According to Intel SDM, SS.DPL must be the CPL.
		u16 limit;
		u64 base;
		bool prevention=false;
		vmx_segment_access_right ss_attrib;
		ulong_ptr gcr3;
		noir_vt_vmread(guest_cr3,&gcr3);
		// Unlike AMD-V, interception does not mean prevention is active.
		if(exit_info.f2.is_idtr_access)
			prevention|=noir_bt((u32*)&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sidt);
		else
			prevention|=noir_bt((u32*)&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sgdt);
		noir_vt_vmread(guest_ss_access_rights,&ss_attrib.value);
		if(ss_attrib.dpl>0 && prevention)
		{
			limit=0x2333;
			base=0x6666666666666666;
		}
		else
		{
			if(exit_info.f2.is_idtr_access)
			{
				noir_vt_vmread(guest_idtr_limit,&limit);
				noir_vt_vmread(guest_idtr_base,&base);
			}
			else
			{
				noir_vt_vmread(guest_gdtr_limit,&limit);
				noir_vt_vmread(guest_gdtr_base,&base);
			}
		}
		noir_writecr3(gcr3);
		if(cs_attrib.long_mode)
			*(u64*)(pointer+2)=base;
		else
			*(u32*)(pointer+2)=(u32)base;
		*(u16*)pointer=limit;
	}
	noir_vt_advance_rip();
}

// Expected Exit Reason: 47
void static fastcall nvc_vt_access_ldtr_tr_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	ia32_vmexit_instruction_information exit_info;
	long_ptr displacement;
	// ulong_ptr *gpr_array=(ulong_ptr*)gpr_state;
	ulong_ptr pointer=0,seg_base=0,gdt_base,gcr0;
	vmx_segment_access_right cs_attrib;
	noir_vt_vmread(guest_cs_access_rights,&cs_attrib.value);
	noir_vt_vmread(vmexit_instruction_information,&exit_info.value);
	noir_vt_vmread(vmexit_qualification,&displacement);
	noir_vt_vmread(guest_cr0,&gcr0);
	noir_vt_vmread(guest_gdtr_base,&gdt_base);
	unref_var(pointer);
	unref_var(seg_base);
	// Accessing LDTR and TR does not necessarily reference memory.
	if(!exit_info.f3.use_register)
	{
		;
	}
	noir_vt_advance_rip();
}

// Expected Exit Reason: 48
// This is VM-Exit of obligation on EPT.
// Specifically, this handler is invoked on EPT-based stealth hook feature.
// Critical Hypervisor Protection and Real-Time Code Integrity features
// may invoke this handler. Simply advance the rip for these circumstances.
void static fastcall nvc_vt_ept_violation_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	bool advance=true;
#if !defined(_hv_type1)
	noir_ept_manager_p eptm=vcpu->ept_manager;
	i32 lo=0,hi=noir_hook_pages_count;
	u64 gpa;
	noir_vt_vmread64(guest_physical_address,&gpa);
	// We previously sorted the list.
	// Use binary search to reduce time complexity.
	while(hi>=lo)
	{
		i32 mid=(lo+hi)>>1;
		noir_hook_page_p nhp=&eptm->hook_pages[mid];
		if(gpa>=nhp->orig.phys+page_size)
			lo=mid+1;
		else if(gpa<nhp->orig.phys)
			hi=mid-1;
		else
		{
			ia32_ept_violation_qualification info;
			// The violated page is found. Perform page-substitution
			ia32_ept_pte_p pte_p=(ia32_ept_pte_p)nhp->pte_descriptor;
			noir_vt_vmread(vmexit_qualification,(ulong_ptr*)&info);
			if(info.read || info.write)
			{
				// If the access is read or write, we should check if
				// the instruction pointer is located in the same page
				// to the data to be referenced.
				ulong_ptr gip;
				noir_vt_vmread(guest_rip,&gip);
				if(page_base(gip)==(ulong_ptr)nhp->orig.virt)
				{
					ia32_vmx_priproc_controls proc_ctrl;
					// They are in the same page. Grant all permissions
					// and substitute the page to be original page.
					pte_p->read=pte_p->write=pte_p->execute=1;
					// Enable MTF at the same time.
					noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl);
					proc_ctrl.monitor_trap_flag=1;
					noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl.value);
					// Indicate which hook page is pending to be stepped over.
					eptm->pending_hook_index=mid;
				}
				else
				{
					// They are in different pages. We may grant the
					// read/write permission but revoke execute permission
					// and substitute the page to be original page.
					pte_p->read=1;
					pte_p->write=1;
					pte_p->execute=0;
				}
				pte_p->page_offset=nhp->orig.phys>>page_shift;
			}
			else if(info.execute)
			{
				// If the access is execute, we grant
				// execute permission but revoke read/write permission
				// and substitute the page to be hooked page
				pte_p->read=0;
				pte_p->write=0;
				pte_p->execute=1;
				pte_p->page_offset=nhp->hook.phys>>page_shift;
			}
			advance=false;
			// According to Intel SDM, an EPT Violation invalidates any guest-physical mappings (associated
			// with the current EP4TA) that would be used to translate the GPA that caused EPT Violation.
			// In other words, there is no need to invalidate the TLB via invept instruction.
			break;
		}
	}
#endif
	if(advance)noir_vt_advance_rip();
}

// Expected Exit Reason: 49
// This is VM-Exit of obligation on EPT.
// You may want to debug your code if this handler is invoked.
void static fastcall nvc_vt_ept_misconfig_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	u64 gpa;
	noir_vt_vmread64(guest_physical_address,&gpa);
	nv_panicf("EPT Misconfiguration Occured! GPA=0x%llX\n",gpa);
	noir_int3();
}

// Expected Exit Reason: 55
// This is VM-Exit of obligation.
void static fastcall nvc_vt_xsetbv_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	u64 value=0;
	u32 index=(u32)gpr_state->rcx;
	bool gp_exception=false;
	switch(index)
	{
		case 0:
		{
			ia32_xcr0 xcr0;
			u32 a=hvm_p->xfeat.support_mask.low;
			u32 d=hvm_p->xfeat.support_mask.high;
			xcr0.lo=(u32)gpr_state->rax;
			xcr0.hi=(u32)gpr_state->rdx;
			// IA-32 architecture bans x87 being disabled.
			if(xcr0.x87==0)gp_exception=true;
			// SSE is necessary condition of AVX.
			if(xcr0.sse==0 && xcr0.avx!=0)gp_exception=true;
			if((xcr0.lo&a)!=a)gp_exception=true;
			if((xcr0.hi&d)!=d)gp_exception=true;
			value=xcr0.value;
			break;
		}
		default:
		{
			// Unknown XCR index goes to #GP exception.
			gp_exception=true;
			break;
		}
	}
	if(gp_exception)	// Exception induced by xsetbv has error code zero pushed onto stack.
		noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
	else
	{
		// Everything is fine.
		noir_xsetbv(index,value);
		noir_vt_advance_rip();
	}
}

// It is important that this function uses fastcall convention.
void fastcall nvc_vt_exit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	noir_vt_initial_stack_p loader_stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	u64 vmcs_phys;
	u32 exit_reason;
	noir_vt_vmread(vmexit_reason,&exit_reason);
	exit_reason&=0xFFFF;
	loader_stack->flags.initial_vmcs=false;
	noir_vt_vmptrst(&vmcs_phys);
	// Confirm which vCPU is exiting so that the correct handler is to be invoked...
	if(likely(vmcs_phys==vcpu->vmcs.phys))
	{
		if(exit_reason<vmx_maximum_exit_reason)
			vt_exit_handlers[exit_reason](gpr_state,vcpu);
		else
			nvc_vt_default_handler(gpr_state,vcpu);
	}
	else if(vmcs_phys==loader_stack->custom_vcpu->vmcs.phys)
	{
		noir_vt_custom_vcpu_p cvcpu=loader_stack->custom_vcpu;
		if(exit_reason<vmx_maximum_exit_reason)
			vt_cvexit_handlers[exit_reason](gpr_state,vcpu,cvcpu);
		else
			nvc_vt_default_cvexit_handler(gpr_state,vcpu,cvcpu);
	}
	else
	{
		nv_dprintf("VM-Exit occured from an unexpected source! VMCS Physical Address=0x%llX, Exit Reason=%u\n",vmcs_phys,exit_reason);
		noir_int3();
	}
	// Guest RIP is supposed to be advanced in specific handlers, not here.
	// Do not execute vmresume here. It will be done as this function returns.
}

void fastcall nvc_vt_resume_failure(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,u8 vmx_status)
{
	noir_vt_initial_stack_p loader_stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	// This function could be called by virtue of bugs in NoirVisor CVM.
	u64 vmcs_phys;
	noir_vt_vmptrst(&vmcs_phys);
	if(vmcs_phys==vcpu->vmcs.phys)
	{
		nv_panicf("The VM-Entry failed on Resume of Host vCPU!\n");
		switch(vmx_status)
		{
			case vmx_fail_valid:
			{
				u32 err_code;
				noir_vt_vmread(vm_instruction_error,&err_code);
				nv_panicf("Error Code of VM-Entry Failure: %u\n",err_code);
				nv_panicf("Explanation to VM-Entry Failure: %s\n",vt_error_message[err_code]);
				break;
			}
			case vmx_fail_invalid:
			{
				nv_panicf("Failed to VM-Entry on Resume because no active VMCS is loaded!\n");
				break;
			}
			default:
			{
				nv_panicf("Unknown VM-Entry Failure status on Resume! Status=%u\n",vmx_status);
				break;
			}
		}
		// Break at last to let the debug personnel know the failure.
		noir_int3();
		// Call the panic function to stop the system.
	}
	else if(vmcs_phys==loader_stack->custom_vcpu->vmcs.phys)
	{
		noir_vt_custom_vcpu_p cvcpu=loader_stack->custom_vcpu;
		nv_dprintf("The VM-Entry failed on Running CVM Guest vCPU!\n");
		switch(vmx_status)
		{
			case vmx_fail_valid:
			{
				u32 err_code;
				noir_vt_vmread(vm_instruction_error,&err_code);
				nv_panicf("Error Code of VM-Entry Failure: %u\n",err_code);
				nv_panicf("Explanation to VM-Entry Failure: %s\n",vt_error_message[err_code]);
				nvc_vt_dump_vmcs_guest_state();
				break;
			}
			case vmx_fail_invalid:
			{
				nv_panicf("Failed to VM-Entry on Resume because no active VMCS is loaded!\n");
				break;
			}
			default:
			{
				nv_panicf("Unknown VM-Entry Failure status on Resume! Status=%u\n",vmx_status);
				break;
			}
		}
		// Indicate there is a bug while running this vCPU.
		cvcpu->header.exit_context.intercept_code=cv_scheduler_bug;
		// Switch to Host Context.
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void nvc_vt_inject_nmi_to_subverted_host()
{
	noir_vt_vcpu_p vcpu=(noir_vt_vcpu_p)noir_rdvcpuptr(field_offset(noir_vt_vcpu,self));
	noir_vt_initial_stack_p loader_stack=(noir_vt_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_vt_initial_stack));
	u64 vmcs_phys;
	// Save the previous VMCS pointer.
	noir_vt_vmptrst(&vmcs_phys);
	// Load the VMCS of subverted host.
	noir_vt_vmptrld(&vcpu->vmcs.phys);
	// Inject the NMI.
	// FIXME: Are there unresolved NMIs?
	noir_vt_inject_event(ia32_nmi_interrupt,ia32_non_maskable_interrupt,false,0,0);
	// Finally, load the previous VMCS.
	noir_vt_vmptrld(&vmcs_phys);
}

void nvc_vt_reconfigure_npiep_interceptions(noir_vt_vcpu_p vcpu)
{
	ulong_ptr gcr4;
	ia32_vmx_2ndproc_controls proc_ctrl2;
	noir_vt_vmread(guest_cr4,&gcr4);
	noir_vt_vmread(secondary_processor_based_vm_execution_controls,&proc_ctrl2.value);
	// If the UMIP is to be enabled, disable descriptor-table exiting.
	// If no NPIEP preventions are set, disable descriptor-table exiting.
	// Otherwise, enable descriptor-table exiting.
	if(noir_bt((u32*)&gcr4,ia32_cr4_umip) || ((vcpu->mshvcpu.npiep_config & noir_mshv_npiep_prevent_all)==0))
		proc_ctrl2.descriptor_table_exiting=0;
	else
		proc_ctrl2.descriptor_table_exiting=1;
	noir_vt_vmwrite(secondary_processor_based_vm_execution_controls,proc_ctrl2.value);
}

void nvc_vt_set_mshv_handler(bool option)
{
	nvcp_vt_cpuid_handler=option?nvc_vt_cpuid_hvp_handler:nvc_vt_cpuid_hvs_handler;
}