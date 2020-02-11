/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines structures and constants for SVM Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

// Definition of vmmcall Codes
#define noir_svm_callexit			1

// Definition of Enabled features
#define noir_svm_vmcb_caching		1		// Bit 0
#define noir_svm_nested_paging		2		// Bit 1
#define noir_svm_flush_by_asid		4		// Bit 2
#define noir_svm_virtual_gif		8		// Bit 3
#define noir_svm_virtualized_vmls	16		// Bit 4

typedef struct _memory_descriptor
{
	void* virt;
	u64 phys;
}memory_descriptor,*memory_descriptor_p;

// Optimize Memory Usage of MSRPM and IOPM.
typedef struct _noir_svm_hvm
{
	memory_descriptor msrpm;
	memory_descriptor iopm;
	memory_descriptor blank_page;
	void* primary_nptm;
	void* secondary_nptm;
	u32 std_leaftotal;
	u32 ext_leaftotal;
}noir_svm_hvm,*noir_svm_hvm_p;

// Improve performance of CPUID virtualization under the nested scenario.
typedef struct _noir_svm_cached_cpuid
{
	// The info will be implementation specific.
	// Different function leaf would have different structure.
	void** std_leaf;	// 0x00000000-0x000000FF
	void** hvm_leaf;	// 0x40000000-0x400000FF
	void** ext_leaf;	// 0x80000000-0x800000FF
	void** res_leaf;	// 0xC0000000-0xC00000FF
	// Maximum Counts.
	u32 max_leaf[4];
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
	u8 enabled_feature;
}noir_svm_vcpu,*noir_svm_vcpu_p;

// Layout of initial stack.
typedef struct _noir_svm_initial_stack
{
	u64 guest_vmcb_pa;
	noir_svm_vcpu_p vcpu;
	u32 proc_id;
}noir_svm_initial_stack,*noir_svm_initial_stack_p;

u8 nvc_svm_subvert_processor_a(noir_svm_initial_stack_p host_rsp);
void fastcall nvc_svm_reserved_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
bool nvc_svm_build_cpuid_handler(u32 std_count,u32 hvm_count,u32 ext_count,u32 res_count);
void nvc_svm_teardown_cpuid_handler();
bool nvc_svm_build_exit_handler();
void nvc_svm_teardown_exit_handler();
void nvc_svm_return(noir_gpr_state_p stack);