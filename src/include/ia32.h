/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines constants and structures for Intel IA-32 processors.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /include/ia32.h
*/

#include <nvdef.h>

// CR0 Bit Fields
#define ia32_cr0_pe		0
#define ia32_cr0_mp		1
#define ia32_cr0_em		2
#define ia32_cr0_ts		3
#define ia32_cr0_et		4
#define ia32_cr0_ne		5
#define ia32_cr0_wp		16
#define ia32_cr0_am		18
#define ia32_cr0_nw		29
#define ia32_cr0_cd		30
#define ia32_cr0_pg		31
#define ia32_cr0_pe_bit	0x1
#define ia32_cr0_mp_bit	0x2
#define ia32_cr0_em_bit	0x4
#define ia32_cr0_ts_bit	0x8
#define ia32_cr0_et_bit	0x10
#define ia32_cr0_ne_bit	0x20
#define ia32_cr0_wp_bit	0x10000
#define ia32_cr0_am_bit	0x40000
#define ia32_cr0_nw_bit	0x20000000
#define ia32_cr0_cd_bit	0x40000000
#define ia32_cr0_pg_bit	0x80000000

// CR4 Bit Fields
#define ia32_cr4_vme				0
#define ia32_cr4_pvi				1
#define ia32_cr4_tsd				2
#define ia32_cr4_de					3
#define ia32_cr4_pse				4
#define ia32_cr4_pae				5
#define ia32_cr4_mce				6
#define ia32_cr4_pge				7
#define ia32_cr4_pce				8
#define ia32_cr4_osfxsr				9
#define ia32_cr4_osxmmexcept		10
#define ia32_cr4_umip				11
#define ia32_cr4_la57				12
#define ia32_cr4_vmxe				13
#define ia32_cr4_smxe				14
#define ia32_cr4_fsgsbase			16
#define ia32_cr4_pcide				17
#define ia32_cr4_osxsave			18
#define ia32_cr4_kl					19
#define ia32_cr4_smep				20
#define ia32_cr4_smap				21
#define ia32_cr4_pke				22
#define ia32_cr4_cet				23
#define ia32_cr4_pks				24
#define ia32_cr4_vme_bit			0x1
#define ia32_cr4_pvi_bit			0x2
#define ia32_cr4_tsd_bit			0x4
#define ia32_cr4_de_bit				0x8
#define ia32_cr4_pse_bit			0x10
#define ia32_cr4_pae_bit			0x20
#define ia32_cr4_mce_bit			0x40
#define ia32_cr4_pge_bit			0x80
#define ia32_cr4_pce_bit			0x100
#define ia32_cr4_osfxsr_bit			0x200
#define ia32_cr4_osxmmexcept_bit	0x400
#define ia32_cr4_umip_bit			0x800
#define ia32_cr4_la57_bit			0x1000
#define ia32_cr4_vmxe_bit			0x2000
#define ia32_cr4_smxe_bit			0x4000
#define ia32_cr4_fsgsbase_bit		0x10000
#define ia32_cr4_pcide_bit			0x20000
#define ia32_cr4_osxsave_bit		0x40000
#define ia32_cr4_kl_bit				0x80000
#define ia32_cr4_smep_bit			0x100000
#define ia32_cr4_smap_bit			0x200000
#define ia32_cr4_pke_bit			0x400000
#define ia32_cr4_cet_bit			0x800000
#define ia32_cr4_pks_bit			0x1000000

// This is used for defining MSRs.
#define ia32_feature_control			0x3A
#define ia32_bios_updt_trig				0x79
#define ia32_bios_sign_id				0x8B
#define ia32_mtrr_cap					0xFE
#define ia32_sysenter_cs				0x174
#define ia32_sysenter_esp				0x175
#define ia32_sysenter_eip				0x176
#define ia32_debug_control				0x1D9
#define ia32_smrr_phys_base				0x1F2
#define ia32_smrr_phys_mask				0x1F3
#define ia32_mtrr_phys_base0			0x200
#define ia32_mtrr_phys_mask0			0x201
#define ia32_mtrr_phys_base1			0x202
#define ia32_mtrr_phys_mask1			0x203
#define ia32_mtrr_phys_base2			0x204
#define ia32_mtrr_phys_mask2			0x205
#define ia32_mtrr_phys_base3			0x206
#define ia32_mtrr_phys_mask3			0x207
#define ia32_mtrr_phys_base4			0x208
#define ia32_mtrr_phys_mask4			0x209
#define ia32_mtrr_phys_base5			0x20A
#define ia32_mtrr_phys_mask5			0x20B
#define ia32_mtrr_phys_base6			0x20C
#define ia32_mtrr_phys_mask6			0x20D
#define ia32_mtrr_phys_base7			0x20E
#define ia32_mtrr_phys_mask7			0x20F
#define ia32_mtrr_phys_base8			0x210
#define ia32_mtrr_phys_mask8			0x211
#define ia32_mtrr_phys_base9			0x212
#define ia32_mtrr_phys_mask9			0x213
#define ia32_mtrr_fix64k_00000			0x250
#define ia32_mtrr_fix16k_80000			0x258
#define ia32_mtrr_fix16k_a0000			0x259
#define ia32_mtrr_fix4k_c0000			0x268
#define ia32_mtrr_fix4k_c8000			0x269
#define ia32_mtrr_fix4k_d0000			0x26A
#define ia32_mtrr_fix4k_d8000			0x26B
#define ia32_mtrr_fix4k_e0000			0x26C
#define ia32_mtrr_fix4k_e8000			0x26D
#define ia32_mtrr_fix4k_f0000			0x26E
#define ia32_mtrr_fix4k_f8000			0x26F
#define ia32_pat						0x277
#define ia32_mtrr_def_type				0x2FF
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
#define ia32_vmx_3rdproc_ctrl			0x492
#define ia32_efer						0xC0000080
#define ia32_star						0xC0000081
#define ia32_lstar						0xC0000082
#define ia32_fmask						0xC0000084
#define ia32_fs_base					0xC0000100
#define ia32_gs_base					0xC0000101
#define ia32_kernel_gs_base				0xC0000102

