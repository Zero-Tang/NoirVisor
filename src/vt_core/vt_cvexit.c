/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the exit handler of Intel VT-x for Customizable VMs.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_cvexit.c
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

void static nvc_vt_save_generic_cvexit_context(noir_vt_custom_vcpu_p cvcpu)
{
	// Generic vCPU Exit Context
	u32 len;
	ulong_ptr cr0;
	u64 efer;
	ia32_vmx_interruptibility_state int_state;
	vmx_segment_access_right ss_ar;
	// Read from VMCS.
	noir_vt_vmread(vmexit_instruction_length,&len);
	noir_vt_vmread(guest_interruptibility_state,&int_state.value);
	noir_vt_vmread(guest_ss_access_rights,&ss_ar.value);
	noir_vt_vmread(guest_cr0,&cr0);
	noir_vt_vmread(guest_msr_ia32_efer,&efer);
	// Saving states for generic context.
	cvcpu->header.exit_context.vcpu_state.instruction_length=len;
	cvcpu->header.exit_context.vcpu_state.int_shadow=int_state.blocking_by_mov_ss;
	cvcpu->header.exit_context.vcpu_state.pe=noir_bt(&cr0,ia32_cr0_pe);
	cvcpu->header.exit_context.vcpu_state.lm=noir_bt(&efer,ia32_efer_lma);
	cvcpu->header.exit_context.vcpu_state.cpl=ss_ar.dpl;
	// Saving some registers...
	cvcpu->header.exit_context.cs=cvcpu->header.seg.cs;
	cvcpu->header.exit_context.rflags=cvcpu->header.rflags;
	cvcpu->header.exit_context.rip=cvcpu->header.rip;
}

void fastcall nvc_vt_default_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	nv_dprintf("Unexpected VM-Exit is intercepted in VM-Exit for CVM!\n");
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_scheduler_bug;
}

// Expected Exit Reason: 0
void static fastcall nvc_vt_exception_nmi_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	ia32_vmexit_interruption_information_field exit_int_info;
	noir_vt_vmread(vmexit_interruption_information,&exit_int_info);
	switch(exit_int_info.vector)
	{
		case ia32_nmi_interrupt:
		{
			// The NMI must be transferred to the host.
			ia32_vmx_interruptibility_state int_state;
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
			noir_vt_vmread(guest_interruptibility_state,&int_state.value);
			int_state.blocking_by_nmi=true;
			noir_vt_vmwrite(guest_interruptibility_state,int_state.value);
			noir_vt_inject_event(ia32_nmi_interrupt,ia32_non_maskable_interrupt,false,0,0);
			cvcpu->header.exit_context.intercept_code=cv_scheduler_exit;
			break;
		}
		case ia32_machine_check:
		{
			// The #MC exception must be transferred to the host.
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
			noir_vt_inject_event(ia32_machine_check,ia32_hardware_exception,false,0,0);
			cvcpu->header.exit_context.intercept_code=cv_scheduler_exit;
			break;
		}
		case ia32_page_fault:
		{
			noir_vt_vmread(vmexit_qualification,&cvcpu->header.exit_context.exception.pf_addr);
			cvcpu->header.exit_context.exception.fetched_bytes=0;
		}
		default:
		{
			cvcpu->header.exit_context.exception.vector=exit_int_info.vector;
			cvcpu->header.exit_context.exception.ev_valid=exit_int_info.err_code;
			if(exit_int_info.err_code)
				noir_vt_vmread(vmexit_interruption_error_code,&cvcpu->header.exit_context.exception.error_code);
			nvc_vt_save_generic_cvexit_context(cvcpu);
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
			cvcpu->header.exit_context.intercept_code=cv_exception;
			break;
		}
	}
}

// Expected Exit Reason: 1
void static fastcall nvc_vt_extint_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Setting the RFlags.IF should have the external interrupt transferred to the host.
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_scheduler_exit;
}

void static fastcall nvc_vt_trifault_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Triple-Fault is a shutdown condition. Deliver to the host.
	nvc_vt_save_generic_cvexit_context(cvcpu);
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_shutdown_condition;
}

void static fastcall nvc_vt_init_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	;
}

void static fastcall nvc_vt_sipi_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	;
}

