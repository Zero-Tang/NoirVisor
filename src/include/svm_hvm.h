/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines structures and constants for SVM Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

// Definition of vmmcall Codes
#define noir_svm_callexit					0x1
#define noir_svm_disasm_length				0x2
#define noir_svm_disasm_mnemonic			0x3
#define noir_svm_disasm_full				0x4
#define noir_svm_init_custom_vmcb			0x10000
#define noir_svm_run_custom_vcpu			0x10001
#define noir_svm_dump_vcpu_vmcb				0x10002
#define noir_svm_set_vcpu_options			0x10003

// Definition of Enabled features
#define noir_svm_vmcb_caching				1		// Bit 0
#define noir_svm_nested_paging				2		// Bit 1
#define noir_svm_flush_by_asid				4		// Bit 2
#define noir_svm_virtual_gif				8		// Bit 3
#define noir_svm_virtualized_vmls			16		// Bit 4
#define noir_svm_syscall_hook				32		// Bit 5
#define noir_svm_npt_with_hooks				64		// Bit 6
#define noir_svm_kva_shadow_present			128		// Bit 7

// Number of nested VMCBs to be cached.
#define noir_svm_cached_nested_vmcb			32

// When synchronizing nested VMCB for nested VM-Exits,
// most fields in VMCB requires synchronization.
#define noir_svm_nesting_vmcb_clean_bits	0xFFFFE817

struct _noir_npt_manager;

typedef enum _noir_svm_consistency_check_failure_id
{
	noir_svm_failure_unknown_failure,
	noir_svm_failure_cr0_cd0_nw1,
	noir_svm_failure_cr0_high,
	noir_svm_failure_cr3_mbz,
	noir_svm_failure_cr4_mbz,
	noir_svm_failure_dr6_high,
	noir_svm_failure_dr7_high,
	noir_svm_failure_efer_mbz,
	noir_svm_failure_efer_lme1_cr0_pg1_cr4_pae0,
	noir_svm_failure_efer_lme1_cr0_pg1_pe0,
	noir_svm_failure_efer_lme1_cr0_pg1_cr4_pae1_cs_l1_d1,
	noir_svm_failure_illegal_event_injection,
	noir_svm_failure_incanonical_segment_base,
}noir_svm_consistency_check_failure_id,*noir_svm_consistency_check_failure_id_p;

