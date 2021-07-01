/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file defines structures and constants for SVM Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

// Definition of vmmcall Codes
#define noir_svm_callexit				1
#define noir_svm_create_custom_vcpu		2
#define noir_svm_delete_custom_vcpu		3
#define noir_svm_run_custom_vcpu		4

// Definition of Enabled features
#define noir_svm_vmcb_caching		1		// Bit 0
#define noir_svm_nested_paging		2		// Bit 1
#define noir_svm_flush_by_asid		4		// Bit 2
#define noir_svm_virtual_gif		8		// Bit 3
#define noir_svm_virtualized_vmls	16		// Bit 4
#define noir_svm_syscall_hook		32		// Bit 5
#define noir_svm_npt_with_hooks		64		// Bit 6

struct _noir_npt_manager;

// Optimize Memory Usage of MSRPM and IOPM.
typedef struct _noir_svm_hvm
{
	memory_descriptor msrpm;
	memory_descriptor iopm;
	memory_descriptor blank_page;
	u32 hvm_cpuid_leaf_max;
}noir_svm_hvm,*noir_svm_hvm_p;

typedef struct _noir_svm_virtual_msr
{
	// Add virtualized SVM MSR here.
	// System Call MSR
	u64 efer;
	u64 lstar;
	u64 sysenter_eip;
}noir_svm_virtual_msr,*noir_svm_virtual_msr_p;

typedef struct _noir_svm_nested_vcpu
{
	u64 hsave_gpa;
	void* hsave_hva;
	u32 asid_max;
	struct
	{
		u64 svme:1;
		u64 gif:1;
		u64 smm_status:1;	// 0 for outside SMM. 1 for inside SMM.
		u64 reserved:61;
	};
}noir_svm_nested_vcpu,*noir_svm_nested_vcpu_p;

// Virtual Processor defined for host.
typedef struct _noir_svm_vcpu
{
	memory_descriptor vmcb;
	memory_descriptor hvmcb;
	memory_descriptor hsave;
	void* hv_stack;
	noir_svm_hvm_p relative_hvm;
	struct _noir_npt_manager* primary_nptm;
#if !defined(_hv_type1)
	struct _noir_npt_manager* secondary_nptm;
#endif
	u32 proc_id;
	u32 asid;
	noir_svm_virtual_msr virtual_msr;
	noir_svm_nested_vcpu nested_hvm;
	noir_cvm_virtual_cpu cvm_state;
	u32 cpuid_fms;
	u8 status;
	u8 enabled_feature;
	u8 vcpu_property;
	u8 reserved;
}noir_svm_vcpu,*noir_svm_vcpu_p;

struct _noir_svm_custom_vm;
struct _noir_npt_pdpte_descriptor;
struct _noir_npt_pde_descriptor;
struct _noir_npt_pte_descriptor;
struct _amd64_npt_pml4e;

typedef struct _noir_svm_custom_npt_manager
{
	struct
	{
		struct _amd64_npt_pml4e *virt;	// There are only 512 possible PML4Es in principle.
		u64 phys;
	}ncr3;
	struct
	{
		struct _noir_npt_pdpte_descriptor *head;
		struct _noir_npt_pdpte_descriptor *tail;
	}pdpte;
	struct
	{
		struct _noir_npt_pde_descriptor *head;
		struct _noir_npt_pde_descriptor *tail;
	}pde;
	struct
	{
		struct _noir_npt_pte_descriptor *head;
		struct _noir_npt_pte_descriptor *tail;
	}pte;
}noir_svm_custom_npt_manager,*noir_svm_custom_npt_manager_p;

// Virtual Processor defined for Customizable VM.
// Use Linked List so that vCPU can be dynamically added in CVM Runtime.
typedef struct _noir_svm_custom_vcpu
{
	struct _noir_svm_custom_vm *vm;
	struct _noir_svm_custom_vcpu *next;
	memory_descriptor vmcb;
	noir_cvm_virtual_cpu cvm_state;
	u64 lasted_tsc;
	u32 proc_id;
}noir_svm_custom_vcpu,*noir_svm_custom_vcpu_p;

typedef struct _noir_svm_custom_vm
{
	struct
	{
		noir_svm_custom_vcpu_p head;
		noir_svm_custom_vcpu_p tail;
	}vcpu;
	u32 vcpu_count;
	u32 asid;
	struct _noir_svm_custom_npt_manager nptm;
}noir_svm_custom_vm,*noir_svm_custom_vm_p;

// Layout of initial stack.
typedef struct _noir_svm_initial_stack
{
	u64 guest_vmcb_pa;
	u64 host_vmcb_pa;
	noir_svm_vcpu_p vcpu;
	noir_svm_custom_vcpu_p custom_vcpu;
	u32 proc_id;
}noir_svm_initial_stack,*noir_svm_initial_stack_p;

u8 fastcall nvc_svm_subvert_processor_a(noir_svm_initial_stack_p host_rsp);
void nvc_svm_return(noir_gpr_state_p stack);
void fastcall nvc_svm_reserved_cpuid_handler(u32* info);
void nvc_svm_set_mshv_handler(bool option);
bool nvc_svm_build_cpuid_handler();
void nvc_svm_teardown_cpuid_handler();
bool nvc_svm_build_exit_handler();
void nvc_svm_teardown_exit_handler();