void static fastcall nvc_vt_interrupt_window_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// There are two conditions of Interrupt Window:
	// 1. There is a pending external interrupt. Do not return to host.
	// 2. The host specifies interception for interrupt window. Return to host.
	if(cvcpu->header.injected_event.attributes.valid && cvcpu->header.injected_event.attributes.type==ia32_external_interrupt && cvcpu->header.injected_event.attributes.priority>(cvcpu->header.crs.cr8&0xf))
		noir_vt_inject_event(cvcpu->header.injected_event.attributes.vector,ia32_external_interrupt,cvcpu->header.injected_event.attributes.ec_valid,0,cvcpu->header.injected_event.error_code);		// Inject the pending interrupt.
	else
	{
		ia32_vmx_priproc_controls proc_ctrl1;
		// Cancel the interception of interrupt window.
		noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl1.value);
		proc_ctrl1.interrupt_window_exiting=false;
		noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl1.value);
		// If user hypervisor specifies intercepting interrupt windows, do it here.
		if(cvcpu->header.vcpu_options.intercept_interrupt_window)
		{
			nvc_vt_save_generic_cvexit_context(cvcpu);
			cvcpu->header.exit_context.intercept_code=cv_interrupt_window;
			cvcpu->header.exit_context.interrupt_window.nmi=false;
			cvcpu->header.exit_context.interrupt_window.iret_passed=false;
			cvcpu->header.exit_context.interrupt_window.reserved=0;
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
		}
	}
}

void static fastcall nvc_vt_nmi_window_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// There are two conditions of Interrupt Window:
	// 1. There is a pending NMI. Do not return to host.
	// 2. The host specifies interception for interrupt window. Return to host.
	if(cvcpu->header.injected_event.attributes.valid && cvcpu->header.injected_event.attributes.type==ia32_non_maskable_interrupt)
		noir_vt_inject_event(ia32_nmi_interrupt,ia32_non_maskable_interrupt,false,0,0);
	else
	{
		// Cancel the interception of NMI-window.
		ia32_vmx_priproc_controls proc_ctrl1;
		// Cancel the interception of interrupt window.
		noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl1.value);
		proc_ctrl1.nmi_window_exiting=false;
		noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl1.value);
		// If user hypervisor specifies intercepting NMI-windows, do it here.
		if(cvcpu->header.vcpu_options.intercept_nmi_window)
		{
			nvc_vt_save_generic_cvexit_context(cvcpu);
			cvcpu->header.exit_context.intercept_code=cv_interrupt_window;
			cvcpu->header.exit_context.interrupt_window.nmi=true;
			cvcpu->header.exit_context.interrupt_window.iret_passed=true;
			cvcpu->header.exit_context.interrupt_window.reserved=0;
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
		}
	}
}

void static fastcall nvc_vt_task_switch_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// FIXME: Implement Task-Switch Emulation.
	// Current implementation will force a VM-Exit.
	ia32_task_switch_qualification info;
	noir_vt_vmread(vmexit_qualification,&info.value);
	nvc_vt_save_generic_cvexit_context(cvcpu);
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_task_switch;
	cvcpu->header.exit_context.task_switch.task_selector=(u16)info.tss_selector;
	cvcpu->header.exit_context.task_switch.initiator_id=(u16)info.ts_source;
}

