/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file defines structures and constants for VMX Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

#define noir_vt_callexit		1

#define noir_nvt_vmxe			0
#define noir_nvt_vmxon			1

// Constant for TSC offseting by Assembly Part.
#define noir_vt_tsc_asm_offset		666		// To be fine tuned.

// Definition of Enabled Features
#define noir_vt_extended_paging		1		// Bit	0
#define noir_vt_vpid_tagged_tlb		2		// Bit	1
#define noir_vt_vmcs_shadowing		4		// Bit	2
#define noir_vt_ept_with_hooks		8		// Bit	3
#define noir_vt_syscall_hook		16		// Bit	4

typedef struct _noir_vt_hvm
{
	memory_descriptor msr_bitmap;
	memory_descriptor io_bitmap_a;
	memory_descriptor io_bitmap_b;
	memory_descriptor msr_auto_list;
	u32 hvm_cpuid_leaf_max;
}noir_vt_hvm,*noir_vt_hvm_p;

typedef struct _noir_vt_msr_entry
{
	u32 index;
	u32 reserved;
	u64 value;
}noir_vt_msr_entry,*noir_vt_msr_entry_p;

typedef struct _noir_vt_virtual_msr
{
	u64 vmx_msr[0x12];
	u64 sysenter_eip;
	u64 lstar;
}noir_vt_virtual_msr,*noir_vt_virtual_msr_p;

typedef struct _noir_vt_nested_vcpu
{
	memory_descriptor vmxon;
	// Abstracted-to-user VMCS.
	memory_descriptor vmcs_c;
	// Abstracted-to-CPU VMCS.
	memory_descriptor vmcs_t;
	u32 status;
}noir_vt_nested_vcpu,*noir_vt_nested_vcpu_p;

typedef struct _noir_vt_vcpu
{
	memory_descriptor vmxon;
	memory_descriptor vmcs;
	void* hv_stack;
	noir_vt_hvm_p relative_hvm;
	void* ept_manager;
	noir_vt_virtual_msr virtual_msr;
	noir_vt_nested_vcpu nested_vcpu;
	u32 family_ext;		// Cached info of Extended Family.
	u8 status;
	u8 enabled_feature;
}noir_vt_vcpu,*noir_vt_vcpu_p;

typedef struct _noir_vt_initial_stack
{
	noir_vt_vcpu_p vcpu;
	u32 proc_id;
}noir_vt_initial_stack,*noir_vt_initial_stack_p;

u8 fastcall nvc_vt_subvert_processor_a(noir_vt_vcpu_p vcpu);
noir_status nvc_vt_build_exit_handlers();
void nvc_vt_teardown_exit_handlers();
void nvc_vt_resume_without_entry(noir_gpr_state_p state);
void nvc_vt_exit_handler_a(void);
void nvc_vt_set_mshv_handler(bool option);
void noir_vt_vmsuccess();
void noir_vt_vmfail_invalid();
void noir_vt_vmfail_valid();
void noir_vt_vmfail(noir_vt_nested_vcpu_p nested_vcpu,u32 message);
bool noir_vt_nested_vmread(void* vmcs,u32 encoding,ulong_ptr* data);
bool noir_vt_nested_vmwrite(void* vmcs,u32 encoding,ulong_ptr data);