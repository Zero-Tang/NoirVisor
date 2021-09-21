/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is header file for Customizable VM of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/cvm_hvm.h
*/

#include <nvdef.h>
#include <nvbdk.h>

// CPUID Leaves for NoirVisor Customizable VM.
#define ncvm_cpuid_leaf_range_and_vendor_string			0x40000000
#define ncvm_cpuid_vendor_neutral_interface_id			0x40000001

#define ncvm_cpuid_leaf_limit		0x40000001

typedef enum _noir_cvm_intercept_code
{
	cv_invalid_state=0,
	cv_shutdown_condition=1,
	cv_memory_access=2,
	cv_init_signal=3,
	cv_hlt_instruction=4,
	cv_io_instruction=5,
	cv_cpuid_instruction=6,
	cv_rdmsr_instruction=7,
	cv_wrmsr_instruction=8,
	cv_cr_access=9,
	cv_dr_access=10,
	cv_hypercall=11,
	cv_exception=12,
	// The rest are scheduler-relevant.
	cv_scheduler_exit=0x80000000,
	cv_scheduler_pause=0x80000001
}noir_cvm_intercept_code,*noir_cvm_intercept_code_p;

typedef struct _noir_cvm_cr_access_context
{
	struct
	{
		u32 cr_number:4;
		u32 gpr_number:4;
		u32 mov_instruction:1;
		u32 write:1;
		u32 reserved0:22;
	};
}noir_cvm_cr_access_context,*noir_cvm_cr_access_context_p;

typedef struct _noir_cvm_dr_access_context
{
	struct
	{
		u32 dr_number:4;
		u32 gpr_number:4;
		u32 write:1;
		u32 reserved:23;
	};
}noir_cvm_dr_access_context,*noir_cvm_dr_access_context_p;

typedef struct _noir_cvm_exception_context
{
	struct
	{
		u32 vector:5;
		u32 ev_valid:1;
		u32 reserved:26;
	};
	u32 error_code;
	u64 pf_addr;
}noir_cvm_exception_context,*noir_cvm_exception_context_p;

typedef struct _noir_cvm_io_context
{
	struct
	{
		u16 io_type:1;
		u16 string:1;
		u16 repeat:1;
		u16 operand_size:3;
		u16 address_width:4;
		u16 reserved:6;
	}access;
	u16 port;
	u64 rax;
	u64 rcx;
	u64 rsi;
	u64 rdi;
	segment_register ds;
	segment_register es;
}noir_cvm_io_context,*noir_cvm_io_context_p;

typedef struct _noir_cvm_msr_context
{
	u32 eax;
	u32 edx;
	u32 ecx;
}noir_cvm_msr_context,*noir_cvm_msr_context_p;

typedef struct _noir_cvm_memory_access_context
{
	struct
	{
		u8 read:1;
		u8 write:1;
		u8 execute:1;
		u8 user:1;
		u8 fetched_bytes:4;
	}access;
	u8 instruction_bytes[15];
	u64 gpa;
}noir_cvm_memory_access_context,*noir_cvm_memory_access_context_p;

typedef struct _noir_cvm_cpuid_context
{
	struct
	{
		u32 a;
		u32 c;
	}leaf;
}noir_cvm_cpuid_context,*noir_cvm_cpuid_context_p;

typedef struct _noir_cvm_exit_context
{
	noir_cvm_intercept_code intercept_code;
	union
	{
		noir_cvm_cr_access_context cr_access;
		noir_cvm_dr_access_context dr_access;
		noir_cvm_exception_context exception;
		noir_cvm_io_context io;
		noir_cvm_io_context msr;
		noir_cvm_memory_access_context memory_access;
		noir_cvm_cpuid_context cpuid;
	};
	u64 rip;
	u64 rflags;
	struct
	{
		u32 cpl:2;
		u32 pe:1;
		u32 lm:1;
		u32 int_shadow:1;
		u32 instruction_length:4;
		u32 reserved:23;
	}vcpu_state;
}noir_cvm_exit_context,*noir_cvm_exit_context_p;

typedef struct _noir_seg_state
{
	segment_register cs;
	segment_register ds;
	segment_register es;
	segment_register fs;
	segment_register gs;
	segment_register ss;
	segment_register tr;
	segment_register gdtr;
	segment_register idtr;
	segment_register ldtr;
}noir_seg_state,*noir_seg_state_p;

typedef struct _noir_cr_state
{
	u64 cr0;
	u64 cr2;
	u64 cr3;
	u64 cr4;
	u64 cr8;
}noir_cr_state,*noir_cr_state_p;

