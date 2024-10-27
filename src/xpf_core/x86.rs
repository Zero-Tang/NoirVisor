/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file lists universal x86 definitions for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

pub mod cpuid
{
	// Standard Leaf
	pub const CPUID_STD_MAX_NUMBER_VENDOR_STRING:u32=0x0;
	pub const CPUID_STD_PROCESSOR_FEATURE:u32=0x1;
	pub const CPUID_STD_MONITOR_FEATURE:u32=0x5;
	pub const CPUID_STD_THERMAL_FEATURE:u32=0x6;
	pub const CPUID_STD_STRUCTURED_EXTENDED_FEATURE_ID:u32=0x7;
	pub const CPUID_STD_EXTENDED_TOPOLOGY_INFORMATION:u32=0xB;
	pub const CPUID_STD_PROCESOR_EXTENDED_STATE_ENUMERATION:u32=0xD;
	// Extended Leaf
	pub const CPUID_EXT_MAX_NUMBER_VENDOR_STRING:u32=0x80000000;
	pub const CPUID_EXT_PROCESSOR_FEATURE:u32=0x80000001;
	pub const CPUID_EXT_BRAND_STRING_P1:u32=0x80000002;
	pub const CPUID_EXT_BRAND_STRING_P2:u32=0x80000003;
	pub const CPUID_EXT_BRAND_STRING_P3:u32=0x80000004;
	pub const CPUID_EXT_L1_CACHE_TLBS:u32=0x80000005;
	pub const CPUID_EXT_L2_L3_CACHE_TLBS:u32=0x80000006;
	pub const CPUID_EXT_POWER_MANAGEMENT_RAS_CAPABILITY:u32=0x80000007;
	pub const CPUID_EXT_PROCESSOR_CAPABILITY_PARAMETERS_EXTENDED_ID:u32=0x80000008;
	/// Use this flag for CPUID[EAX=0x00000001].ECX
	pub const CPUID_UNDER_HYPERVISOR:u32=0x80000000;
	/// Use this flag for CPUID[EAX=0x80000001].EDX
	pub const CPUID_1GB_PAGE:u32=0x4000000;
}

pub mod rflags
{
	pub const RFLAGS_CF_BIT:u32=0;
	pub const RFLAGS_PF_BIT:u32=2;
	pub const RFLAGS_AF_BIT:u32=4;
	pub const RFLAGS_ZF_BIT:u32=6;
	pub const RFLAGS_SF_BIT:u32=7;
	pub const RFLAGS_TF_BIT:u32=8;
	pub const RFLAGS_IF_BIT:u32=9;
	pub const RFLAGS_DF_BIT:u32=10;
	pub const RFLAGS_OF_BIT:u32=11;
	pub const RFLAGS_NT_BIT:u32=14;
	pub const RFLAGS_RF_BIT:u32=16;
	pub const RFLAGS_VM_BIT:u32=17;
	pub const RFLAGS_AC_BIT:u32=18;
	pub const RFLAGS_VIF_BIT:u32=19;
	pub const RFLAGS_VIP_BIT:u32=20;
	pub const RFLAGS_ID_BIT:u32=21;
}

pub mod interrupts
{
	pub const DIVIDE_ERROR_FAULT:u8=0;
	pub const DEBUG_FAULT_OR_TRAP:u8=1;
	pub const NMI_INTERRUPT:u8=2;
	pub const BREAKPOINT_TRAP:u8=3;
	pub const OVERFLOW_TRAP:u8=4;
	pub const EXCEED_BOUND_RANGE_FAULT:u8=5;
	pub const INVALID_OPCODE_FAULT:u8=6;
	pub const NO_MATH_COPROCESSOR_FAULT:u8=7;
	pub const DOUBLE_FAULT_ABORT:u8=8;
	pub const SEGMENT_OVERRUN_FAULT:u8=9;
	pub const INVALID_TSS_FAULT:u8=10;
	pub const SEGMENT_ABSENT_FAULT:u8=11;
	pub const STACK_FAULT:u8=12;
	pub const GENERAL_PROTECTION_FAULT:u8=13;
	pub const PAGE_FAULT:u8=14;
	pub const X87_FP_EXCEPTION_FAULT:u8=16;
	pub const ALIGNMENT_CHECK_FAULT:u8=17;
	pub const MACHINE_CHECK_ABORT:u8=18;
	pub const SIMD_FP_EXCEPTION_FAULT:u8=19;
	pub const CONTROL_PROTECTION_FAULT:u8=21;

	pub enum EventType
	{
		ExternalInterrupt=0,
		NonMaskableInterrupt=2,
		HardwareException=3,
		SoftwareInterrupt=4
	}
}

