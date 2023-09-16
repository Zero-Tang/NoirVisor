/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file decodes interceptions if Decode-Assist and/or Next-RIP are unsupported.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_exit.c
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

// This module is built because Linux KVM does not support Decode-Assist.

// Fetched instruction bytes will be placed in VMCB.
void static nvc_svm_fetch_instruction(noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	if(cvcpu)
	{
		// Fetch instruction bytes from CVM
	}
	else
	{
		// Guest information
		const u64 gcr3=noir_svm_vmread64(vcpu->vmcb.virt,guest_cr3);
		const u64 grip=noir_svm_vmread64(vcpu->vmcb.virt,guest_rip);
		const u64 gcsb=noir_svm_vmread64(vcpu->vmcb.virt,guest_cs_base);
		const u64 gip=gcsb+grip;
		// Fetch instruction bytes from Host
		u8p ins_bytes=(u8p)((ulong_ptr)vcpu->vmcb.virt+guest_instruction_bytes);

	}
}

void static nvc_svm_decoder_instruction_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// This interception does not involve assisting decodings.
	// However, it still helps if Next-RIP Saving is unsupported.
	if(!noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_nrips))
	{
		// Fetch the instruction.
		nvc_svm_fetch_instruction(vcpu,cvcpu);
		// Calculate the length.
	}
}

void static nvc_svm_decoder_event_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// This interception does not involve assisting decodings.
	// So just do nothing.
}

void static nvc_svm_decoder_cr_access_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	;
}

void static nvc_svm_decoder_dr_access_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	;
}

void static nvc_svm_decoder_pf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// For #PF interceptions, fetching the instructions is just what we need to do.
	if(!noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_decoder))
		nvc_svm_fetch_instruction(vcpu,cvcpu);
}

void static nvc_svm_decoder_int_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	;
}

void static nvc_svm_decoder_invlpg_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	;
}

void static nvc_svm_decoder_cr_write_trap_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// For Trap-like interceptions, do not calculate next rip.
	;
}

void static nvc_svm_decoder_npf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// For #NPF interceptions, fetching the instructions is just what we need to do.
	if(!noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_decoder))
		nvc_svm_fetch_instruction(vcpu,cvcpu);
}