// Optimize Memory Usage of MSRPM and IOPM.
typedef struct _noir_svm_hvm
{
	memory_descriptor msrpm;
	memory_descriptor iopm;
	memory_descriptor blank_page;
	struct
	{
		u32 asid_limit;
		u32 capabilities;
	}virt_cap;
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

typedef struct _noir_svm_nested_vcpu_node
{
	memory_descriptor vmcb_t;
	memory_descriptor vmcb_c;
	u64 last_tsc;
	u64 entry_counter;
}noir_svm_nested_vcpu_node,*noir_svm_nested_vcpu_node_p;

typedef struct _noir_svm_nested_vcpu
{
	u64 hsave_gpa;
	void* hsave_hva;
	noir_svm_nested_vcpu_node nested_vmcb[noir_svm_cached_nested_vmcb];
	struct
	{
		u64 svme:1;
		u64 gif:1;
		u64 vgif_accel:1;
		u64 r_init:1;
		u64 pending_db:1;
		u64 pending_nmi:1;
		u64 pending_mc:1;
		u64 pending_init:1;
		u64 reserved:59;
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
	noir_mshv_vcpu mshvcpu;
	u32 proc_id;
	noir_svm_virtual_msr virtual_msr;
	noir_svm_nested_vcpu nested_hvm;
	noir_cvm_virtual_cpu cvm_state;
	u32 cpuid_fms;
	u16 enabled_feature;
	u8 status;
	u8 vcpu_property;
}noir_svm_vcpu,*noir_svm_vcpu_p;

struct _noir_svm_custom_vm;
struct _noir_npt_pdpte_descriptor;
struct _noir_npt_pde_descriptor;
struct _noir_npt_pte_descriptor;
union _amd64_npt_pml4e;

typedef struct _noir_svm_custom_npt_manager
{
	struct
	{
		union _amd64_npt_pml4e *virt;	// There are only 512 possible PML4Es in principle.
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

// Some bits are host-owned. Therefore, Guest's bit must be saved accordingly.
typedef union _noir_svm_shadowed_bits
{
	struct
	{
		u64 mce:1;
		u64 svme:1;
		u64 reserved:62;
	};
	u64 value;
}noir_svm_shadowed_bits,*noir_svm_shadowed_bits_p;

// Virtual Processor defined for Customizable VM.
typedef struct _noir_svm_custom_vcpu
{
	noir_cvm_virtual_cpu header;
	noir_svm_shadowed_bits shadowed_bits;
	struct _noir_svm_custom_vm *vm;
	memory_descriptor vmcb;
	memory_descriptor apic_backing;
	union
	{
		struct
		{
			u64 prev_virq:1;	// Required for interrupt-window interception.
			u64 reserved:62;
			u64 rescission:1;
		};
		u64 value;
	}special_state;
	u64 lasted_tsc;
	u32 proc_id;
}noir_svm_custom_vcpu,*noir_svm_custom_vcpu_p;

typedef struct _noir_svm_custom_vm
{
	noir_cvm_virtual_machine header;
	noir_svm_custom_vcpu_p* vcpu;
	u32 vcpu_count;
	u32 asid;
	memory_descriptor iopm;
	memory_descriptor msrpm;
	memory_descriptor msrpm_full;
	memory_descriptor avic_logical;
	memory_descriptor avic_physical;
	struct _noir_svm_custom_npt_manager nptm;
}noir_svm_custom_vm,*noir_svm_custom_vm_p;

// Layout of initial stack.
typedef struct _noir_svm_initial_stack
{
	u64 guest_vmcb_pa;
	u64 host_vmcb_pa;
	noir_svm_vcpu_p vcpu;
	noir_svm_custom_vcpu_p custom_vcpu;
	noir_svm_nested_vcpu_node_p nested_vcpu;
	u32 proc_id;
}noir_svm_initial_stack,*noir_svm_initial_stack_p;

#if defined(_svm_exit)
noir_svm_custom_vcpu nvc_svm_idle_cvcpu={0};
#else
extern noir_svm_custom_vcpu nvc_svm_idle_cvcpu;
#endif

u8 fastcall nvc_svm_subvert_processor_a(noir_svm_initial_stack_p host_rsp);
void nvc_svm_return(noir_gpr_state_p stack);
void nvc_svm_host_nmi_handler(void);
void nvc_svm_host_ready_nmi(void);
void fastcall nvc_svm_reserved_cpuid_handler(u32* info);
void nvc_svm_set_mshv_handler(bool option);
bool nvc_svm_build_cpuid_handler();
void nvc_svm_teardown_cpuid_handler();
bool nvc_svm_build_exit_handler();
void nvc_svm_teardown_exit_handler();
void nvc_svm_initialize_cvm_vmcb(noir_svm_custom_vcpu_p vmcb);
void nvc_svm_dump_guest_vcpu_state(noir_svm_custom_vcpu_p vcpu);
void nvc_svm_set_guest_vcpu_options(noir_svm_custom_vcpu_p vcpu);
void nvc_svm_switch_to_guest_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void nvc_svm_switch_to_host_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void nvc_svm_emulate_init_signal(noir_gpr_state_p gpr_state,void* vmcb,u32 cpuid_fms);
void nvc_svmn_synchronize_to_l2t_vmcb(noir_svm_nested_vcpu_node_p nvcpu);
void nvc_svmn_synchronize_to_l2c_vmcb(noir_svm_nested_vcpu_node_p nvcpu);
void nvc_svmn_insert_nested_vmcb(noir_svm_vcpu_p vcpu,memory_descriptor_p vmcb);
u8 nvc_npt_get_host_pat_index(u8 type);
noir_status nvc_svmc_initialize_cvm_module();
void nvc_svmc_finalize_cvm_module();