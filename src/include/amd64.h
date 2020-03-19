/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines constants and structures for AMD64 processors.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /include/amd64.h
*/

#include <nvdef.h>

#define amd64_efer_svme			12
#define amd64_efer_svme_bit		0x1000

// This is used for SVM Control Register Flags
#define amd64_vmcr_dpd				0
#define amd64_vmcr_r_init			1
#define amd64_vmcr_disa20m			2
#define amd64_vmcr_lock				3
#define amd64_vmcr_svmdis			4

// This is used for SMM Control Flags
#define amd64_smmctrl_dismiss			0
#define amd64_smmctrl_enter				1
#define amd64_smmctrl_smi_cycle			2
#define amd64_smmctrl_exit				3
#define amd64_smmctrl_rsm_cycle			4

// This is used for defining MSRs
#define amd64_sysenter_cs				0x174
#define amd64_sysenter_esp				0x175
#define amd64_sysenter_eip				0x176
#define amd64_debug_control				0x1D9
#define amd64_pat						0x277
#define amd64_efer						0xC0000080
#define amd64_star						0xC0000081
#define amd64_lstar						0xC0000082
#define amd64_cstar						0xC0000083
#define amd64_sfmask					0xC0000084
#define amd64_fs_base					0xC0000100
#define amd64_gs_base					0xC0000101
#define amd64_kernel_gs_base			0xC0000102
#define amd64_tsc_ratio					0xC0000104

// SVM/SMM Related MSRs
#define amd64_smbase					0xC0010111
#define amd64_smm_addr					0xC0010112
#define amd64_smm_mask					0xC0010113
#define amd64_vmcr						0xC0010114
#define amd64_ignne						0xC0010115
#define amd64_smm_ctrl					0xC0010116
#define amd64_hsave_pa					0xC0010117
#define amd64_svm_key					0xC0010118
#define amd64_smm_key					0xC0010119
#define amd64_local_smi_status			0xC001011A
#define amd64_apic_doorbell				0xC001011B
#define amd64_vmpage_flush				0xC001011E
#define amd64_svm_ghcb_pa				0xC0010130
#define amd64_sev_status				0xC0010131

// This is used for defining AMD64 architectural interrupt vectors.
#define amd64_divide_error				0
#define amd64_debug_exception			1
#define amd64_nmi_interrupt				2
#define amd64_breakpoint				3
#define amd64_overflow					4
#define amd64_exceed_bound_range		5
#define amd64_invalid_opcode			6
#define amd64_no_math_coprocessor		7
#define amd64_double_fault				8
#define amd64_segment_overrun			9
#define amd64_invalid_tss				10
#define amd64_segment_not_present		11
#define amd64_stack_segment_fault		12
#define amd64_general_protection		13
#define amd64_page_fault				14
#define amd64_x87_fp_error				16
#define amd64_alignment_check			17
#define amd64_machine_check				18
#define amd64_simd_exception			19

// This is used for defining AMD64 architectural cpuid flags.
#define amd64_cpuid_svm					2
#define amd64_cpuid_svm_bit				0x4
#define amd64_cpuid_hv_presence			31
#define amd64_cpuid_hv_presence_bit		0x80000000

// CPUID flags for SVM Features
#define amd64_cpuid_npt					0
#define amd64_cpuid_npt_bit				0x1
#define amd64_cpuid_lbr_virt			1
#define amd64_cpuid_lbr_virt_bit		0x2
#define amd64_cpuid_svm_lock			2
#define amd64_cpuid_svm_lock_bit		0x4
#define amd64_cpuid_nrips				3
#define amd64_cpuid_nrips_bit			0x8
#define amd64_cpuid_tsc_msr				4
#define amd64_cpuid_tsc_msr_bit			0x10
#define amd64_cpuid_vmcb_clean			5
#define amd64_cpuid_vmcb_clean_bit		0x20
#define amd64_cpuid_flush_asid			6
#define amd64_cpuid_flush_asid_bit		0x40
#define amd64_cpuid_decoder				7
#define amd64_cpuid_decoder_bit			0x80
#define amd64_cpuid_pause_flt			10
#define amd64_cpuid_pause_flt_bit		0x400
#define amd64_cpuid_pflt_thrshld		12
#define amd64_cpuid_pflt_thrshld_bit	0x1000
#define amd64_cpuid_avic				13
#define amd64_cpuid_avic_bit			0x2000
#define amd64_cpuid_vmlsvirt			15
#define amd64_cpuid_vmlsvirt_bit		0x8000
#define amd64_cpuid_vgif				16
#define amd64_cpuid_vgif_bit			0x10000