typedef struct _noir_dr_state
{
	u64 dr0;
	u64 dr1;
	u64 dr2;
	u64 dr3;
	u64 dr6;
	u64 dr7;
}noir_dr_state,*noir_dr_state_p;

typedef struct _noir_msr_state
{
	u64 sysenter_cs;
	u64 sysenter_esp;
	u64 sysenter_eip;
	u64 pat;
	u64 efer;
	u64 star;
	u64 lstar;
	u64 cstar;
	u64 sfmask;
}noir_msr_state,*noir_msr_state_p;

typedef struct _noir_xcr_state
{
	u64 xcr0;
}noir_xcr_state,*noir_xcr_state_p;

typedef union _noir_cvm_vcpu_options
{
	struct
	{
		u32 intercept_cpuid:1;
		u32 intercept_rdmsr:1;
		u32 intercept_wrmsr:1;
		u32 intercept_exceptions:1;
		u32 intercept_cr0:1;
		u32 intercept_cr2:1;
		u32 intercept_cr3:1;
		u32 intercept_cr4:1;
		u32 intercept_drx:1;
		u32 intercept_pause:1;
		u32 emulate_umip:1;
		u32 reserved:21;
	};
	u32 value;
}noir_cvm_vcpu_options,*noir_cvm_vcpu_options_p;

typedef union _noir_cvm_vcpu_state_cache
{
	struct
	{
		u32 gprvalid:1;		// Includes rsp,rip,rflags.
		u32 cr_valid:1;		// Includes cr0,cr3,cr4,efer.
		u32 cr2valid:1;		// Includes cr2.
		u32 dr_valid:1;		// Includes dr6,dr7.
		u32 sr_valid:1;		// Includes cs,ds,es,ss.
		u32 fg_valid:1;		// Includes fs,gs,kgsbase.
		u32 dt_valid:1;		// Includes idtr,gdtr.
		u32 lt_valid:1;		// Includes tr,ldtr.
		u32 sc_valid:1;		// Includes star,lstar,cstar,sfmask.
		u32 se_valid:1;		// Includes esp,eip,cs for sysenter.
		u32 tp_valid:1;		// Includes cr8.tpr.
		u32 reserved:22;
	};
	u32 value;
}noir_cvm_vcpu_state_cache,*noir_cvm_vcpu_state_cache_p;

typedef struct _noir_cvm_virtual_cpu
{
	noir_gpr_state gpr;
	noir_seg_state seg;
	noir_cr_state crs;
	noir_dr_state drs;
	noir_msr_state msrs;
	noir_xcr_state xcrs;
	void* xsave_area;
	u64 rflags;
	u64 rip;
	noir_cvm_exit_context exit_context;
	noir_cvm_vcpu_options vcpu_options;
	noir_cvm_vcpu_state_cache state_cache;
	u32 exception_bitmap;
	u32 proc_id;
}noir_cvm_virtual_cpu,*noir_cvm_virtual_cpu_p;

typedef struct _noir_cvm_address_mapping
{
	u64 gpa;
	u64 hpa;
	u32 pages;
	union
	{
		struct
		{
			u32 present:1;
			u32 write:1;
			u32 execute:1;
			u32 user:1;
			u32 caching:3;
			u32 psize:2;
			u32 reserved:23;
		};
		u32 value;
	}attributes;
}noir_cvm_address_mapping,*noir_cvm_address_mapping_p;

typedef struct _noir_cvm_virtual_machine
{
	list_entry active_vm_list;
	u32 vcpu_count;
	u32 mappings_count;
	u32 pid;
	noir_reslock vcpu_list_lock;
	noir_cvm_virtual_cpu_p vcpu;
	noir_cvm_address_mapping mappings[1];
}noir_cvm_virtual_machine,*noir_cvm_virtual_machine_p;

#if defined(_central_hvm)
noir_status nvc_svmc_create_vm(noir_cvm_virtual_machine_p* virtual_machine);
void nvc_svmc_release_vm(noir_cvm_virtual_machine_p vm);
noir_status nvc_svmc_create_vcpu(noir_cvm_virtual_cpu_p* virtual_cpu,noir_cvm_virtual_machine_p virtual_machine);
void nvc_svmc_release_vcpu(noir_cvm_virtual_cpu_p vcpu);
u32 nvc_svmc_get_vm_asid(noir_cvm_virtual_machine_p vm);

// Idle VM is to be considered as the List Head.
noir_cvm_virtual_machine noir_idle_vm={0};
noir_reslock noir_vm_list_lock=null;
#else
extern noir_cvm_virtual_machine noir_idle_vm;
extern noir_reslock noir_vm_list_lock;
#endif