void static fastcall nvc_vt_cpuid_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	if(cvcpu->header.vcpu_options.intercept_cpuid)
	{
		// Switch to subverted host in order to handle the cpuid instruction.
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
		cvcpu->header.exit_context.intercept_code=cv_cpuid_instruction;
		cvcpu->header.exit_context.cpuid.leaf.a=(u32)cvcpu->header.gpr.rax;
		cvcpu->header.exit_context.cpuid.leaf.c=(u32)cvcpu->header.gpr.rcx;
	}
	else
	{
		// NoirVisor will be handling CVM's CPUID Interception.
		u32 leaf=(u32)gpr_state->rax,subleaf=(u32)gpr_state->rcx;
		u32 leaf_class=noir_cpuid_class(leaf);
		noir_cpuid_general_info info;
		if(leaf_class==hvm_leaf_index)
		{
			// The first two fields will be compliant with Microsoft Hypervisor Top-Level Functionality Specification
			// even though NoirVisor CVM is running with different set of Hypervisor functionalities.
			switch(leaf)
			{
				case ncvm_cpuid_leaf_range_and_vendor_string:
				{
					info.eax=ncvm_cpuid_leaf_limit;					// NoirVisor CVM CPUID Leaf Limit.
					noir_movsb(&info.ebx,"NoirVisor ZT",12);		// The Vendor String is "NoirVisor ZT"
					break;
				}
				case ncvm_cpuid_vendor_neutral_interface_id:
				{
					noir_movsb(&info.eax,"Nv#1",4);		// Interface Signature is "Nv#1". Indicate Non-Compliance to MSHV-TLFS.
					info.eax=info.ebx=info.ecx=0;		// Clear the Reserved CPUID fields.
					break;
				}
				default:
				{
					noir_movsd((u32*)&info,0,4);
					break;
				}
			}
		}
		else
		{
			noir_cpuid(leaf,subleaf,&info.eax,&info.ebx,&info.ecx,&info.edx);
			switch(leaf)
			{
				case ia32_cpuid_std_proc_feature:
				{
					// Indicate Hypervisor Presence.
					noir_bts(&info.ecx,ia32_cpuid_hv_presence);
					// Indicate no support to Intel VT-x.
					noir_btr(&info.ecx,ia32_cpuid_vmx);
					break;
				}
			}
		}
		noir_vt_advance_rip();
	}
}

void static fastcall nvc_vt_hlt_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// The hlt instruction halts the execution of the processor.
	// Schedule the host to the processor.
	nvc_vt_save_generic_cvexit_context(cvcpu);
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_hlt_instruction;
}

void static fastcall nvc_vt_invd_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// The invd instruction would corrupt the cache globally and it thereby must be intercepted.
	// Execute wbinvd to protect global cache.
	noir_wbinvd();
	noir_vt_advance_rip();
}

void static fastcall nvc_vt_vmcall_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// The Guest invoked a hypercall. Deliver to the subverted host.
	nvc_vt_save_generic_cvexit_context(cvcpu);
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_hypercall;
}

