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

pub mod paging
{
	use crate::xpf_core::nvbdk::*;

	pub const PAGING_PRESENT_BIT:u64=0;
	pub const PAGING_WRITE_BIT:u64=1;
	pub const PAGING_USER_BIT:u64=2;
	pub const PAGING_PWT_BIT:u64=3;
	pub const PAGING_PCD_BIT:u64=4;
	pub const PAGING_ACCESSED_BIT:u64=5;
	pub const PAGING_DIRTY_BIT:u64=6;
	pub const PAGING_PAGE_SIZE_BIT:u64=7;
	pub const PAGING_PTE_PAT_BIT:u64=7;
	pub const PAGING_GLOBAL_BIT:u64=8;
	pub const PAGING_AVL_BIT:u64=9;
	pub const PAGING_PAT_BIT:u64=12;
	pub const PAGING_NX_BIT:u64=63;

	pub struct Pml4e(pub u64);
	
	impl Pml4e
	{
		pub fn new(present:bool,write:bool,user:bool,global:bool,no_execute:bool,pdpte_phys:u64)->Self
		{
			let p=(present as u64)<<PAGING_PRESENT_BIT;
			let w=(write as u64)<<PAGING_WRITE_BIT;
			let u=(user as u64)<<PAGING_USER_BIT;
			let g=(global as u64)<<PAGING_GLOBAL_BIT;
			let b=phys_page_4kb_base(pdpte_phys as usize) as u64;
			let nx=(no_execute as u64)<<PAGING_NX_BIT;
			Self(p|w|u|g|b|nx)
		}
	}

	pub struct HugePdpte(pub u64);

	impl HugePdpte
	{
		pub fn new(present:bool,write:bool,user:bool,global:bool,no_execute:bool,base_phys:u64)->Self
		{
			let p=(present as u64)<<PAGING_PRESENT_BIT;
			let w=(write as u64)<<PAGING_WRITE_BIT;
			let u=(user as u64)<<PAGING_USER_BIT;
			let g=(global as u64)<<PAGING_GLOBAL_BIT;
			let ps=1<<PAGING_PAGE_SIZE_BIT;
			let b=phys_page_4kb_base(base_phys as usize) as u64;
			let nx=(no_execute as u64)<<PAGING_NX_BIT;
			Self(p|w|u|g|ps|b|nx)
		}
	}

	// Remaining items are not yet implemented.
}

pub mod descriptors
{
	use core::fmt::{self,Display};
    use crate::xpf_core::hv_host::x86::AsmInterruptHandler;

	// Descriptor Table register forbids any paddings.
	#[repr(C,packed)] pub struct DescriptorTable
	{
		pub limit:u16,
		pub base:u64
	}

	impl Display for DescriptorTable
	{
		fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
		{
			write!(f,"Limit: 0x{:04X}, Base: 0x{:016X}",{self.limit},{self.base})
		}
	}

	#[repr(C,packed)] pub struct UserSegmentDescriptor
	{
		pub limit_lo:u16,
		pub base_lo:u16,
		pub base_mid:u8,
		pub flags:u16,
		pub base_hi:u8
	}

	#[repr(C,packed)] pub struct SystemSegmentDescriptor
	{
		pub limit_lo:u16,
		pub base_lo:u16,
		pub base_mid1:u8,
		pub flags:u16,
		pub base_mid2:u8,
		pub base_hi:u32,
		pub reserved:u32
	}

	pub const GATE_DESCRIPTOR_LDT:u16=0x2;
	pub const GATE_DESCRIPTOR_AVAILABLE_TSS:u16=0x9;
	pub const GATE_DESCRIPTOR_BUSY_TSS:u16=0xB;
	pub const GATE_DESCRIPTOR_CALL_GATE:u16=0xC;
	pub const GATE_DESCRIPTOR_INTERRUPT_GATE:u16=0xE;
	pub const GATE_DESCRIPTOR_TRAP_GATE:u16=0xF;

