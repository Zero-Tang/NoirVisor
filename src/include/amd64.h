/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file defines constants and structures for AMD64 processors.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /include/amd64.h
*/

#include <nvdef.h>

// CR0 Bit Fields
#define amd64_cr0_pe		0
#define amd64_cr0_mp		1
#define amd64_cr0_em		2
#define amd64_cr0_ts		3
#define amd64_cr0_et		4
#define amd64_cr0_ne		5
#define amd64_cr0_wp		16
#define amd64_cr0_am		18
#define amd64_cr0_nw		29
#define amd64_cr0_cd		30
#define amd64_cr0_pg		31
#define amd64_cr0_pe_bit	0x1
#define amd64_cr0_mp_bit	0x2
#define amd64_cr0_em_bit	0x4
#define amd64_cr0_ts_bit	0x8
#define amd64_cr0_et_bit	0x10
#define amd64_cr0_ne_bit	0x20
#define amd64_cr0_wp_bit	0x10000
#define amd64_cr0_am_bit	0x40000
#define amd64_cr0_nw_bit	0x20000000
#define amd64_cr0_cd_bit	0x40000000
#define amd64_cr0_pg_bit	0x80000000

// CR4 Bit Fields
#define amd64_cr4_vme				0
#define amd64_cr4_pvi				1
#define amd64_cr4_tsd				2
#define amd64_cr4_de				3
#define amd64_cr4_pse				4
#define amd64_cr4_pae				5
#define amd64_cr4_mce				6
#define amd64_cr4_pge				7
#define amd64_cr4_pce				8
#define amd64_cr4_osfxsr			9
#define amd64_cr4_osxmmexcept		10
#define amd64_cr4_umip				11
#define amd64_cr4_fsgsbase			16
#define amd64_cr4_pcide				17
#define amd64_cr4_osxsave			18
#define amd64_cr4_smep				20
#define amd64_cr4_smap				21
#define amd64_cr4_pke				22
#define amd64_cr4_cet				23

// EFER Bit Fields
#define amd64_efer_sce			0
#define amd64_efer_lme			8
#define amd64_efer_lma			10
#define amd64_efer_nxe			11
#define amd64_efer_svme			12
#define amd64_efer_lmsle		13
#define amd64_efer_ffxsr		14
#define amd64_efer_tce			15
#define amd64_efer_mcommit		17
#define amd64_efer_intwb		18
#define amd64_efer_sce_bit		0x1
#define amd64_efer_lme_bit		0x100
#define amd64_efer_lma_bit		0x400
#define amd64_efer_nxe_bit		0x800
#define amd64_efer_svme_bit		0x1000
#define amd64_efer_lmsle_bit	0x2000
#define amd64_efer_ffxsr_bit	0x4000
#define amd64_efer_tce_bit		0x8000
#define amd64_efer_mcommit_bit	0x20000
#define amd64_efer_intwb_bit	0x40000

// DR6 Bit Fields
#define amd64_dr6_b0			0
#define amd64_dr6_b1			1
#define amd64_dr6_b2			2
#define amd64_dr6_b3			3
#define amd64_dr6_bd			13
#define amd64_dr6_bs			14
#define amd64_dr6_bt			15

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
#define amd64_mtrr_cap					0xFE
#define amd64_sysenter_cs				0x174
#define amd64_sysenter_esp				0x175
#define amd64_sysenter_eip				0x176
#define amd64_debug_control				0x1D9
#define amd64_mtrr_phys_base0			0x200
#define amd64_mtrr_phys_mask0			0x201
#define amd64_mtrr_phys_base1			0x202
#define amd64_mtrr_phys_mask1			0x203
#define amd64_mtrr_phys_base2			0x204
#define amd64_mtrr_phys_mask2			0x205
#define amd64_mtrr_phys_base3			0x206
#define amd64_mtrr_phys_mask3			0x207
#define amd64_mtrr_phys_base4			0x208
#define amd64_mtrr_phys_mask4			0x209
#define amd64_mtrr_phys_base5			0x20A
#define amd64_mtrr_phys_mask5			0x20B
#define amd64_mtrr_phys_base6			0x20C
#define amd64_mtrr_phys_mask6			0x20D
#define amd64_mtrr_phys_base7			0x20E
#define amd64_mtrr_phys_mask7			0x20F
#define amd64_mtrr_fix64k_00000			0x250
#define amd64_mtrr_fix16k_80000			0x258
#define amd64_mtrr_fix16k_a0000			0x259
#define amd64_mtrr_fix4k_c0000			0x268
#define amd64_mtrr_fix4k_c8000			0x269
#define amd64_mtrr_fix4k_d0000			0x26A
#define amd64_mtrr_fix4k_d8000			0x26B
#define amd64_mtrr_fix4k_e0000			0x26C
#define amd64_mtrr_fix4k_e8000			0x26D
#define amd64_mtrr_fix4k_f0000			0x26E
#define amd64_mtrr_fix4k_f8000			0x26F
#define amd64_pat						0x277
#define amd64_mtrr_def_type				0x2FF
#define amd64_efer						0xC0000080
#define amd64_star						0xC0000081
#define amd64_lstar						0xC0000082
#define amd64_cstar						0xC0000083
#define amd64_sfmask					0xC0000084
#define amd64_fs_base					0xC0000100
#define amd64_gs_base					0xC0000101
#define amd64_kernel_gs_base			0xC0000102
#define amd64_tsc_ratio					0xC0000104
#define amd64_hwcr						0xC0010015

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
#define amd64_rmp_base					0xC0010132
#define amd64_rmp_end					0xC0010133