void static fastcall nvc_vt_vmclear_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmlaunch_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmptrld_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmptrst_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmread_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmresume_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmwrite_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmxoff_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_vmxon_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_cr_access_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	ulong_ptr* gpr_array=(ulong_ptr*)gpr_state;
	ia32_cr_access_qualification info;
	noir_vt_vmread(vmexit_qualification,&info.value);
	switch(info.access_type)
	{
		case 0:
		{
			// Write to Control Registers is intercepted...
			switch(info.cr_num)
			{
				case 3:
				{
					// Write to CR3 is intercepted...
					if(cvcpu->header.vcpu_options.intercept_cr3 && info.cr_num==3)
					{
						nvc_vt_save_generic_cvexit_context(cvcpu);
						nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
						cvcpu->header.exit_context.intercept_code=cv_cr_access;
						cvcpu->header.exit_context.cr_access.cr_number=3;
						cvcpu->header.exit_context.cr_access.gpr_number=(u32)info.gpr_num;
						cvcpu->header.exit_context.cr_access.mov_instruction=1;
						cvcpu->header.exit_context.cr_access.write=0;
						cvcpu->header.exit_context.cr_access.reserved0=0;
					}
					else
					{
						invvpid_descriptor ivd={0};
						if(info.gpr_num==4)noir_vt_vmread(guest_rsp,&gpr_state->rsp);
						noir_vt_vmwrite(guest_cr3,gpr_array[info.gpr_num]);
						ivd.vpid=cvcpu->vm->vpid;
						noir_vt_invvpid(vpid_sicrgb_invd,&ivd);
						noir_vt_advance_rip();
					}
					break;
				}
				case 4:
				{
					// Write to CR4 register is intercepted...
					ulong_ptr val=gpr_array[info.gpr_num];
					if(info.gpr_num==4)noir_vt_vmread(guest_rsp,&val);
					if(val & ia32_cr4_vmxe_bit)
					{
						// Nested Virtualization in CVM is unsupported. Inject #GP.
						if(noir_bt(&cvcpu->header.exception_bitmap,ia32_general_protection) && cvcpu->header.vcpu_options.intercept_exceptions)
						{
							// The User Hypervisor specified to intercept #GP. Deliver it.
							nvc_vt_save_generic_cvexit_context(cvcpu);
							nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
							cvcpu->header.exit_context.intercept_code=cv_exception;
							cvcpu->header.exit_context.exception.vector=ia32_general_protection;
							cvcpu->header.exit_context.exception.ev_valid=true;
							cvcpu->header.exit_context.exception.error_code=0;
						}
						else
						{
							// The User Hypervisor did not intend to intercept #GP. Inject to the Guest.
							noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
						}
					}
					break;
				}
				case 8:
				{
					// Write to CR8 register is intercepted...
					if(info.gpr_num==4)
						noir_vt_vmread(guest_rsp,&cvcpu->header.crs.cr8);
					else
						cvcpu->header.crs.cr8=gpr_array[info.gpr_num];
					// If there is a pending interrupt, check if it can be injected...
					if(cvcpu->header.crs.cr8<cvcpu->header.injected_event.attributes.priority && cvcpu->header.injected_event.attributes.type==ia32_external_interrupt)
					{
						// If priority is sufficient, check if interrupt is enabled.
						ulong_ptr gflags;
						noir_vt_vmread(guest_rflags,&gflags);
						if(noir_bt(&gflags,ia32_rflags_if))
							noir_vt_inject_event(cvcpu->header.injected_event.attributes.vector,ia32_external_interrupt,cvcpu->header.injected_event.attributes.ec_valid,0,cvcpu->header.injected_event.error_code);
						else
						{
							// Intercept interrupt window.
							ia32_vmx_priproc_controls proc_ctrl1;
							noir_vt_vmread(primary_processor_based_vm_execution_controls,&proc_ctrl1.value);
							proc_ctrl1.interrupt_window_exiting=true;
							noir_vt_vmwrite(primary_processor_based_vm_execution_controls,proc_ctrl1.value);
						}
					}
					break;
				}
				default:
				{
					break;
				}
			}
			break;
		}
		case 1:
		{
			// Read from Control Registers is intercepted...
			switch(info.cr_num)
			{
				case 3:
				{
					if(cvcpu->header.vcpu_options.intercept_cr3)
					{
						nvc_vt_save_generic_cvexit_context(cvcpu);
						nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
						cvcpu->header.exit_context.intercept_code=cv_cr_access;
						cvcpu->header.exit_context.cr_access.cr_number=3;
						cvcpu->header.exit_context.cr_access.gpr_number=(u32)info.gpr_num;
						cvcpu->header.exit_context.cr_access.mov_instruction=1;
						cvcpu->header.exit_context.cr_access.write=0;
						cvcpu->header.exit_context.cr_access.reserved0=0;
					}
					else
					{
						invvpid_descriptor ivd={0};
						if(info.gpr_num==4)noir_vt_vmread(guest_rsp,&gpr_state->rsp);
						noir_vt_vmwrite(guest_cr3,gpr_array[info.gpr_num]);
						noir_vt_invvpid(vpid_sicrgb_invd,&ivd);
						noir_vt_advance_rip();
					}
					break;
				}
				case 8:
				{
					// The CR8 register is subject to be virtualized...
					if(info.gpr_num==4)
						noir_vt_vmwrite(guest_rsp,cvcpu->header.crs.cr8);
					else
						gpr_array[info.gpr_num]=cvcpu->header.crs.cr8;
					break;
				}
			}
			break;
		}
		case 2:
		{
			// The clts instruction is intercepted... (unexpected interception)
			// At this point, emulate it.
			ulong_ptr gcr0,cr0_rs;
			noir_vt_vmread(guest_cr0,&gcr0);
			noir_vt_vmread(cr0_read_shadow,&cr0_rs);
			noir_btr(&gcr0,ia32_cr0_ts);
			noir_btr(&cr0_rs,ia32_cr0_ts);
			noir_vt_vmwrite(guest_cr0,gcr0);
			noir_vt_vmwrite(cr0_read_shadow,cr0_rs);
			break;
		}
		case 3:
		{
			// The lmsw instruction is intercepted... (unexpected interception)
			// At this point, emulate it.
			ulong_ptr gcr0,cr0_rs;
			noir_vt_vmread(guest_cr0,&gcr0);
			noir_vt_vmread(cr0_read_shadow,&cr0_rs);
			*(u16*)&gcr0=(u16)info.lmsw_src;
			*(u16*)&cr0_rs=(u16)info.lmsw_src;
			noir_vt_vmwrite(guest_cr0,gcr0);
			noir_vt_vmwrite(cr0_read_shadow,cr0_rs);
			break;
		}
	}
}