	pub const GATE_DESCRIPTOR_TYPE_BIT:u16=8;
	pub const GATE_DESCRIPTOR_DPL_BIT:u16=13;
	pub const GATE_DESCRIPTOR_PRESENT_BIT:u16=15;

	#[derive(Default,Clone,Copy)]
	#[repr(C,packed)] pub struct GateDescriptor
	{
		pub offset_lo:u16,
		pub selector:u16,
		pub flags:u16,
		pub offset_mid:u16,
		pub offset_hi:u32,
		pub reserved:u32
	}

	impl GateDescriptor
	{
		pub fn new_intgate(target_handler:AsmInterruptHandler,selector:u16,dpl:u16,ist:u16)->Option<Self>
		{
			if dpl>3
			{
				return None;
			}
			if ist>7
			{
				return None;
			}
			let offset_lo=(target_handler as usize & 0xFFFF) as u16;
			let offset_mid=((target_handler as usize >> 16) & 0xFFFF) as u16;
			let offset_hi=(target_handler as usize >> 32) as u32;
			let flags=ist|(GATE_DESCRIPTOR_INTERRUPT_GATE<<GATE_DESCRIPTOR_TYPE_BIT)|(dpl<<GATE_DESCRIPTOR_DPL_BIT)|(1<<GATE_DESCRIPTOR_PRESENT_BIT);
			Some
			(
				Self
				{
					offset_lo,
					offset_mid,
					selector,
					flags,
					offset_hi,
					reserved:0
				}
			)
		}
	}

	#[derive(Default,Clone,Copy)]
	#[repr(C,packed)] pub struct TaskSegmentState64
	{
		reserved0:u32,
		pub rsp0:u64,
		pub rsp1:u64,
		pub rsp2:u64,
		reserved1:u64,
		pub ist1:u64,
		pub ist2:u64,
		pub ist3:u64,
		pub ist4:u64,
		pub ist5:u64,
		pub ist6:u64,
		pub ist7:u64,
		reserved2:u64,
		reserved3:u16,
		iomap_base:u16
	}
}

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
	use core::fmt::Display;

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
		ReservedEvent=1,
		NonMaskableInterrupt=2,
		HardwareException=3,
		SoftwareInterrupt=4,
		PrivilegedSoftwareException=5,
		SoftwareException=6,
		OtherEvent=7
	}

	#[derive(Clone, Copy)]
	#[repr(C)] pub struct InterruptStackFrame
	{
		pub return_rip:u64,
		pub return_cs:u16,
		pub return_rflags:u64,
		pub return_rsp:u64,
		pub return_ss:u16
	}

	impl Display for InterruptStackFrame
	{
		fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
		{
			writeln!(f,"Return cs:rip={:04X}:{:016X}, ",self.return_cs,self.return_rip)?;
			writeln!(f,"Return ss:rip={:04X}:{:016X}, ",self.return_ss,self.return_rsp)?;
			writeln!(f,"Return rflags=0x{:016X}",self.return_rflags)
		}
	}

	#[derive(Clone, Copy)]
	#[repr(C)] pub struct InterruptStackFrameWithErrorCode
	{
		pub error_code:u32,
		pub return_rip:u64,
		pub return_cs:u16,
		pub return_rflags:u64,
		pub return_rsp:u64,
		pub return_ss:u16
	}

	impl Display for InterruptStackFrameWithErrorCode
	{
		fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
		{
			writeln!(f,"Return cs:rip={:04X}:{:016X}, ",self.return_cs,self.return_rip)?;
			writeln!(f,"Return ss:rip={:04X}:{:016X}, ",self.return_ss,self.return_rsp)?;
			writeln!(f,"Return rflags=0x{:016X}",self.return_rflags)?;
			writeln!(f,"Error-Code=0x{:08X}",self.error_code)
		}
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