pub mod msr
{
	pub const MSR_TSC:u32=0x10;
	pub const MSR_APIC_BASE:u32=0x1B;
	pub const MSR_SPEC_CTRL:u32=0x48;
	pub const MSR_PRED_CMD:u32=0x49;
	pub const MSR_MTRR_CAP:u32=0xFE;
	pub const MSR_SYSENTER_CS:u32=0x174;
	pub const MSR_SYSENTER_ESP:u32=0x175;
	pub const MSR_SYSENTER_EIP:u32=0x176;
	pub const MSR_DEBUG_CONTROL:u32=0x1D9;
	pub const MSR_MTRR_PHYS_BASE0:u32=0x200;
	pub const MSR_MTRR_PHYS_MASK0:u32=0x201;
	pub const MSR_MTRR_PHYS_BASE1:u32=0x202;
	pub const MSR_MTRR_PHYS_MASK1:u32=0x203;
	pub const MSR_MTRR_PHYS_BASE2:u32=0x204;
	pub const MSR_MTRR_PHYS_MASK2:u32=0x205;
	pub const MSR_MTRR_PHYS_BASE3:u32=0x206;
	pub const MSR_MTRR_PHYS_MASK3:u32=0x207;
	pub const MSR_MTRR_PHYS_BASE4:u32=0x208;
	pub const MSR_MTRR_PHYS_MASK4:u32=0x209;
	pub const MSR_MTRR_PHYS_BASE5:u32=0x20A;
	pub const MSR_MTRR_PHYS_MASK5:u32=0x20B;
	pub const MSR_MTRR_PHYS_BASE6:u32=0x20C;
	pub const MSR_MTRR_PHYS_MASK6:u32=0x20D;
	pub const MSR_MTRR_PHYS_BASE7:u32=0x20E;
	pub const MSR_MTRR_PHYS_MASK7:u32=0x20F;
	pub const MSR_MTRR_FIX64K_00000:u32=0x250;
	pub const MSR_MTRR_FIX16K_80000:u32=0x258;
	pub const MSR_MTRR_FIX16K_A0000:u32=0x259;
	pub const MSR_MTRR_FIX4K_C0000:u32=0x268;
	pub const MSR_MTRR_FIX4K_C8000:u32=0x269;
	pub const MSR_MTRR_FIX4K_D0000:u32=0x26A;
	pub const MSR_MTRR_FIX4K_D8000:u32=0x26B;
	pub const MSR_MTRR_FIX4K_E0000:u32=0x26C;
	pub const MSR_MTRR_FIX4K_E8000:u32=0x26D;
	pub const MSR_MTRR_FIX4K_F0000:u32=0x26E;
	pub const MSR_MTRR_FIX4K_F8000:u32=0x26F;
	pub const MSR_PAT:u32=0x277;
	pub const MSR_MTRR_DEF_TYPE:u32=0x2FF;
	pub const MSR_U_CET:u32=0x6A0;
	pub const MSR_S_CET:u32=0x6A2;
	pub const MSR_PL0_SSP:u32=0x6A4;
	pub const MSR_PL1_SSP:u32=0x6A5;
	pub const MSR_PL2_SSP:u32=0x6A6;
	pub const MSR_PL3_SSP:u32=0x6A7;
	pub const MSR_ISST_ADDR:u32=0x6A8;
	pub const MSR_X2APIC_MSR_START:u32=0x800;
	pub const MSR_X2APIC_ID:u32=0x802;
	pub const MSR_X2APIC_VERSION:u32=0x803;
	pub const MSR_X2APIC_TPR:u32=0x808;
	pub const MSR_X2APIC_APR:u32=0x809;
	pub const MSR_X2APIC_PPR:u32=0x80A;
	pub const MSR_X2APIC_EOI:u32=0x80B;
	pub const MSR_X2APIC_LDR:u32=0x80D;
	pub const MSR_X2APIC_SPUR_INT_VECTOR:u32=0x80F;
	pub const MSR_X2APIC_ISR:u32=0x810;
	pub const MSR_X2APIC_TMR:u32=0x818;
	pub const MSR_X2APIC_IRR:u32=0x820;
	pub const MSR_X2APIC_ESR:u32=0x828;
	pub const MSR_X2APIC_ICR:u32=0x830;
	pub const MSR_X2APIC_TIMER_LVT:u32=0x832;
	pub const MSR_X2APIC_THERMAL_LVT:u32=0x833;
	pub const MSR_X2APIC_PERFCNT_LVT:u32=0x834;
	pub const MSR_X2APIC_LINT0_LVT:u32=0x835;
	pub const MSR_X2APIC_LINT1_LVT:u32=0x836;
	pub const MSR_X2APIC_EVT:u32=0x837;
	pub const MSR_X2APIC_TIMER_INIT_COUNT:u32=0x838;
	pub const MSR_X2APIC_TIMER_CUR_COUNT:u32=0x839;
	pub const MSR_X2APIC_TIMER_DIV_CONF:u32=0x83E;
	pub const MSR_X2APIC_SELF_IPI:u32=0x83F;
	pub const MSR_X2APIC_EXT_FEAT:u32=0x840;
	pub const MSR_X2APIC_EXT_CTRL:u32=0x841;
	pub const MSR_X2APIC_SEOI:u32=0x842;
	pub const MSR_X2APIC_IER:u32=0x848;
	pub const MSR_X2APIC_EXTINT_LVT:u32=0x850;
	pub const MSR_X2APIC_MSR_END:u32=0x8FF;
	pub const MSR_XSS:u32=0xDA0;
	pub const MSR_EFER:u32=0xC0000080;
	pub const MSR_STAR:u32=0xC0000081;
	pub const MSR_LSTAR:u32=0xC0000082;
	pub const MSR_CSTAR:u32=0xC0000083;
	pub const MSR_SFMASK:u32=0xC0000084;
	pub const MSR_FS_BASE:u32=0xC0000100;
	pub const MSR_GS_BASE:u32=0xC0000101;
	pub const MSR_KERNEL_GS_BASE:u32=0xC0000102;
}