void static fastcall nvc_vt_io_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	u16 size_array[8]={1,2,8,4,0,0,0,0};
	ia32_io_access_qualification info;
	ia32_vmexit_instruction_information exit_info;
	u32 seg_ar;
	noir_vt_vmread(vmexit_qualification,&info.value);
	noir_vt_vmread(vmexit_instruction_information,&exit_info.value);
	// Deliver the I/O interception to subverted host.
	nvc_vt_save_generic_cvexit_context(cvcpu);
	// Before the VMCS is switched, read essential data from VMCS.
	noir_vt_vmread(guest_es_selector+(exit_info.f0.segment<<1),&cvcpu->header.exit_context.io.segment.selector);
	noir_vt_vmread(guest_es_access_rights+(exit_info.f0.segment<<1),&seg_ar);
	*(u16p)&cvcpu->header.exit_context.io.segment.attrib=seg_ar;
	noir_vt_vmread(guest_es_limit+(exit_info.f0.segment<<1),&cvcpu->header.exit_context.io.segment.limit);
	noir_vt_vmread(guest_es_base+(exit_info.f0.segment<<1),&cvcpu->header.exit_context.io.segment.base);
	// Switch the vCPU context.
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_io_instruction;
	cvcpu->header.exit_context.io.access.io_type=(u16)info.direction;
	cvcpu->header.exit_context.io.access.string=(u16)info.string;
	cvcpu->header.exit_context.io.access.repeat=(u16)info.repeat;
	cvcpu->header.exit_context.io.access.operand_size=size_array[info.access_size];
	cvcpu->header.exit_context.io.access.address_width=1<<(exit_info.f0.address_size+1);
	cvcpu->header.exit_context.io.port=(u16)info.port;
	cvcpu->header.exit_context.io.rax=cvcpu->header.gpr.rax;
	cvcpu->header.exit_context.io.rcx=cvcpu->header.gpr.rcx;
	cvcpu->header.exit_context.io.rsi=cvcpu->header.gpr.rsi;
	cvcpu->header.exit_context.io.rdi=cvcpu->header.gpr.rdi;
}

void static fastcall nvc_vt_rdmsr_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	if(cvcpu->header.vcpu_options.intercept_msr)
	{
		// Switch to subverted host in order to handle rdmsr instruction.
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
		cvcpu->header.exit_context.intercept_code=cv_rdmsr_instruction;
		cvcpu->header.exit_context.msr.eax=(u32)cvcpu->header.gpr.rax;
	}
	else
	{
		// #GP exception is subject to be injected.
		if(!cvcpu->header.vcpu_options.intercept_exceptions)
			noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
		else
		{
			// If the User Hypervisor specifies interception of exceptions, pass to the User Hypervisor.
			nvc_vt_save_generic_cvexit_context(cvcpu);
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
			cvcpu->header.exit_context.intercept_code=cv_exception;
			cvcpu->header.exit_context.exception.vector=ia32_general_protection;
			cvcpu->header.exit_context.exception.ev_valid=true;
			cvcpu->header.exit_context.exception.reserved=0;
			cvcpu->header.exit_context.exception.error_code=0;
		}
	}
}

void static fastcall nvc_vt_wrmsr_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	if(cvcpu->header.vcpu_options.intercept_msr)
	{
		// Switch to subverted host in order to handle wrmsr instruction.
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
		cvcpu->header.exit_context.intercept_code=cv_wrmsr_instruction;
		cvcpu->header.exit_context.msr.eax=(u32)cvcpu->header.gpr.rax;
	}
	else
	{
		// #GP exception is subject to be injected.
		if(!cvcpu->header.vcpu_options.intercept_exceptions)
			noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
		else
		{
			// If the User Hypervisor specifies interception of exceptions, pass to the User Hypervisor.
			nvc_vt_save_generic_cvexit_context(cvcpu);
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
			cvcpu->header.exit_context.intercept_code=cv_exception;
			cvcpu->header.exit_context.exception.vector=ia32_general_protection;
			cvcpu->header.exit_context.exception.ev_valid=true;
			cvcpu->header.exit_context.exception.reserved=0;
			cvcpu->header.exit_context.exception.error_code=0;
		}
	}
}