// This is used for defining IA-32 architectural interrupt vectors.
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
#define ia32_control_protection			21

// Index of Standard Leaves
#define ia32_cpuid_std_max_num_vstr		0x0
#define ia32_cpuid_std_proc_feature		0x1
#define ia32_cpuid_std_cache_tlb_ft		0x2
#define ia32_cpuid_std_proc_serialn		0x3
#define ia32_cpuid_std_det_cc_param		0x4
#define ia32_cpuid_std_monitor_feat		0x5
#define ia32_cpuid_std_thermal_feat		0x6
#define ia32_cpuid_std_struct_extid		0x7
#define ia32_cpuid_std_di_cache_inf		0x9
#define ia32_cpuid_std_arch_perfmon		0xA
#define ia32_cpuid_std_ex_proc_topo		0xB
#define ia32_cpuid_std_pestate_enum		0xD
#define ia32_cpuid_std_irdt_mon_cap		0xF
#define ia32_cpuid_std_irdt_alloc_e		0x10
#define ia32_cpuid_std_sgx_cap_enum		0x12
#define ia32_cpuid_std_prtrace_enum		0x14
#define ia32_cpuid_std_tsc_ncc_info		0x15
#define ia32_cpuid_std_proc_frq_inf		0x16
#define ia32_cpuid_std_sys_ocv_attr		0x17
#define ia32_cpuid_std_atrans_param		0x18

// Index of Extended Leaves
#define ia32_cpuid_ext_max_num_vstr		0x80000000
#define ia32_cpuid_ext_proc_feature		0x80000001
#define ia32_cpuid_ext_brand_str_p1		0x80000002
#define ia32_cpuid_ext_brand_str_p2		0x80000003
#define ia32_cpuid_ext_brand_str_p3		0x80000004
#define ia32_cpuid_ext_caching_tlbs		0x80000005
#define ia32_cpuid_ext_l23cache_tlb		0x80000006
#define ia32_cpuid_ext_powermgr_ras		0x80000007
#define ia32_cpuid_ext_pcap_prm_eid		0x80000008

// This is used for defining IA-32 architectural cpuid flags.
#define ia32_cpuid_vmx				5
#define ia32_cpuid_vmx_bit			0x20
#define ia32_cpuid_hv_presence		31
#define ia32_cpuid_hv_presence_bit	0x80000000

// Segment Descriptor Types
#define ia32_segment_data_ro				0x0
#define ia32_segment_data_ro_accessed		0x1
#define ia32_segment_data_rw				0x2
#define ia32_segment_data_rw_accessed		0x3
#define ia32_segment_data_ro_xdown			0x4
#define ia32_segment_data_ro_xdown_accessed	0x5
#define ia32_segment_data_rw_xdown			0x6
#define ia32_segment_data_rw_xdown_accessed	0x7

#define ia32_segment_code_xo				0x8
#define ia32_segment_code_xo_accessed		0x9
#define ia32_segment_code_rx				0xA
#define ia32_segment_code_rx_accessed		0xB
#define ia32_segment_code_xo_cform			0xC
#define ia32_segment_code_xo_cform_accessed	0xD
#define ia32_segment_code_rx_cform			0xE
#define ia32_segment_code_rx_cform_accessed	0xF

#define ia32_segment_system_16bit_tss_avail	1
#define ia32_segment_system_ldt				2
#define ia32_segment_system_16bit_tss_busy	3
#define ia32_segment_system_16bit_call_gate	4
#define ia32_segment_system_task_gate		5
#define ia32_segment_system_16bit_int_gate	6
#define ia32_segment_system_16bit_trap_gate	7
#define ia32_segment_system_32bit_tss_avail	9
#define ia32_segment_system_32bit_tss_busy	11
#define ia32_segment_system_32bit_call_gate	12
#define ia32_segment_system_32bit_int_gate	14
#define ia32_segment_system_32bit_trap_gate	15

