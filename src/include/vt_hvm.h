/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file defines structures and constants for VMX Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

#define noir_vt_callexit		1

typedef struct _memory_descriptor
{
	void* virt;
	u64 phys;
}memory_descriptor,*memory_descriptor_p;

typedef struct _noir_vt_hvm
{
	memory_descriptor msr_bitmap;
	memory_descriptor io_bitmap_a;
	memory_descriptor io_bitmap_b;
	memory_descriptor msr_auto_list;
	u64 eptp;
}noir_vt_hvm,*noir_vt_hvm_p;

typedef struct _noir_vt_msr_entry
{
	u32 index;
	u32 reserved;
	u64 value;
}noir_vt_msr_entry,*noir_vt_msr_entry_p;

typedef struct _noir_vt_vcpu
{
	memory_descriptor vmxon;
	memory_descriptor vmcs;
	void* hv_stack;
	noir_vt_hvm_p relative_hvm;
	void* ept_manager;
	u8 status;
}noir_vt_vcpu,*noir_vt_vcpu_p;

u8 nvc_vt_subvert_processor_a(noir_vt_vcpu_p vcpu);
noir_status nvc_vt_build_exit_handlers();
void nvc_vt_teardown_exit_handlers();
void nvc_vt_resume_without_entry(noir_gpr_state_p state);
extern void nvc_vt_exit_handler_a();