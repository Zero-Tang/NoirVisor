/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file defines structures and constants for SVM Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

// Definition of vmmcall Codes
#define noir_svm_callexit					0x1		// Restore from subversion.
#define noir_svm_call_flush_tlb				0x2		// Flush TLBs.
#define noir_svm_call_signal_runtime		0x3		// Signal the hypervisor that UEFI is entering runtime stage.

#define noir_svm_init_custom_vmcb			0x10000
#define noir_svm_run_custom_vcpu			0x10001
#define noir_svm_dump_vcpu_vmcb				0x10002
#define noir_svm_set_vcpu_options			0x10003
#define noir_svm_guest_memory_operation		0x10004
#define noir_svm_nsv_reassign_rmt			0x10005
#define noir_svm_nsv_remap_by_rmt			0x10006
#define noir_svm_nsv_crypto_for_rmt			0x10007

// Definition of Enabled features
#define noir_svm_vmcb_caching				1		// Bit 0
#define noir_svm_nested_paging				2		// Bit 1
#define noir_svm_flush_by_asid				4		// Bit 2
#define noir_svm_virtual_gif				8		// Bit 3
#define noir_svm_virtualized_vmls			16		// Bit 4
#define noir_svm_syscall_hook				32		// Bit 5
#define noir_svm_npt_with_hooks				64		// Bit 6
#define noir_svm_kva_shadow_present			128		// Bit 7

// Definition of vCPU APIC Global Status Bit Fields
#define noir_svm_sipi_sent					0

// Number of nested VMCBs to be cached.
#define noir_svm_cached_nested_vmcb			16

// Definitions of CVM CPUID maskings
#define noir_svm_cpuid_cvmask0_ecx_fn0000_0001	0xE2D83209
#define noir_svm_cpuid_cvmask1_ecx_fn0000_0001	0x80000000
#define noir_svm_cpuid_cvmask0_edx_fn0000_0001	0x078BBB7F
#define noir_svm_cpuid_cvmask0_ebx_fn0000_0007	0x01DC0589
#define noir_svm_cpuid_cvmask0_ecx_fn0000_0007	0x00000004
#define noir_svm_cpuid_cvmask0_ecx_fn8000_0001	0x002001FB
#define noir_svm_cpuid_cvmask0_edx_fn8000_0001	0xEFDFBB7F

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
	struct _noir_npt_manager* primary_nptm;
#if !defined(_hv_type1)
	struct _noir_npt_manager* secondary_nptm;
#else
	u64 apic_base;
#endif
	struct
	{
		u32 asid_limit;
		u32 capabilities;
	}virt_cap;
	struct
	{
		u32 capabilities;
		u32 mem_virt_cap;
		u32 simultaneous;
		u32 minimum_asid;
	}sev_cap;
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
	union
	{
		struct
		{
			u64 clean:1;
			u64 reserved:63;
		};
		u64 value;
	}flags;
}noir_svm_nested_vcpu_node,*noir_svm_nested_vcpu_node_p;

typedef struct _noir_svm_nested_vcpu
{
	u64 hsave_gpa;
	void* hsave_hva;
	noir_svm_nested_vcpu_node node_pool[noir_svm_cached_nested_vmcb];
	struct
	{
		u64 svme:1;
		u64 forward:1;
		u64 gif:1;
		u64 vgif_accel:1;
		u64 r_init:1;
		u64 pending_db:1;
		u64 pending_nmi:1;
		u64 pending_mc:1;
		u64 pending_init:1;
		u64 reserved:55;
	};
}noir_svm_nested_vcpu,*noir_svm_nested_vcpu_p;

union _amd64_npt_pml4e;
union _amd64_npt_pdpte;
union _amd64_npt_pde;
union _amd64_npt_pte;

typedef struct _noir_svm_fallback_info
{
	bool valid;
	u32 offset;
	u64 value;
	size_t field_size;
	ulong_ptr rip;
	u64 fault_info;
}noir_svm_fallback_info,*noir_svm_fallback_info_p;

// Virtual Processor defined for host.
typedef struct _noir_svm_vcpu
{
	memory_descriptor vmcb;
	memory_descriptor hvmcb;
	memory_descriptor hsave;
	void* hv_stack;
	struct _noir_svm_vcpu *self;
	noir_svm_hvm_p relative_hvm;
	struct
	{
		bool valid;
		u32 offset;
		u64 value;
		size_t field_size;
		ulong_ptr rip;
		u64 fault_info;
	}fallback;
	noir_mshv_vcpu mshvcpu;
	noir_svm_virtual_msr virtual_msr;
	noir_svm_nested_vcpu nested_hvm;
	noir_cvm_virtual_cpu cvm_state;
	union
	{
		struct
		{
			u64 x2apic:1;
			u64 reserved:63;
		};
		u64 value;
	}flags;
	u32 cpuid_fms;
	u32 enabled_feature;
	u32v global_state;
	u32 proc_id;
	u32 x2apic_id;
	u8 apic_id;
	u8 sipi_vector;
	u8 status;
	u8 vcpu_property;
	// Align GDT, IDT and TSS at 16-byte boundary!
	align_at(16) noir_gate_descriptor idt_buffer[0x100];
	align_at(16) noir_segment_descriptor gdt_buffer[0x100];
	align_at(16) u8 tss_buffer[0x100];
	// Force the vCPU to have the size of multiple pages!
	align_at(page_size) u8 alignment_holder[0];
}noir_svm_vcpu,*noir_svm_vcpu_p;

