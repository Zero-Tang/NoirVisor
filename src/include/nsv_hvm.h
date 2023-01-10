/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is header file for Customizable VM of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/cvm_hvm.h
*/

#include <nvdef.h>
#include <nvbdk.h>

#define nsv_msr_guest_os_id				0x40000000
#define nsv_msr_ghcb					0x40000001
#define nsv_msr_vcpu_index				0x40000002
#define nsv_msr_eoi						0x40000070
#define nsv_msr_icr						0x40000071
#define nsv_msr_tpr						0x40000072
#define nsv_msr_activation				0x40010130
#define nsv_msr_active_status			0x40010131
#define nsv_msr_vc_handler_cs			0x40010140
#define nsv_msr_vc_handler_rsp			0x40010141
#define nsv_msr_vc_handler_rip			0x40010142
#define nsv_msr_vc_return_cs			0x40010150
#define nsv_msr_vc_return_rsp			0x40010151
#define nsv_msr_vc_return_rip			0x40010152
#define nsv_msr_vc_return_rflags		0x40010153
#define nsv_msr_vc_next_rip				0x40010154
#define nsv_msr_vc_error_code			0x40010155
#define nsv_msr_vc_info1				0x40010156
#define nsv_msr_vc_info2				0x40010157
#define nsv_msr_vc_info3				0x40010158
#define nsv_msr_vc_info4				0x40010159
#define nsv_msr_claim_gpa_cmd			0x40010180
#define nsv_msr_claim_gpa_start			0x40010181
#define nsv_msr_claim_gpa_end			0x40010182

// Generic Structure of MSRs that contain selectors.
typedef union _noir_nsv_selector_msr
{
	struct
	{
		u16 cs;
		u16 ss;
		u32 reserved;
	};
	u64 value;
}noir_nsv_selector_msr,*noir_nsv_selector_msr_p;

typedef struct _noir_nsv_internal_msr_state
{
	noir_nsv_selector_msr vc_handler_cs;
	u64 vc_handler_rsp;
	u64 vc_handler_rip;
	noir_nsv_selector_msr vc_return_cs;
	u64 vc_return_rsp;
	u64 vc_return_rip;
	u64 vc_return_rflags;
	u64 vc_next_rip;
	u64 vc_error_code;
	u64 vc_info1;
	u64 vc_info2;
	u64 vc_info3;
	u64 vc_info4;
}noir_nsv_internal_msr_state,*noir_nsv_internal_msr_state_p;

typedef struct _noir_nsv_synthetic_msr_state
{
	u64 ghcb;
	u64 claim_gpa_start;
	u64 claim_gpa_end;
}noir_nsv_synthetic_msr_state,*noir_nsv_synthetic_msr_state_p;

typedef struct _noir_nsv_activation_context
{
	union
	{
		struct
		{
			u64 activation:1;	// Activate as NSV Guest.
			u64 reserved:63;
		};
		u64 value;
	};
}noir_nsv_activation_context,*noir_nsv_activation_context_p;

typedef struct _noir_nsv_claim_pages_context
{
	union
	{
		struct
		{
			u64 claim:1;
			u64 reserved:63;
		};
		u64 value;
	};
}noir_nsv_claim_pages_context,*noir_nsv_claim_pages_context_p;

struct _noir_cvm_virtual_cpu;
struct _noir_cvm_virtual_machine;

// This structure must be aligned on page-granularity.
// This structure is intended for NSV-enabled vCPUs.
typedef struct _noir_nsv_virtual_cpu
{
	// We only need to save registers that
	// are not covered by VMCS or VMCB.
	noir_gpr_state gpr;		// VMCB covers rax.
	u64 cr2;				// VMCS does not cover cr2.
	u64 cr8;				// VMCS does not cover cr8.
	u64 dr0;
	u64 dr1;
	u64 dr2;
	u64 dr3;
	u64 dr6;				// VMCS does not cover dr6.
	u64 xcr0;
	struct
	{
		struct _noir_cvm_virtual_cpu *vcpu;
		union
		{
			memory_descriptor vmcb;
			memory_descriptor vmcs;
		};
	}parent;
	noir_nsv_internal_msr_state nsvs;
	// The XSAVE State must be aligned on 64-byte boundary.
	align_at(64) noir_fx_state xstate;
}noir_nsv_virtual_cpu,*noir_nsv_virtual_cpu_p;

// This structure must be aligned on page-granularity.
// This structure is intended for NSV-enabled VMs.
typedef struct _noir_nsv_virtual_machine
{
	struct
	{
		struct _noir_cvm_virtual_machine *vm;
	}parent;
	// Store AES Keys intended for crypto operations.
	align_at(16) u8 aes_key[16];
	align_at(16) u8 expanded_encryption_keys[160];
	align_at(16) u8 expanded_decryption_keys[160];
}noir_nsv_virtual_machine,*noir_nsv_virtual_machine_p;