void static fastcall nvc_vt_invalid_guest_state_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	ulong_ptr cr0,cr3,cr4,dr7;
	u64 efer,pat,debug_ctrl,se_cs,se_esp,se_eip;
	u16 cs_sel,ds_sel,es_sel,fs_sel,gs_sel,ss_sel,tr_sel,ldtr_sel;
	vmx_segment_access_right cs_ar,ds_ar,es_ar,fs_ar,gs_ar,ss_ar,tr_ar,ldtr_ar;
	u32 cs_lim,ds_lim,es_lim,fs_lim,gs_lim,ss_lim,tr_lim,ldtr_lim,idtr_lim,gdtr_lim;
	u64 cs_base,ds_base,es_base,fs_base,gs_base,ss_base,tr_base,ldtr_base,idtr_base,gdtr_base;
	ulong_ptr rflags,rip;
	// Invalid State in VMCS is detected by processor.
	nvc_vt_dump_vmcs_guest_state();
	// Dump the VMCS and parse the reason of failure.
	// Read Control & Debug Registers.
	noir_vt_vmread(guest_cr0,&cr0);
	noir_vt_vmread(guest_cr3,&cr3);
	noir_vt_vmread(guest_cr4,&cr4);
	noir_vt_vmread(guest_dr7,&dr7);
	// Read MSRs saved in VMCS.
	noir_vt_vmread(guest_msr_ia32_efer,&efer);
	noir_vt_vmread(guest_msr_ia32_pat,&pat);
	noir_vt_vmread(guest_msr_ia32_debug_ctrl,&debug_ctrl);
	noir_vt_vmread(guest_msr_ia32_sysenter_cs,&se_cs);
	noir_vt_vmread(guest_msr_ia32_sysenter_esp,&se_esp);
	noir_vt_vmread(guest_msr_ia32_sysenter_eip,&se_eip);
	// Read Segment Register - CS
	noir_vt_vmread(guest_cs_selector,&cs_sel);
	noir_vt_vmread(guest_cs_access_rights,&cs_ar);
	noir_vt_vmread(guest_cs_limit,&cs_lim);
	noir_vt_vmread(guest_cs_base,&cs_base);
	// Read Segment Register - DS
	noir_vt_vmread(guest_ds_selector,&ds_sel);
	noir_vt_vmread(guest_ds_access_rights,&ds_ar);
	noir_vt_vmread(guest_ds_limit,&ds_lim);
	noir_vt_vmread(guest_ds_base,&ds_base);
	// Read Segment Register - ES
	noir_vt_vmread(guest_es_selector,&es_sel);
	noir_vt_vmread(guest_es_access_rights,&es_ar);
	noir_vt_vmread(guest_es_limit,&es_lim);
	noir_vt_vmread(guest_es_base,&es_base);
	// Read Segment Register - FS
	noir_vt_vmread(guest_fs_selector,&fs_sel);
	noir_vt_vmread(guest_fs_access_rights,&fs_ar);
	noir_vt_vmread(guest_fs_limit,&fs_lim);
	noir_vt_vmread(guest_fs_base,&fs_base);
	// Read Segment Register - GS
	noir_vt_vmread(guest_gs_selector,&gs_sel);
	noir_vt_vmread(guest_gs_access_rights,&gs_ar);
	noir_vt_vmread(guest_gs_limit,&gs_lim);
	noir_vt_vmread(guest_gs_base,&gs_base);
	// Read Segment Register - SS
	noir_vt_vmread(guest_ss_selector,&ss_sel);
	noir_vt_vmread(guest_ss_access_rights,&ss_ar);
	noir_vt_vmread(guest_ss_limit,&ss_lim);
	noir_vt_vmread(guest_ss_base,&ss_base);
	// Read Segment Register - TR
	noir_vt_vmread(guest_tr_selector,&tr_sel);
	noir_vt_vmread(guest_tr_access_rights,&tr_ar);
	noir_vt_vmread(guest_tr_limit,&tr_lim);
	noir_vt_vmread(guest_tr_base,&tr_base);
	// Read Segment Register - LDTR
	noir_vt_vmread(guest_ldtr_selector,&ldtr_sel);
	noir_vt_vmread(guest_ldtr_access_rights,&ldtr_ar);
	noir_vt_vmread(guest_ldtr_limit,&ldtr_lim);
	noir_vt_vmread(guest_ldtr_base,&ldtr_base);
	// Read Descriptor Tables - IDTR & GDTR
	noir_vt_vmread(guest_idtr_limit,&idtr_lim);
	noir_vt_vmread(guest_idtr_base,&idtr_base);
	noir_vt_vmread(guest_gdtr_limit,&gdtr_lim);
	noir_vt_vmread(guest_gdtr_base,&gdtr_base);
	// Read General-Purpose Registers - Rip & RFlags
	noir_vt_vmread(guest_rip,&rip);
	noir_vt_vmread(guest_rflags,&rflags);
	// Perform examinations...

	// Deliver to subverted host.
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	nvc_vt_dump_vmcs_guest_state();
	cvcpu->header.exit_context.intercept_code=cv_invalid_state;
}