// This is used for defining IA-32 RFlags bits.
#define ia32_rflags_cf			0
#define ia32_rflags_pf			2
#define ia32_rflags_af			4
#define ia32_rflags_zf			6
#define ia32_rflags_sf			7
#define ia32_rflags_tf			8
#define ia32_rflags_if			9
#define ia32_rflags_df			10
#define ia32_rflags_of			11
#define ia32_rflags_nt			14
#define ia32_rflags_rf			16
#define ia32_rflags_vm			17
#define ia32_rflags_ac			18
#define ia32_rflags_vif			19
#define ia32_rflags_vip			20
#define ia32_rflags_id			21

#define ia32_rflags_iopl(f)	((f&0x3000)>>12)

// This is used for defining IA-32 architectural cache types.
#define ia32_uncacheable		0
#define ia32_write_combining	1
#define ia32_write_through		4
#define ia32_write_protected	5
#define ia32_write_back			6
#define ia32_uncached_minus		7

// This is used for defining IA-32 Extended Control Registers (XCR)
typedef union _ia32_xcr0
{
	struct
	{
		u64 x87:1;			// Bit	0
		u64 sse:1;			// Bit	1
		u64 avx:1;			// Bit	2
		u64 bngreg:1;		// Bit	3
		u64 bndcsr:1;		// Bit	4
		u64 opmask:1;		// Bit	5
		u64 zmm_hi256:1;	// Bit	6
		u64 hi16_zmm:1;		// Bit	7
		u64 reserved0:1;	// Bit	8
		u64 pkru:1;			// Bit	9
		u64 reserved1:54;	// Bits	10-63
	};
	struct
	{
		u32 lo;
		u32 hi;
	};
	u64 value;
}ia32_xcr0,*ia32_xcr0_p;

// IA32 Feature Control MSR
typedef union _ia32_feature_control_msr
{
	struct
	{
		u64 lock:1;						// Bit	0
		u64 vmx_enabled_inside_smx:1;	// Bit	1
		u64 vmx_enabled_outside_smx:1;	// Bit	2
		u64 reserved1:5;				// Bits	3-7
		u64 senter_local_enabled:7;		// Bits	8-14
		u64 senter_global_enabled:1;	// Bit	15
		u64 reserved2:1;				// Bit	16
		u64 sgx_launch_enable:1;		// Bit	17
		u64 sgx_global_enable:1;		// Bit	18
		u64 reserved3:1;				// Bit	19
		u64 lmce:1;						// Bit	20
		u64 reserved4:43;				// Bits	21-63
	};
	u64 value;
}ia32_feature_control_msr,*ia32_feature_control_msr_p;

// IA32 MTRR Capability MSR
typedef union _ia32_mtrr_cap_msr
{
	struct
	{
		u64 variable_count:8;
		u64 support_fixed:1;
		u64 reserved1:1;
		u64 support_wc:1;
		u64 support_smrr:1;
		u64 support_prmrr:1;
		u64 reserved2:51;
	};
	u64 value;
}ia32_mtrr_cap_msr,*ia32_mtrr_cap_msr_p;

// IA32 MTRR Default-Type MSR
typedef union _ia32_mtrr_def_type_msr
{
	struct
	{
		u64 type:8;
		u64 reserved1:2;
		u64 fix_enabled:1;
		u64 enabled:1;
		u64 reserved2:52;
	};
	u64 value;
}ia32_mtrr_def_type_msr,*ia32_mtrr_def_type_msr_p;

typedef union _ia32_mtrr_phys_base_msr
{
	struct
	{
		u64 type:8;
		u64 reserved1:4;
		u64 phys_base:52;
	};
	u64 value;
}ia32_mtrr_phys_base_msr,*ia32_mtrr_phys_base_msr_p;

typedef union _ia32_mtrr_phys_mask_msr
{
	struct
	{
		u64 reserved:11;
		u64 valid:1;
		u64 phys_mask:52;
	};
	u64 value;
}ia32_mtrr_phys_mask_msr,*ia32_mtrr_phys_mask_msr_p;

// IA32 Page Fault Error Code
typedef union _ia32_page_fault_error_code
{
	struct
	{
		u32 present:1;			// Bit	0
		u32 write:1;			// Bit	1
		u32 user:1;				// Bit	2
		u32 reserved:1;			// Bit	3
		u32 execute:1;			// Bit	4
		u32 protection_key:1;	// Bit	5
		u32 shadow_stack:1;		// Bit	6
		u32 reserved0:8;		// Bits	7-14
		u32 sgx:1;				// Bit	15
		u32 reserved1:16;		// Bits	16-31
	};
	u32 value;
}ia32_page_fault_error_code,*ia32_page_fault_error_code_p;