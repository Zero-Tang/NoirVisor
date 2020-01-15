/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines constants and structures for Intel IA-32 processors.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /include/ia32.h
*/

#include <nvdef.h>

//
#define ia32_cr4_vmxe			13
#define ia32_cr4_vmxe_bit		0x2000

//This is used for defining MSRs.
#define ia32_feature_control			0x3A
#define ia32_sysenter_cs				0x174
#define ia32_sysenter_esp				0x175
#define ia32_sysenter_eip				0x176
#define ia32_debug_control				0x1D9
#define ia32_pat						0x277
#define ia32_perf_global_ctrl			0x38F
#define ia32_vmx_basic					0x480
#define ia32_vmx_pinbased_ctrl			0x481
#define ia32_vmx_priproc_ctrl			0x482
#define ia32_vmx_exit_ctrl				0x483
#define ia32_vmx_entry_ctrl				0x484
#define ia32_vmx_misc					0x485
#define ia32_vmx_cr0_fixed0				0x486
#define ia32_vmx_cr0_fixed1				0x487
#define ia32_vmx_cr4_fixed0				0x488
#define ia32_vmx_cr4_fixed1				0x489
#define ia32_vmx_vmcs_enum				0x48A
#define ia32_vmx_2ndproc_ctrl			0x48B
#define ia32_vmx_ept_vpid_cap			0x48C
#define ia32_vmx_true_pinbased_ctrl		0x48D
#define ia32_vmx_true_priproc_ctrl		0x48E
#define ia32_vmx_true_exit_ctrl			0x48F
#define ia32_vmx_true_entry_ctrl		0x490
#define ia32_vmx_vmfunc					0x491
#define ia32_efer						0xC0000080
#define ia32_star						0xC0000081
#define ia32_lstar						0xC0000082
#define ia32_fmask						0xC0000084
#define ia32_fs_base					0xC0000100
#define ia32_gs_base					0xC0000101
#define ia32_kernel_gs_base				0xC0000102

//This is used for defining IA-32 architectural interrupt vectors.
#define ia32_divide_error				0
#define ia32_debug_exception			1
#define ia32_nmi_interrupt				2
#define ia32_breakpoint					3
#define ia32_overflow					4
#define ia32_exceed_bound_range			5
#define ia32_invalid_opcode				6
#define ia32_no_math_coprocessor		7
#define ia32_double_fault				8
#define ia32_segment_overrun			9
#define ia32_invalid_tss				10
#define ia32_segment_not_present		11
#define ia32_stack_segment_fault		12
#define ia32_general_protection			13
#define ia32_page_fault					14
#define ia32_x87_fp_error				16
#define ia32_alignment_check			17
#define ia32_machine_check				18
#define ia32_simd_exception				19
#define ia32_virtualization_exception	20

//This is used for defining IA-32 architectural cpuid flags.
#define ia32_cpuid_vmx			5
#define ia32_cpuid_vmx_bit		0x20

//This is used for defining IA-32 architectural cache types.
#define ia32_uncacheable		0
#define ia32_write_back			6