/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines structures and constants for VMX Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

#define noir_vt_callexit				0x1
#define noir_vt_disasm_length			0x2
#define noir_vt_disasm_mnemonic			0x3

#define noir_vt_init_custom_vmcs		0x10000
#define noir_vt_run_custom_vcpu			0x10001
#define noir_vt_dump_vcpu_vmcs			0x10002
#define noir_vt_set_vcpu_options		0x10003

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
#define noir_vt_kva_shadow_presence	32		// Bit	5

// Index of MSRs to be loaded in array
#define noir_vt_cvm_msr_auto_star		0
#define noir_vt_cvm_msr_auto_lstar		1
#define noir_vt_cvm_msr_auto_cstar		2
#define noir_vt_cvm_msr_auto_sfmask		3
#define noir_vt_cvm_msr_auto_gsswap		4

#define noir_vt_cvm_msr_auto_max		5

typedef struct _noir_vt_hvm
{
	memory_descriptor msr_bitmap;
	memory_descriptor io_bitmap_a;
	memory_descriptor io_bitmap_b;
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

struct _noir_ept_manager;

typedef struct _noir_vt_vcpu
{
	memory_descriptor vmxon;
	memory_descriptor vmcs;
	memory_descriptor msr_auto;
	void* hv_stack;
	noir_vt_hvm_p relative_hvm;
	struct _noir_ept_manager *ept_manager;
	noir_vt_virtual_msr virtual_msr;
	noir_vt_nested_vcpu nested_vcpu;
	noir_cvm_virtual_cpu cvm_state;
	noir_mshv_vcpu mshvcpu;
	u32 family_ext;		// Cached info of Extended Family.
	u8 status;
	u8 enabled_feature;
	u8 mtrr_dirty;
}noir_vt_vcpu,*noir_vt_vcpu_p;

struct _noir_vt_custom_vm;
struct _noir_ept_pdpte_descriptor;
struct _noir_ept_pde_descriptor;
struct _noir_ept_pte_descriptor;
union _ia32_ept_pml4e;
union _ia32_ept_pointer;

typedef struct _noir_vt_custom_ept_manager
{
	struct
	{
		union _ia32_ept_pml4e *virt;	// There are only 512 possible PML4Es in principle.
		u64 phys;
	}eptp;
	struct
	{
		struct _noir_ept_pdpte_descriptor *head;
		struct _noir_ept_pdpte_descriptor *tail;
	}pdpte;
	struct
	{
		struct _noir_ept_pde_descriptor *head;
		struct _noir_ept_pde_descriptor *tail;
	}pde;
	struct
	{
		struct _noir_ept_pte_descriptor *head;
		struct _noir_ept_pte_descriptor *tail;
	}pte;
}noir_vt_custom_ept_manager,*noir_vt_custom_ept_manager_p;

// Virtual Processor defined for Customizable VM.
typedef struct _noir_vt_custom_vcpu
{
	noir_cvm_virtual_cpu header;
	struct _noir_vt_custom_vm *vm;
	memory_descriptor vmcs;
	memory_descriptor msr_auto;
	union
	{
		struct
		{
			u64 reserved:63;
			u64 rescission:1;
		};
		u64 value;
	}special_state;
	u64 lasted_tsc;
	u32 proc_id;
	u32 vcpu_id;
}noir_vt_custom_vcpu,*noir_vt_custom_vcpu_p;

typedef struct _noir_vt_custom_vm
{
	noir_cvm_virtual_machine header;
	memory_descriptor msr_bitmap;
	noir_vt_custom_vcpu_p* vcpu;
	u32 vcpu_count;
	u16 vpid;
	struct _noir_vt_custom_ept_manager eptm;
}noir_vt_custom_vm,*noir_vt_custom_vm_p;

typedef struct _noir_vt_initial_stack
{
	noir_vt_vcpu_p vcpu;
	noir_vt_custom_vcpu_p custom_vcpu;
	u32 proc_id;
	union
	{
		struct
		{
			u32 initial_vmcs:1;		// This bit is set to indiate that vmlaunch instruction is required.
			u32 reserved:31;
		};
		u32 value;
	}flags;
}noir_vt_initial_stack,*noir_vt_initial_stack_p;

#if defined(_vt_exit)
noir_vt_custom_vcpu nvc_vt_idle_cvcpu={0};
#else
extern noir_vt_custom_vcpu nvc_vt_idle_cvcpu;
#endif

u8 fastcall nvc_vt_subvert_processor_a(noir_vt_vcpu_p vcpu);
noir_status nvc_vtc_initialize_cvm_module();
void nvc_vtc_finalize_cvm_module();
void nvc_vt_initialize_cvm_vmcs(noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu);
void nvc_vt_switch_to_guest_vcpu(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu);
void nvc_vt_switch_to_host_vcpu(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void nvc_vt_dump_vcpu_state(noir_vt_custom_vcpu_p vcpu);
void nvc_vt_set_guest_vcpu_options(noir_vt_vcpu_p vcpu,noir_vt_custom_vcpu_p cvcpu);
void nvc_vt_resume_without_entry(noir_gpr_state_p state);
void nvc_vt_exit_handler_a(void);
void nvc_vt_set_mshv_handler(bool option);
void noir_vt_vmsuccess();
void noir_vt_vmfail_invalid();
void noir_vt_vmfail_valid();
void noir_vt_vmfail(noir_vt_nested_vcpu_p nested_vcpu,u32 message);
bool noir_vt_nested_vmread(void* vmcs,u32 encoding,ulong_ptr* data);
bool noir_vt_nested_vmwrite(void* vmcs,u32 encoding,ulong_ptr data);