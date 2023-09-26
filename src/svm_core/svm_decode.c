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
		const void* vmcb=vcpu->vmcb.virt;
		const u64 gcr3=noir_svm_vmread64(vmcb,guest_cr3);
		const u64 grip=noir_svm_vmread64(vmcb,guest_rip);
		const u64 gcsb=noir_svm_vmread64(vmcb,guest_cs_base);
		const u64 gip=gcsb+grip;
		// Fetch instruction bytes from Host
		u8p ins_bytes=(u8p)((ulong_ptr)vcpu->vmcb.virt+guest_instruction_bytes);
		u32 error_code=0;
		// For the sake of universal safety, do not load cr3 into the processor.
		size_t len=nvc_copy_host_virtual_memory64(gcr3,gip,ins_bytes,15,false,noir_svm_vmcb_bt32(vmcb,guest_cr4,amd64_cr4_la57),&error_code);
		noir_svm_vmwrite8(vmcb,number_of_bytes_fetched,(u8)len);
	}
}

void static nvc_svm_decoder_instruction_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// This interception does not involve assisting decodings.
	// However, it still helps if Next-RIP Saving is unsupported.
	if(!noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_nrips))
	{
		const void* vmcb=(cvcpu?cvcpu->vmcb:vcpu->vmcb).virt;
		u32 mode=0,len=0;
		u64 nrip;
		// Fetch the instruction.
		nvc_svm_fetch_instruction(vcpu,cvcpu);
		// Calculate the length.
		if(noir_svm_vmcb_bt32(vmcb,guest_cs_attrib,9))noir_bts(&mode,1);
		if(noir_svm_vmcb_bt32(vmcb,guest_cs_attrib,10))noir_bts(&mode,0);
		len=noir_get_instruction_length_ex((u8p)((ulong_ptr)vmcb+guest_instruction_bytes),mode);
		nrip=noir_svm_vmread64(vmcb,guest_rip)+len;
		if(noir_svm_vmcb_bt32(vmcb,guest_cs_attrib,9))nrip&=0xFFFFFFFF;
		noir_svm_vmwrite64(vmcb,next_rip,nrip);
	}
}

void static nvc_svm_decoder_event_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// This interception does not involve assisting decodings.
	// Event has no instruction length, so just do nothing.
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

void static nvc_svm_decoder_io_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// AMD-V will always assist decoding I/O instructions, regardless of the support to Decode-Assists!
	// The next rip is always saved in Exit-Info 2, regardless of the support to Next-Rip Saving!
	if(!noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_nrips))
	{
		const void* vmcb=(cvcpu?cvcpu->vmcb:vcpu->vmcb).virt;
		noir_svm_vmwrite64(vmcb,next_rip,noir_svm_vmread64(vmcb,exit_info2));
	}
}

void static nvc_svm_decoder_npf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// For #NPF interceptions, fetching the instructions is just what we need to do.
	if(!noir_bt(&hvm_p->relative_hvm->virt_cap.capabilities,amd64_cpuid_decoder))
		nvc_svm_fetch_instruction(vcpu,cvcpu);
}