void static fastcall nvc_vt_invalid_msr_loading_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	nv_dprintf("The MSRs to be loaded for CVM is invalid! vCPU=0x%p\n",cvcpu);
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_invalid_state;
}

void static fastcall nvc_vt_ept_violation_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	ia32_ept_violation_qualification info;
	// EPT Violation occured, tell the subverted host there is a memory access fault.
	noir_vt_vmread64(guest_physical_address,&cvcpu->header.exit_context.memory_access.gpa);
	noir_vt_vmread(vmexit_qualification,(ulong_ptr*)&info);
	nvc_vt_save_generic_cvexit_context(cvcpu);
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_memory_access;
	cvcpu->header.exit_context.memory_access.access.read=(u8)info.read;
	cvcpu->header.exit_context.memory_access.access.write=(u8)info.write;
	cvcpu->header.exit_context.memory_access.access.execute=(u8)info.execute;
	cvcpu->header.exit_context.memory_access.access.fetched_bytes=0;
}

void static fastcall nvc_vt_ept_misconfig_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_scheduler_bug;
}

void static fastcall nvc_vt_invept_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_invvpid_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Nested Virtualization in CVM is currently unsupported.
	if(noir_bt(&cvcpu->header.exception_bitmap,ia32_invalid_opcode) && cvcpu->header.vcpu_options.intercept_exceptions)
	{
		cvcpu->header.exit_context.intercept_code=cv_exception;
		cvcpu->header.exit_context.exception.vector=ia32_invalid_opcode;
		cvcpu->header.exit_context.exception.ev_valid=false;
		cvcpu->header.exit_context.exception.reserved=0;
		cvcpu->header.exit_context.exception.error_code=0;
		cvcpu->header.exit_context.exception.fetched_bytes=0;
		nvc_vt_save_generic_cvexit_context(cvcpu);
		nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
	}
}

void static fastcall nvc_vt_xsetbv_cvexit_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu)
{
	// Emulate the xsetbv instruction.
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
	if(gp_exception)
	{
		// Exception induced by xsetbv has error code zero pushed onto stack.
		if(!noir_bt(&cvcpu->header.exception_bitmap,ia32_general_protection) || cvcpu->header.vcpu_options.intercept_exceptions)
			noir_vt_inject_event(ia32_general_protection,ia32_hardware_exception,true,0,0);
		else
		{
			nvc_vt_switch_to_host_vcpu(gpr_state,vcpu);
			cvcpu->header.exit_context.intercept_code=cv_exception;
			cvcpu->header.exit_context.exception.vector=ia32_general_protection;
			cvcpu->header.exit_context.exception.ev_valid=true;
			cvcpu->header.exit_context.exception.reserved=0;
			cvcpu->header.exit_context.exception.error_code=0;
			cvcpu->header.exit_context.exception.fetched_bytes=0;
		}
	}
	else
	{
		// Everything is fine.
		noir_xsetbv(index,value);
		noir_vt_advance_rip();
	}
}