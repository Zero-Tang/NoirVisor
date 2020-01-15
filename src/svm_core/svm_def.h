/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines constants and basic structures for AMD-V.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /svm_core/svm_def.h
*/

#include <nvdef.h>

#define svm_attrib(a)		(u16)(a&0xFF)|((a&0xF000)>>4)

typedef union _nvc_svm_cr_intercept
{
	struct
	{
		u16 cr0:1;
		u16 cr1:1;
		u16 cr2:1;
		u16 cr3:1;
		u16 cr4:1;
		u16 cr5:1;
		u16 cr6:1;
		u16 cr7:1;
		u16 cr8:1;
		u16 cr9:1;
		u16 cr10:1;
		u16 cr11:1;
		u16 cr12:1;
		u16 cr13:1;
		u16 cr14:1;
		u16 cr15:1;
	};
	u16 value;
}nvc_svm_cr_intercept,*nvc_svm_cr_intercept_p;

typedef union _nvc_svm_cra_intercept
{
	struct
	{
		nvc_svm_cr_intercept read;
		nvc_svm_cr_intercept write;
	};
	u32 value;
}nvc_svm_cra_intercept,*nvc_svm_cra_intercept_p;

typedef union _nvc_svm_dr_intercept
{
	struct
	{
		u16 dr0:1;
		u16 dr1:1;
		u16 dr2:1;
		u16 dr3:1;
		u16 dr4:1;
		u16 dr5:1;
		u16 dr6:1;
		u16 dr7:1;
		u16 dr8:1;
		u16 dr9:1;
		u16 dr10:1;
		u16 dr11:1;
		u16 dr12:1;
		u16 dr13:1;
		u16 dr14:1;
		u16 dr15:1;
	};
	u16 value;
}nvc_svm_dr_intercept,*nvc_svm_dr_intercept_p;

typedef union _nvc_svm_dra_intercept
{
	struct
	{
		nvc_svm_dr_intercept read;
		nvc_svm_dr_intercept write;
	};
	u32 value;
}nvc_svm_dra_intercept,*nvc_svm_dra_intercept_p;

typedef union _nvc_svm_instruction_intercept1
{
	struct
	{
		u32 intercept_intr:1;
		u32 intercept_nmi:1;
		u32 intercept_smi:1;
		u32 intercept_init:1;
		u32 intercept_vint:1;
		u32 intercept_cr0_tsmp:1;
		u32 intercept_sidt:1;
		u32 intercept_sgdt:1;
		u32 intercept_sldt:1;
		u32 intercept_str:1;
		u32 intercept_lidt:1;
		u32 intercept_lgdt:1;
		u32 intercept_lldt:1;
		u32 intercept_ltr:1;
		u32 intercept_rdtsc:1;
		u32 intercept_rdpmc:1;
		u32 intercept_pushf:1;
		u32 intercept_popf:1;
		u32 intercept_cpuid:1;
		u32 intercept_rsm:1;
		u32 intercept_iret:1;
		u32 intercept_int:1;
		u32 intercept_invd:1;
		u32 intercept_pause:1;
		u32 intercept_hlt:1;
		u32 intercept_invlpg:1;
		u32 intercept_invlpga:1;
		u32 intercept_io:1;
		u32 intercept_msr:1;
		u32 intercept_task_switch:1;
		u32 intercept_ferr_freeze:1;
		u32 intercept_shutdown:1;
	};
	u32 value;
}nvc_svm_instruction_intercept1,*nvc_svm_instruction_intercept1_p;

typedef union _nvc_svm_instruction_intercept2
{
	struct
	{
		u16 intercept_vmrun:1;
		u16 intercept_vmmcall:1;
		u16 intercept_vmload:1;
		u16 intercept_vmsave:1;
		u16 intercept_stgi:1;
		u16 intercept_clgi:1;
		u16 intercept_skinit:1;
		u16 intercept_rdtscp:1;
		u16 intercept_icebp:1;
		u16 intercept_wbinvd:1;
		u16 intercept_monitor:1;
		u16 intercept_mwait:1;
		u16 intercept_mwait_c:1;
		u16 intercept_xsetbv:1;
		u16 reserved1:1;
		u16 intercept_post_efer_write:1;
	};
	u16 value;
}nvc_svm_instruction_intercept2,*nvc_svm_instruction_intercept2_p;

#define nvc_svm_tlb_control_do_nothing			0
#define nvc_svm_tlb_control_flush_entire		1
#define nvc_svm_tlb_control_flush_guest			3
#define nvc_svm_tlb_control_flush_non_global	7

typedef union _nvc_svm_asid_control
{
	struct
	{
		u32 guest_asid;
		u8 tlb_control;
		u8 reserved[3];
	};
	u64 value;
}nvc_svm_asid_control,*nvc_svm_asid_control_p;

typedef union _nvc_svm_avic_control
{
	struct
	{
		u64 virtual_tpr:8;
		u64 virtual_irq:1;
		u64 virtual_gif:1;
		u64 reserved1:6;
		u64 virtual_interrupt_priority:4;
		u64 ignore_virtual_tpr:1;
		u64 reserved2:3;
		u64 virtual_interrupt_masking:1;
		u64 enable_virtual_gif:1;
		u64 reserved3:5;
		u64 enable_avic:1;
		u64 virtual_interrupt_vector:8;
		u64 reserved4:24;
	};
	u64 value;
}nvc_svm_avic_control,*nvc_svm_avic_control_p;

typedef union _nvc_svm_guest_interrupt
{
	struct
	{
		u64 interrupt_shadow:1;
		u64 guest_interrupt_mask:1;
		u64 reserved:62;
	};
	u64 value;
}nvc_svm_guest_interrupt,*nvc_svm_guest_interrupt_p;

typedef union _nvc_svm_npt_control
{
	struct
	{
		u64 enable_npt:1;
		u64 enable_sev:1;
		u64 enable_sev_es:1;
		u64 gmet:1;
		u64 reserved1:1;
		u64 virtual_transparent_encryption:1;
		u64 reserved2:58;
	};
	u64 value;
}nvc_svm_npt_control,*nvc_svm_npt_control_p;

typedef union _nvc_svm_lbr_virtualization_control
{
	struct
	{
		u64 enable_lbr_virtualization:1;
		u64 virtualize_vmsave_vmload:1;
		u64 reserved:62;
	};
	u64 value;
}nvc_svm_lbr_virtualization_control,*nvc_svm_lbr_virtualization_control_p;

typedef struct _nvc_svm_fetched_instruction
{
	u8 number_of_bytes;
	u8 instruction_bytes[15];
}nvc_svm_fetched_instruction,*nvc_svm_fetched_instruction_p;

typedef struct _nvc_svm_avic_physical_table
{
	struct
	{
		u64 max_index:8;
		u64 reserved1:4;
		u64 table_pointer:40;
		u64 reserved2:12;
	};
	u64 value;
}nvc_svm_avic_physical_table,*nvc_svm_avic_physical_table_p;