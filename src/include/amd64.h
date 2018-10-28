/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file defines constants and structures for AMD64 processors.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /include/amd64.h
*/

#include <nvdef.h>

#define amd64_efer_svme			12
#define amd64_efer_svme_bit		0x1000

//This is used for defining MSRs
#define amd64_sysenter_cs				0x174
#define amd64_sysenter_esp				0x175
#define amd64_sysenter_eip				0x176
#define amd64_debug_control				0x1D9
#define amd64_pat						0x277
#define amd64_efer						0xC0000080
#define amd64_star						0xC0000081
#define amd64_lstar						0xC0000082
#define amd64_fmask						0xC0000084
#define amd64_fs_base					0xC0000100
#define amd64_gs_base					0xC0000101
#define amd64_kernel_gs_base			0xC0000102
#define amd64_vmcr						0xC0010114
#define amd64_hsave_pa					0xC0010117

//This is used for defining AMD64 architectural interrupt vectors.
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

//This is used for defining AMD64 architectural cpuid flags.
#define amd64_cpuid_svm				2
#define amd64_cpuid_svm_bit			0x4
#define amd64_cpuid_decoder			7
#define amd64_cpuid_decoder_bit		0x80
#define amd64_cpuid_nrips			3
#define amd64_cpuid_nrips_bit		0x8