// Index of Standard Leaves
#define amd64_cpuid_std_max_num_vstr		0x0
#define amd64_cpuid_std_proc_feature		0x1
#define amd64_cpuid_std_monitor_feat		0x5
#define amd64_cpuid_std_thermal_feat		0x6
#define amd64_cpuid_std_struct_extid		0x7
#define amd64_cpuid_std_pestate_enum		0xD

// Index of Extended Leaves
#define amd64_cpuid_ext_max_num_vstr		0x80000000
#define amd64_cpuid_ext_proc_feature		0x80000001
#define amd64_cpuid_ext_brand_str_p1		0x80000002
#define amd64_cpuid_ext_brand_str_p2		0x80000003
#define amd64_cpuid_ext_brand_str_p3		0x80000004
#define amd64_cpuid_ext_caching_tlbs		0x80000005
#define amd64_cpuid_ext_l23cache_tlb		0x80000006
#define amd64_cpuid_ext_powermgr_ras		0x80000007
#define amd64_cpuid_ext_pcap_prm_eid		0x80000008
#define amd64_cpuid_ext_svm_features		0x8000000A
#define amd64_cpuid_ext_svm_tlbs_1gb		0x80000019
#define amd64_cpuid_ext_ins_optimize		0x8000001A
#define amd64_cpuid_ext_ins_sampling		0x8000001B
#define amd64_cpuid_ext_lw_profiling		0x8000001C
#define amd64_cpuid_ext_cache_topinf		0x8000001D
#define amd64_cpuid_ext_proc_topoinf		0x8000001E
#define amd64_cpuid_ext_mem_crypting		0x8000001F

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
#define amd64_control_protection		21
#define amd64_hypervisor_injection		28
#define amd64_vmm_communication			29
#define amd64_security_exception		30

// #SX Exception Error Codes
#define amd64_sx_init_redirection		1

// This is used for defining AMD64 architectural memory types.
#define amd64_memory_type_uc			0x00	// Uncacheable
#define amd64_memory_type_wc			0x01	// Write-Combining
#define amd64_memory_type_wt			0x04	// Write-Through
#define amd64_memory_type_wp			0x05	// Write-Protect
#define amd64_memory_type_wb			0x06	// Write-Back
#define amd64_memory_type_ucm			0x07	// UC-minus

// This is used for defining AMD64 architectural cpuid flags.
#define amd64_cpuid_svm					2
#define amd64_cpuid_svm_bit				0x4
#define amd64_cpuid_hv_presence			31
#define amd64_cpuid_hv_presence_bit		0x80000000

// This is used for defining AMD64 RFlags bits.
#define amd64_rflags_cf			0
#define amd64_rflags_pf			2
#define amd64_rflags_af			4
#define amd64_rflags_zf			6
#define amd64_rflags_sf			7
#define amd64_rflags_tf			8
#define amd64_rflags_if			9
#define amd64_rflags_df			10
#define amd64_rflags_of			11
#define amd64_rflags_nt			14
#define amd64_rflags_rf			16
#define amd64_rflags_vm			17
#define amd64_rflags_ac			18
#define amd64_rflags_vif		19
#define amd64_rflags_vip		20
#define amd64_rflags_id			21

#define amd64_rflags_iopl(f)	((f&0x3000)>>12)

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

typedef union _amd64_page_fault_error_code
{
	struct
	{
		u32 present:1;			// Bit	0
		u32 write:1;			// Bit	1
		u32 user:1;				// Bit	2
		u32 reserved:1;			// Bit	3
		u32 execution:1;		// Bit	4
		u32 protection_key:1;	// Bit	5
		u32 shadow_stack:1;		// Bit	6
		u32 reserved0:24;		// Bits	7-30
		u32 rmp_violation:1;	// Bit	31
	};
	u32 value;
}amd64_page_fault_error_code,*amd64_page_fault_error_code_p;