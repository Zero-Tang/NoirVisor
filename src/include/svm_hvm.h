/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

  This file defines structures and constants for SVM Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

//This is AMD specific
#define noir_svm_cpuid_std_submask	0xFFFFE080
#define noir_svm_cpuid_ext_submask	0x20000000

#define noir_svm_callexit			1

typedef struct _memory_descriptor
{
	void* virt;
	u64 phys;
}memory_descriptor,*memory_descriptor_p;

//Optimize Memory Usage of MSRPM and IOPM.
typedef struct _noir_svm_hvm
{
	memory_descriptor msrpm;
	memory_descriptor iopm;
	u32 std_leaftotal;
	u32 ext_leaftotal;
	u32 cpuid_std_submask;
	u32 cpuid_ext_submask;
}noir_svm_hvm,*noir_svm_hvm_p;

typedef struct _noir_svm_cpuid_info
{
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
}noir_svm_cpuid_info,*noir_svm_cpuid_info_p;

// Improve performance of CPUID virtualization under the nested scenario.
typedef struct _noir_svm_cached_cpuid
{
	noir_svm_cpuid_info_p std_leaf;
	noir_svm_cpuid_info_p ext_leaf;
}noir_svm_cached_cpuid,*noir_svm_cached_cpuid_p;

typedef struct _noir_svm_nested_vcpu
{
	u64 hsave_gpa;
	bool svme;
}noir_svm_nested_vcpu,*noir_svm_nested_vcpu_p;

typedef struct _noir_svm_vcpu
{
	memory_descriptor vmcb;
	memory_descriptor hsave;
	void* hv_stack;
	noir_svm_hvm_p relative_hvm;
	u32 proc_id;
	noir_svm_cached_cpuid cpuid_cache;
	noir_svm_nested_vcpu nested_hvm;
	u8 status;
}noir_svm_vcpu,*noir_svm_vcpu_p;

// Layout of initial stack.
typedef struct _noir_svm_initial_stack
{
	u64 guest_vmcb_pa;
	noir_svm_vcpu_p vcpu;
	u32 proc_id;
}noir_svm_initial_stack,*noir_svm_initial_stack_p;

u8 nvc_svm_subvert_processor_a(noir_svm_initial_stack_p host_rsp);
bool nvc_svm_build_exit_handler();
void nvc_svm_teardown_exit_handler();
void nvc_svm_return(noir_gpr_state_p stack);