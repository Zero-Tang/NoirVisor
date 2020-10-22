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

#define noir_svm_cpuid_std_submask	0x2080
#define noir_svm_cpuid_ext_submask	0x20000000

// Definition of vmmcall Codes
#define noir_svm_callexit			1

// Definition of Enabled features
#define noir_svm_vmcb_caching		1		// Bit 0
#define noir_svm_nested_paging		2		// Bit 1
#define noir_svm_flush_by_asid		4		// Bit 2
#define noir_svm_virtual_gif		8		// Bit 3
#define noir_svm_virtualized_vmls	16		// Bit 4
#define noir_svm_syscall_hook		32		// Bit 5

// Optimize Memory Usage of MSRPM and IOPM.
typedef struct _noir_svm_hvm
{
	memory_descriptor msrpm;
	memory_descriptor iopm;
	memory_descriptor blank_page;
	void* primary_nptm;
#if !defined(_hv_type1)
	void* secondary_nptm;
#endif
	u32 hvm_cpuid_leaf_max;
}noir_svm_hvm,*noir_svm_hvm_p;

typedef struct _noir_svm_virtual_msr
{
	// Add virtualized SVM MSR here.
	// System Call MSR
	u64 lstar;
	u64 sysenter_eip;
}noir_svm_virtual_msr,*noir_svm_virtual_msr_p;

typedef struct _noir_svm_nested_vcpu
{
	u64 hsave_gpa;
	bool svme;
}noir_svm_nested_vcpu,*noir_svm_nested_vcpu_p;

typedef struct _noir_svm_vcpu
{
	memory_descriptor vmcb;
	memory_descriptor hvmcb;
	memory_descriptor hsave;
	void* hv_stack;
	noir_svm_hvm_p relative_hvm;
	u32 proc_id;
	noir_svm_virtual_msr virtual_msr;
	noir_svm_nested_vcpu nested_hvm;
	u8 status;
	u8 enabled_feature;
	u8 reserved;
}noir_svm_vcpu,*noir_svm_vcpu_p;

// Layout of initial stack.
typedef struct _noir_svm_initial_stack
{
	u64 guest_vmcb_pa;
	u64 host_vmcb_pa;
	noir_svm_vcpu_p vcpu;
	u32 proc_id;
}noir_svm_initial_stack,*noir_svm_initial_stack_p;

u8 fastcall nvc_svm_subvert_processor_a(noir_svm_initial_stack_p host_rsp);
void nvc_svm_return(noir_gpr_state_p stack);
void fastcall nvc_svm_reserved_cpuid_handler(u32* info);
bool nvc_svm_build_cpuid_handler();
void nvc_svm_teardown_cpuid_handler();
bool nvc_svm_build_exit_handler();
void nvc_svm_teardown_exit_handler();