struct _noir_svm_custom_vm;
struct _noir_svm_secure_vm;
struct _noir_npt_pdpte_descriptor;
struct _noir_npt_pde_descriptor;
struct _noir_npt_pte_descriptor;

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
		u64 tf:1;	// Shadowed bit for MTF
		u64 btf:1;	// Shadowed bit for MTF
		u64 sce:1;	// Shadowed bit for MTF
		u64 reserved:59;
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
			u64 prev_nmi:1;
			u64 mtf_active:1;
			u64 gif:1;
			u64 reserved:57;
			u64 switch_success:1;
			u64 hv_mtf:1;		// Trap-Flag by NoirVisor.
			u64 rescission:1;
		};
		u64 value;
	}special_state;
	u64 lasted_tsc;
	u32 proc_id;	// The physical processor id this vCPU was scheduled to
	u32 vcpu_id;	// The virtual processor id of this vCPU
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
	struct _noir_svm_custom_npt_manager smm_nptm;
}noir_svm_custom_vm,*noir_svm_custom_vm_p;

// Virtual Processor defined for Encrypted CVM.
typedef struct _noir_svm_secure_vcpu
{
	struct _noir_svm_secure_vm *vm;
	memory_descriptor vmcb;
	// Albeit VMSA is used only when SEV-ES is enabled.
	// Let's emulate the usage and discard the usage of CVM vCPU Header.
	memory_descriptor vmsa;
	u32 proc_id;
	u32 vcpu_id;
}noir_svm_secure_vcpu,*noir_svm_secure_vcpu_p;

typedef struct _noir_svm_secure_vm
{
	noir_cvm_virtual_machine header;
	noir_svm_secure_vcpu_p vcpu;
	noir_svm_custom_npt_manager_p nptm;
	u32 vcpu_count;
	u32 asid;
	memory_descriptor iopm;
	memory_descriptor msrpm;
	memory_descriptor msrpm_full;
}noir_svm_secure_vm,*noir_svm_secure_vm_p;

// Layout of initial stack.
typedef struct _noir_svm_initial_stack
{
	u64 guest_vmcb_pa;
	u64 host_vmcb_pa;
	noir_svm_vcpu_p vcpu;
	noir_svm_custom_vcpu_p custom_vcpu;
	noir_svm_nested_vcpu_node_p nested_vcpu;
	u32 proc_id;
	u32 reserved;
}noir_svm_initial_stack,*noir_svm_initial_stack_p;

#define noir_svm_get_loader_stack(p)	(noir_svm_initial_stack_p)(((ulong_ptr)p+nvc_stack_size-sizeof(noir_svm_initial_stack))&0xfffffffffffffff0)

#if defined(_svm_exit)
noir_svm_custom_vcpu nvc_svm_idle_cvcpu={0};
#else
extern noir_svm_custom_vcpu nvc_svm_idle_cvcpu;
#endif

u8 fastcall nvc_svm_subvert_processor_a(noir_svm_initial_stack_p host_rsp);
void nvc_svm_return(noir_gpr_state_p stack);
void nvc_svm_host_nmi_handler(void);
void nvc_svm_host_ready_nmi(void);
void nvc_svm_guest_start(void);
void fastcall nvc_svm_reserved_cpuid_handler(u32* info);
void nvc_svm_set_mshv_handler(bool option);
void nvc_svm_initialize_cvm_vmcb(noir_svm_custom_vcpu_p vmcb);
void nvc_svm_dump_guest_vcpu_state(noir_svm_custom_vcpu_p vcpu);
void nvc_svm_dump_guest_segments(noir_cvm_virtual_cpu_p vcpu,void* vmcb);
void nvc_svm_dump_guest_fs_gs(noir_cvm_virtual_cpu_p vcpu,void* vmcb);
void nvc_svm_set_guest_vcpu_options(noir_svm_custom_vcpu_p vcpu);
void nvc_svm_switch_to_guest_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void nvc_svm_switch_to_host_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void nvc_svm_inject_cvm_exception(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu,u8 vector,bool ev,u32 error_code,u64 pf_addr,u8 fetch_length,u8p fetched_instruction);
bool nvc_svm_nsv_save_guest_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
bool nvc_svm_nsv_load_guest_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void nvc_svm_load_basic_exit_context(noir_svm_custom_vcpu_p vcpu);
void nvc_svm_operate_guest_memory(noir_cvm_gmem_op_context_p context);
void nvc_svm_emulate_init_signal(noir_gpr_state_p gpr_state,void* vmcb,u32 cpuid_fms);
noir_svm_nested_vcpu_node_p nvc_svm_get_nested_vcpu_node(noir_svm_nested_vcpu_p nvcpu,u64 vmcb);
void noir_hvcode nvc_svm_switch_to_nested_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu_node);
void noir_hvcode nvc_svm_switch_from_nested_vcpu(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void noir_hvcode nvc_svm_clear_nested_gif(noir_svm_vcpu_p vcpu);
void noir_hvcode nvc_svm_set_nested_gif(noir_svm_vcpu_p vcpu);
bool nvc_svmc_get_physical_mapping(noir_svm_custom_npt_manager_p npt_manager,u64 gpa,u64p hpa,bool r,bool w,bool x);
void nvc_npt_reassign_page_ownership_hvrt(noir_svm_vcpu_p vcpu,noir_rmt_remap_context_p context);
bool nvc_npt_reassign_page_ownership(u64p hpa,u64p gpa,u32 pages,u32 asid,bool shared,u8 ownership);
bool nvc_npt_reassign_cvm_all_pages_ownership(noir_svm_custom_vm_p vm,u32 asid,bool shared,u8 ownership);
u8 nvc_npt_get_host_pat_index(u8 type);
noir_status nvc_svmc_initialize_cvm_module();
void nvc_svmc_finalize_cvm_module();

u8 nvc_emu_decode_npiep_instruction(noir_cvm_virtual_cpu_p vcpu,u8p buffer,size_t buffer_limit,noir_npiep_operand_p operand);