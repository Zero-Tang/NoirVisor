/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the NoirVisor Secure Virtualization Engine for AMD-V

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_cvnsv.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
#include "svm_exit.h"
#include "svm_def.h"
#include "svm_npt.h"

void nvc_svm_nsv_load_nae_synthetic_msr_state(noir_svm_custom_vcpu_p cvcpu)
{
	// Save information for return.
	cvcpu->header.nsvs.vc_return_cs.cs=noir_svm_vmread16(cvcpu->vmcb.virt,guest_cs_selector);
	cvcpu->header.nsvs.vc_return_cs.ss=noir_svm_vmread16(cvcpu->vmcb.virt,guest_ss_selector);
	cvcpu->header.nsvs.vc_return_cs.reserved=0;
	cvcpu->header.nsvs.vc_return_rsp=noir_svm_vmread64(cvcpu->vmcb.virt,guest_rsp);
	cvcpu->header.nsvs.vc_return_rip=noir_svm_vmread64(cvcpu->vmcb.virt,guest_rip);
	cvcpu->header.nsvs.vc_return_rflags=noir_svm_vmread64(cvcpu->vmcb.virt,guest_rflags);
	cvcpu->header.nsvs.vc_next_rip=noir_svm_vmread64(cvcpu->vmcb.virt,next_rip);
	// FIXME: Setup fallback if injection fails...
	// Load information for #VC exception.
	noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_cs_selector,cvcpu->header.nsvs.vc_handler_cs.cs);
	noir_svm_vmwrite16(cvcpu->vmcb.virt,guest_ss_selector,cvcpu->header.nsvs.vc_handler_cs.ss);
	// FIXME: Load segment attributes, limits and bases.
	noir_svm_vmcb_btr32(cvcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_segment_reg);
	noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_rsp,cvcpu->header.nsvs.vc_handler_rsp);
	noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_rip,cvcpu->header.nsvs.vc_handler_rip);
	noir_svm_vmcb_btr32(cvcpu->vmcb.virt,guest_rflags,amd64_rflags_if);
}

bool nvc_svm_rdmsr_nsvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	const u32 index=(const u32)gpr_state->rcx;
	large_integer val;
	bool no_exit=true,fault=false;
	switch(index)
	{
		case nsv_msr_ghcb:
		{
			val.value=cvcpu->header.nsvs.ghcb;
			break;
		}
		case nsv_msr_vc_handler_cs:
		{
			val.value=cvcpu->header.nsvs.vc_handler_cs.value;
			break;
		}
		case nsv_msr_vc_handler_rsp:
		{
			val.value=cvcpu->header.nsvs.vc_handler_rsp;
			break;
		}
		case nsv_msr_vc_handler_rip:
		{
			val.value=cvcpu->header.nsvs.vc_handler_rip;
			break;
		}
		case nsv_msr_vc_return_cs:
		{
			val.value=cvcpu->header.nsvs.vc_return_cs.value;
			break;
		}
		case nsv_msr_vc_return_rsp:
		{
			val.value=cvcpu->header.nsvs.vc_return_rsp;
			break;
		}
		case nsv_msr_vc_return_rip:
		{
			val.value=cvcpu->header.nsvs.vc_return_rip;
			break;
		}
		case nsv_msr_vc_return_rflags:
		{
			val.value=cvcpu->header.nsvs.vc_return_rflags;
			break;
		}
		case nsv_msr_vc_next_rip:
		{
			val.value=cvcpu->header.nsvs.vc_next_rip;
			break;
		}
		case nsv_msr_vc_error_code:
		{
			val.value=cvcpu->header.nsvs.vc_error_code;
			break;
		}
		case nsv_msr_vc_info1:
		{
			val.value=cvcpu->header.nsvs.vc_info1;
			break;
		}
		case nsv_msr_vc_info2:
		{
			val.value=cvcpu->header.nsvs.vc_info2;
			break;
		}
		default:
		{
			// Unhandled rdmsr instruction!
			fault=true;
			break;
		}
	}
	if(fault)
		noir_svm_inject_event(cvcpu->vmcb.virt,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
	else if(!no_exit)
	{
		*(u32p)&gpr_state->rax=val.low;
		*(u32p)&gpr_state->rdx=val.high;
		noir_svm_advance_rip(cvcpu->vmcb.virt);
	}
	return no_exit;
}

bool nvc_svm_wrmsr_nsvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	const u32 index=(const u32)gpr_state->rcx;
	large_integer val;
	bool no_exit=true,fault=false;
	val.low=(u32)gpr_state->rax;
	val.high=(u32)gpr_state->rdx;
	switch(index)
	{
		case nsv_msr_ghcb:
		{
			cvcpu->header.nsvs.ghcb=val.value;
			break;
		}
		case nsv_msr_activation:
		{
			// Guest is requesting NSV (de)activation.
			// Validate the command.
			cvcpu->header.exit_context.nsv_activation.value=val.value;
			if(cvcpu->header.exit_context.nsv_activation.reserved)
				fault=true;		// Reserved fields MBZ.
			else
			{
				cvcpu->header.exit_context.intercept_code=cv_scheduler_nsv_activate;
				no_exit=false;
			}
			break;
		}
		case nsv_msr_vc_handler_cs:
		{
			cvcpu->header.nsvs.vc_handler_cs.value=val.value;
			break;
		}
		case nsv_msr_vc_handler_rsp:
		{
			cvcpu->header.nsvs.vc_handler_rsp=val.value;
			break;
		}
		case nsv_msr_vc_handler_rip:
		{
			cvcpu->header.nsvs.vc_handler_rip=val.value;
			break;
		}
		case nsv_msr_claim_gpa_cmd:
		{
			// Guest is claiming security of its pages.
			// Validate the command.
			if(cvcpu->vm->header.properties.nsv_guest==false)
				fault=true;			// NSV must be activated before claiming security.
			else
			{
				cvcpu->header.exit_context.claim_pages.value=val.value;
				if(cvcpu->header.exit_context.claim_pages.reserved)
					fault=true;		// Reserved fields MBZ.
				else
				{
					u64 pages=page_count(cvcpu->header.nsvs.claim_gpa_end-cvcpu->header.nsvs.claim_gpa_start);
					if(pages>maxu32)
						fault=true;
					else
					{
						cvcpu->header.exit_context.intercept_code=cv_scheduler_nsv_claim_security;
						no_exit=false;
					}
				}
			}
			break;
		}
		case nsv_msr_claim_gpa_start:
		{
			cvcpu->header.nsvs.claim_gpa_start=val.value;
			break;
		}
		case nsv_msr_claim_gpa_end:
		{
			cvcpu->header.nsvs.claim_gpa_end=val.value;
			break;
		}
		default:
		{
			// Unhandled wrmsr instruction!
			fault=true;
			break;
		}
	}
	if(fault)
		noir_svm_inject_event(cvcpu->vmcb.virt,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
	else if(!no_exit)
		noir_svm_advance_rip(cvcpu->vmcb.virt);
	return no_exit;
}