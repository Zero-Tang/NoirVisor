/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file manages VMCB in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{arch::asm, ffi::c_void, ops::{BitAndAssign, BitOrAssign, BitXorAssign}};
use paste::paste;
use bitfield_struct::bitfield;

use crate::{xpf_core::x86::crdr::DR6_BS_BIT, *};
use xpf_core::{x86::{interrupts::*, rflags::*}, nvbdk::SegmentRegister};

#[inline] pub fn svm_attrib(attrib:u16)->u16
{
	((attrib&0xFF)|((attrib&0xF000)>>4))&0xfff
}

#[inline] pub fn svm_attrib_inverse(attrib:u16)->u16
{
	((attrib&0xF00)<<4)|(attrib&0xFF)
}

#[inline] pub fn svm_msrpm_bit(index:u32,operation:bool)->Option<u32>
{
	let base=match index
	{
		0..0x2000=>Some(index<<1),
		0xC0000000..0xC0002000=>Some(((index-0xC0000000)<<1)+0x4000),
		0xC0010000..0xC0012000=>Some(((index-0xC0010000)<<1)+0x8000),
		_=>None
	};
	base.map(|x| x+if operation {1} else {0})
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn inject_event(vmcb:*mut c_void,vector:u8,event_type:EventType,error_code:Option<u32>,valid:bool)
{
	let evt=match error_code
	{
		Some(ec)=>EventInjection::construct(vector,event_type,true,valid,ec),
		None=>EventInjection::construct(vector,event_type,false,valid,0)
	};
	unsafe
	{
		vmwrite(vmcb,EVENT_INJECTION,evt.0);
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn advance_rip(vmcb:*mut c_void)
{
	unsafe
	{
		vmwrite(vmcb,GUEST_RIP,vmread::<u64>(vmcb,NEXT_RIP));
		if vmcb_bt32(vmcb,GUEST_RFLAGS,RFLAGS_TF_BIT)
		{
			// In case the guest is single-step debugging, we should inject debug trace trap so that
			// the next instruction won't be skipped in debugger, confusing the debugging personnel.
			// If guest is single-step debugging, slight penalty due to branch predictor is acceptable.
			inject_event(vmcb,DEBUG_FAULT_OR_TRAP,EventType::HardwareException,None,true);
			vmcb_bts32(vmcb,GUEST_DR6,DR6_BS_BIT as u32);
			vmcb_clean_dr(vmcb);
		}
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn advance_rip_manually(vmcb:*mut c_void,len:usize)
{
	unsafe
	{
		vmwrite(vmcb,NEXT_RIP,vmread::<u64>(vmcb,GUEST_RIP)+(len as u64));
		advance_rip(vmcb)
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmread_segment(vmcb:*mut c_void,offset:usize)->SegmentRegister
{
	unsafe
	{
		SegmentRegister
		{
			selector:vmread(vmcb,offset),
			attrib:svm_attrib_inverse(vmread(vmcb,offset+2)),
			limit:vmread(vmcb,offset+4),
			base:vmread(vmcb,offset+8)
		}
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmwrite_segment(vmcb:*mut c_void,offset:usize,value:SegmentRegister)
{
	unsafe
	{
		vmwrite(vmcb,offset,value.selector);
		vmwrite(vmcb,offset+2,svm_attrib(value.attrib));
		vmwrite(vmcb,offset+4,value.limit);
		vmwrite(vmcb,offset+8,value.base);
	}
}

// Let's abuse generics here!

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmread<T>(vmcb:*mut c_void,offset:usize)->T
{
	unsafe
	{
		vmcb.byte_add(offset).cast::<T>().read()
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmwrite<T>(vmcb:*mut c_void,offset:usize,value:T)
{
	unsafe
	{
		vmcb.byte_add(offset).cast::<T>().write(value)
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcopy<T>(dest:*mut c_void,src:*mut c_void,offset:usize)
{
	unsafe
	{
		vmwrite(dest,offset,vmread::<T>(src,offset))
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcb_and<T:BitAndAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	unsafe
	{
		*vmcb.byte_add(offset).cast::<T>()&=value
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcb_or<T:BitOrAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	unsafe
	{
		*vmcb.byte_add(offset).cast::<T>()|=value
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcb_xor<T:BitXorAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	unsafe
	{
		*vmcb.byte_add(offset).cast::<T>()^=value
	}
}

// It seems impossible to use generics to build bit-test operations on VMCB.
// So let's abuse macros to make generate operations on VMCB.
macro_rules! build_vmcb_bt
{
	($ins:tt,$size:tt,$bits:tt) =>
	{
		paste!
		{
			/// # Safety
			/// The `vmcb` argument is not guaranteed to be valid.
			#[inline] pub unsafe fn [<vmcb_ $ins $bits>](vmcb:*mut c_void,offset:usize,pos:u32)->bool
			{
				let flag:u8;
				unsafe
				{
					let p=vmcb.byte_add(offset);
					asm!
					(
						concat!(stringify!($ins)," ",$size," ptr [{p}],{b:e}"),
						"setc {f}",
						p=in(reg) p,
						b=in(reg) pos,
						f=out(reg_byte) flag
					);
				}
				flag!=0
			}
		}
	};
}

build_vmcb_bt!(bt,"dword",32);
build_vmcb_bt!(bts,"dword",32);
build_vmcb_bt!(btr,"dword",32);
build_vmcb_bt!(btc,"dword",32);
build_vmcb_bt!(bt,"qword",64);
build_vmcb_bt!(bts,"qword",64);
build_vmcb_bt!(btr,"qword",64);
build_vmcb_bt!(btc,"qword",64);

// Using offsets is much easier than defining a structure.
// This is also how NoirVisor in C operates the VMCB.
// Control-Area
pub const INTERCEPT_READ_CR:usize=0x0;
pub const INTERCEPT_WRITE_CR:usize=0x2;
pub const INTERCEPT_ACCESS_CR:usize=0x0;
pub const INTERCEPT_READ_DR:usize=0x4;
pub const INTERCEPT_WRITE_DR:usize=0x6;
pub const INTERCEPT_ACCESS_DR:usize=0x4;
pub const INTERCEPT_EXCEPTIONS:usize=0x8;
pub const INTERCEPT_VECTOR1:usize=0xC;
pub const INTERCEPT_VECTOR2:usize=0x10;
pub const INTERCEPT_WRITE_CR_POST:usize=0x12;
pub const INTERCEPT_VECTOR3:usize=0x14;
pub const PAUSE_FILTER_THRESHOLD:usize=0x3C;
pub const PAUSE_FILTER_COUNT:usize=0x3E;
pub const IOPM_PHYSICAL_ADDRESS:usize=0x40;
pub const MSRPM_PHYSICAL_ADDRESS:usize=0x48;
pub const TSC_OFFSET:usize=0x50;
pub const GUEST_ASID:usize=0x58;
pub const TLB_CONTROL:usize=0x5C;
pub const AVIC_CONTROL:usize=0x60;
pub const AVIC_VIRQ_VECTOR:usize=0x64;
pub const GUEST_INTERRUPT:usize=0x68;
pub const EXIT_CODE:usize=0x70;
pub const EXIT_INFO1:usize=0x78;
pub const EXIT_INFO2:usize=0x80;
pub const EXIT_INTERRUPT_INFO:usize=0x88;
pub const NPT_CONTROL:usize=0x90;
pub const AVIC_APIC_BAR:usize=0x98;
pub const GHCB_PHYSICAL_ADDRESS:usize=0xA0;
pub const EVENT_INJECTION:usize=0xA8;
pub const EVENT_ERROR_CODE:usize=0xAC;
pub const NPT_CR3:usize=0xB0;
pub const LBR_VIRTUALIZATION_CONTROL:usize=0xB8;
pub const VMCB_CLEAN_BITS:usize=0xC0;
pub const NEXT_RIP:usize=0xC8;
pub const NUMBER_OF_BYTES_FETCHED:usize=0xD0;
pub const GUEST_INSTRUCTION_BYTES:usize=0xD1;
pub const AVIC_BACKING_PAGE_POINTER:usize=0xE0;
pub const AVIC_LOGICAL_TABLE_POINTER:usize=0xF0;
pub const AVIC_PHYSICAL_TABLE_POINTER:usize=0xF8;
pub const VMSA_POINTER:usize=0x108;
pub const VMGEXIT_RAX:usize=0x110;
pub const VMGEXIT_CPL:usize=0x118;
pub const BUSLOCK_THRESHOLD_COUNTER:usize=0x120;
pub const UPDATE_IRR:usize=0x134;
pub const ALLOWED_SEV_FEATURES:usize=0x138;
pub const GUEST_SEV_FEATURES:usize=0x140;
pub const REQUESTED_IRR:usize=0x150;
// Following offset definitions would be available only if Microsoft Enlightenments are enabled.
pub const ENLIGHTENMENTS_CONTROL:usize=0x3E0;
pub const VP_ID:usize=0x3E4;
pub const VM_ID:usize=0x3E8;
pub const PARTITION_ASSIST_PAGE:usize=0x3F0;
// Following offset definitions are unusable if SEV-ES is enabled.
pub const GUEST_ES_SELECTOR:usize=0x400;
pub const GUEST_ES_ATTRIB:usize=0x402;
pub const GUEST_ES_LIMIT:usize=0x404;
pub const GUEST_ES_BASE:usize=0x408;
pub const GUEST_CS_SELECTOR:usize=0x410;
pub const GUEST_CS_ATTRIB:usize=0x412;
pub const GUEST_CS_LIMIT:usize=0x414;
pub const GUEST_CS_BASE:usize=0x418;
pub const GUEST_SS_SELECTOR:usize=0x420;
pub const GUEST_SS_ATTRIB:usize=0x422;
pub const GUEST_SS_LIMIT:usize=0x424;
pub const GUEST_SS_BASE:usize=0x428;
pub const GUEST_DS_SELECTOR:usize=0x430;
pub const GUEST_DS_ATTRIB:usize=0x432;
pub const GUEST_DS_LIMIT:usize=0x434;
pub const GUEST_DS_BASE:usize=0x438;
pub const GUEST_FS_SELECTOR:usize=0x440;
pub const GUEST_FS_ATTRIB:usize=0x442;
pub const GUEST_FS_LIMIT:usize=0x444;
pub const GUEST_FS_BASE:usize=0x448;
pub const GUEST_GS_SELECTOR:usize=0x450;
pub const GUEST_GS_ATTRIB:usize=0x452;
pub const GUEST_GS_LIMIT:usize=0x454;
pub const GUEST_GS_BASE:usize=0x458;
pub const GUEST_GDTR_SELECTOR:usize=0x460;
pub const GUEST_GDTR_ATTRIB:usize=0x462;
pub const GUEST_GDTR_LIMIT:usize=0x464;
pub const GUEST_GDTR_BASE:usize=0x468;
pub const GUEST_LDTR_SELECTOR:usize=0x470;
pub const GUEST_LDTR_ATTRIB:usize=0x472;
pub const GUEST_LDTR_LIMIT:usize=0x474;
pub const GUEST_LDTR_BASE:usize=0x478;
pub const GUEST_IDTR_SELECTOR:usize=0x480;
pub const GUEST_IDTR_ATTRIB:usize=0x482;
pub const GUEST_IDTR_LIMIT:usize=0x484;
pub const GUEST_IDTR_BASE:usize=0x488;
pub const GUEST_TR_SELECTOR:usize=0x490;
pub const GUEST_TR_ATTRIB:usize=0x492;
pub const GUEST_TR_LIMIT:usize=0x494;
pub const GUEST_TR_BASE:usize=0x498;
pub const GUEST_CPL:usize=0x4CB;
pub const GUEST_EFER:usize=0x4D0;
pub const GUEST_PERF_CTL0:usize=0x4E0;
pub const GUEST_PERF_CTR0:usize=0x4E8;
pub const GUEST_PERF_CTL1:usize=0x4F0;
pub const GUEST_PERF_CTR1:usize=0x4F8;
pub const GUEST_PERF_CTL2:usize=0x500;
pub const GUEST_PERF_CTR2:usize=0x508;
pub const GUEST_PERF_CTL3:usize=0x510;
pub const GUEST_PERF_CTR3:usize=0x518;
pub const GUEST_PERF_CTL4:usize=0x520;
pub const GUEST_PERF_CTR4:usize=0x528;
pub const GUEST_PERF_CTL5:usize=0x530;
pub const GUEST_PERF_CTR5:usize=0x538;
pub const GUEST_CR4:usize=0x548;
pub const GUEST_CR3:usize=0x550;
pub const GUEST_CR0:usize=0x558;
pub const GUEST_DR7:usize=0x560;
pub const GUEST_DR6:usize=0x568;
pub const GUEST_RFLAGS:usize=0x570;
pub const GUEST_RIP:usize=0x578;
pub const INSTRUCTION_RETIRED_COUNTER:usize=0x5C0;
pub const PERF_CTR_GLOBAL_STS:usize=0x5C8;
pub const PERF_CTR_GLOBAL_CTR:usize=0x5D0;
pub const GUEST_RSP:usize=0x5D8;
pub const GUEST_S_CET:usize=0x5E0;
pub const GUEST_SSP:usize=0x5E8;
pub const GUEST_ISST:usize=0x5F0;
pub const GUEST_RAX:usize=0x5F8;
pub const GUEST_STAR:usize=0x600;
pub const GUEST_LSTAR:usize=0x608;
pub const GUEST_CSTAR:usize=0x610;
pub const GUEST_SFMASK:usize=0x618;
pub const GUEST_KERNEL_GS_BASE:usize=0x620;
pub const GUEST_SYSENTER_CS:usize=0x628;
pub const GUEST_SYSENTER_ESP:usize=0x630;
pub const GUEST_SYSENTER_EIP:usize=0x638;
pub const GUEST_CR2:usize=0x640;
pub const GUEST_PAT:usize=0x668;
pub const GUEST_DEBUG_CTRL:usize=0x670;
pub const GUEST_LAST_BRANCH_FROM:usize=0x678;
pub const GUEST_LAST_BRANCH_TO:usize=0x680;
pub const GUEST_LAST_EXCEPTION_FROM:usize=0x688;
pub const GUEST_LAST_EXCEPTION_TO:usize=0x690;
pub const GUEST_DEBUG_EXTENED_CONTROL:usize=0x698;
pub const GUEST_SPEC_CTRL:usize=0x6E0;
pub const GUEST_LBR_STACK_FROM:usize=0xA70;
pub const GUEST_LBR_STACK_TO:usize=0xAF0;
pub const GUEST_LBR_SELECT:usize=0xB70;
pub const GUEST_IBS_FETCH_CTRL:usize=0xB78;
pub const GUEST_IBS_FETCH_LINEAR_ADDRESS:usize=0xB80;
pub const GUEST_IBS_OP_CTRL:usize=0xB88;
pub const GUEST_IBS_OP_RIP:usize=0xB90;
pub const GUEST_IBS_OP_DATA1:usize=0xB98;
pub const GUEST_IBS_OP_DATA2:usize=0xBA0;
pub const GUEST_IBS_OP_DATA3:usize=0xBA8;
pub const GUEST_IBS_DC_LINEAR_ADDRESS:usize=0xBB0;
pub const GUEST_BP_IBSTGT_RIP:usize=0xBB8;
pub const GUEST_IC_IBS_EXTD_CTRL:usize=0xBC0;

macro_rules! derive_rw_method
{
	($offset:expr) =>
	{
		#[inline] pub unsafe fn read(vmcb:*mut c_void)->Self
		{
			unsafe
			{
				Self(vmread(vmcb,$offset))
			}
		}

		#[inline] pub unsafe fn write(&self,vmcb:*mut c_void)
		{
			unsafe
			{
				vmwrite(vmcb,$offset,self.0);
			}
		}
	};
}

// Vector 1 of Control Area
#[bitfield(u32)] pub struct InterceptVector1
{
	pub phys_intr:bool,
	pub nmi:bool,
	pub smi:bool,
	pub init:bool,
	pub virt_intr:bool,
	pub cr0_non_ts_mp:bool,
	pub sidt:bool,
	pub sgdt:bool,
	pub sldt:bool,
	pub str:bool,
	pub lidt:bool,
	pub lgdt:bool,
	pub lldt:bool,
	pub ltr:bool,
	pub rdtsc:bool,
	pub rdpmc:bool,
	pub pushf:bool,
	pub popf:bool,
	pub cpuid:bool,
	pub rsm:bool,
	pub iret:bool,
	pub int:bool,
	pub invd:bool,
	pub pause:bool,
	pub hlt:bool,
	pub invlpg:bool,
	pub invlpga:bool,
	pub io:bool,
	pub msr:bool,
	pub task_switch:bool,
	pub ferr_freeze:bool,
	pub shutdown:bool
}

impl InterceptVector1
{
	derive_rw_method!(INTERCEPT_VECTOR1);
}

// Vector 2 of Control Area
#[bitfield(u32)] pub struct InterceptVector2
{
	pub vmrun:bool,
	pub vmmcall:bool,
	pub vmload:bool,
	pub vmsave:bool,
	pub stgi:bool,
	pub clgi:bool,
	pub skinit:bool,
	pub rdtscp:bool,
	pub icebp:bool,
	pub wbinvd:bool,
	pub monitor:bool,
	pub mwait:bool,
	pub mwait_armed:bool,
	pub xsetbv:bool,
	pub rdpru:bool,
	pub post_w_efer:bool,
	pub post_w_cr0:bool,
	pub post_w_cr1:bool,
	pub post_w_cr2:bool,
	pub post_w_cr3:bool,
	pub post_w_cr4:bool,
	pub post_w_cr5:bool,
	pub post_w_cr6:bool,
	pub post_w_cr7:bool,
	pub post_w_cr8:bool,
	pub post_w_cr9:bool,
	pub post_w_cr10:bool,
	pub post_w_cr11:bool,
	pub post_w_cr12:bool,
	pub post_w_cr13:bool,
	pub post_w_cr14:bool,
	pub post_w_cr15:bool
}

impl InterceptVector2
{
	derive_rw_method!(INTERCEPT_VECTOR2);
}

// Vector 3 of Control Area
#[bitfield(u32)] pub struct InterceptVector3
{
	pub invlpgb:bool,
	pub illegal_invlpgb:bool,
	pub invpcid:bool,
	pub mcommit:bool,
	pub tlbsync:bool,
	pub buslock:bool,
	pub idle_hlt:bool,
	#[bits(25)] pub reserved:u32
}

impl InterceptVector3
{
	derive_rw_method!(INTERCEPT_VECTOR3);
}

// TLB Control
pub const TLB_CONTROL_DO_NOTHING:u8=0;
pub const TLB_CONTROL_FLUSH_ENTIRE_TLB:u8=1;
pub const TLB_CONTROL_FLUSH_GUEST_TLB:u8=3;
pub const TLB_CONTROL_FLUSH_GUEST_NON_GLOBAL_TLB:u8=7;

// Offset 0x060: AVIC Control
#[bitfield(u64)] pub struct AvicControl
{
	pub v_tpr:u8,
	pub v_irq:bool,
	pub v_gif:bool,
	rsvd0:bool,
	pub v_nmi:bool,
	pub v_nmi_mask:bool,
	#[bits(3)] rsvd1:u64,
	#[bits(4)] pub v_intr_prio:u64,
	pub v_ignore_tpr:bool,
	#[bits(3)] rsvd2:u64,
	pub v_intr_mask:bool,
	pub v_gif_enabled:bool,
	pub v_nmi_enabled:bool,
	#[bits(3)] rsvd3:u64,
	pub x2avic_enabled:bool,
	pub avic_enabled:bool,
	pub v_intr_vector:u8,
	#[bits(24)] rsvd4:u64
}

impl AvicControl
{
	derive_rw_method!(AVIC_CONTROL);
}

// Offset 0x068: Interrupt Control
#[bitfield(u64)] pub struct InterruptControl
{
	pub interrupt_shadow:bool,
	pub sev_es_rflags_if:bool,
	#[bits(62)] rsvd:u64
}

impl InterruptControl
{
	derive_rw_method!(GUEST_INTERRUPT);
}

// Offset 0x090: Nested Paing
#[bitfield(u64)] pub struct NptControl
{
	pub enable_npt:bool,
	pub enable_sev:bool,
	pub enable_sev_es:bool,
	pub enable_gmet:bool,
	pub enable_sss_check:bool,
	pub enable_vte:bool,
	pub enable_rogpt:bool,
	pub enable_invlpgb:bool,
	#[bits(56)] rsvd:u64
}

impl NptControl
{
	derive_rw_method!(NPT_CONTROL);
}

// Offset 0x0A8: Event Injection
#[bitfield(u64)] pub struct EventInjection
{
	pub vector:u8,
	#[bits(3)] pub event_type:u8,
	pub error_code_valid:bool,
	#[bits(19)] rsvd0:u64,
	pub valid:bool,
	pub error_code:u32
}

impl EventInjection
{
	#[inline] pub fn construct(vector:u8,event_type:EventType,has_error_code:bool,valid:bool,error_code:u32)->Self
	{
		let mut v=Self(0);
		v.set_vector(vector);
		v.set_event_type(event_type as u8);
		v.set_error_code_valid(has_error_code);
		v.set_valid(valid);
		v.set_error_code(error_code);
		v
	}
}

// Offset 0x0B8: LBR Virtualization
#[bitfield(u64)] pub struct LbrVirtualization
{
	pub lbr_virt:bool,
	pub vmls_virt:bool,
	pub ibs_virt:bool,
	pub pmc_virt:bool,
	#[bits(60)] rsvd:u64
}

impl LbrVirtualization
{
	derive_rw_method!(LBR_VIRTUALIZATION_CONTROL);
}

// Offset 0x0C0: VMCB Clean Bits
pub const CLEAN_INTERCEPTION_BIT:u32=0;
pub const CLEAN_IOMSRPM_BIT:u32=1;
pub const CLEAN_ASID_BIT:u32=2;
pub const CLEAN_TPR_BIT:u32=3;
pub const CLEAN_NPT_BIT:u32=4;
pub const CLEAN_CR_BIT:u32=5;
pub const CLEAN_DR_BIT:u32=6;
pub const CLEAN_DT_BIT:u32=7;
pub const CLEAN_SEG_BIT:u32=8;
pub const CLEAN_CR2_BIT:u32=9;
pub const CLEAN_LBR_BIT:u32=10;
pub const CLEAN_AVIC_BIT:u32=11;
pub const CLEAN_CET_BIT:u32=12;

macro_rules! build_clean_bit_fn
{
	($name:tt) =>
	{
		paste!
		{
			#[inline] pub unsafe fn [<vmcb_clean_ $name:lower>](vmcb:*mut c_void)
			{
				unsafe
				{
					vmcb_btr32(vmcb,VMCB_CLEAN_BITS,[<CLEAN_ $name:upper _BIT>]);
				}
			}
		}
	};
}

build_clean_bit_fn!(interception);
build_clean_bit_fn!(iomsrpm);
build_clean_bit_fn!(asid);
build_clean_bit_fn!(tpr);
build_clean_bit_fn!(npt);
build_clean_bit_fn!(cr);
build_clean_bit_fn!(dr);
build_clean_bit_fn!(dt);
build_clean_bit_fn!(seg);
build_clean_bit_fn!(cr2);
build_clean_bit_fn!(lbr);
build_clean_bit_fn!(avic);
build_clean_bit_fn!(cet);

// Following definitions is for State Save Area with SEV-ES Enabled
// You may notice the offset is 0x400 different from corresponding fields.
pub const GUEST_SEV_ES_SEGMENT:usize=0x0;
pub const GUEST_SEV_CS_SEGMENT:usize=0x10;
pub const GUEST_SEV_SS_SEGMENT:usize=0x20;
pub const GUEST_SEV_DS_SEGMENT:usize=0x30;
pub const GUEST_SEV_FS_SEGMENT:usize=0x40;
pub const GUEST_SEV_GS_SEGMENT:usize=0x50;
pub const GUEST_SEV_GDTR_SEGMENT:usize=0x60;
pub const GUEST_SEV_LDTR_SEGMENT:usize=0x70;
pub const GUEST_SEV_IDTR_SEGMENT:usize=0x80;
pub const GUEST_SEV_TR_SEGMENT:usize=0x90;
pub const GUEST_SEV_CPL:usize=0xCB;
pub const GUEST_SEV_EFER:usize=0xCC;
pub const GUEST_SEV_CR4:usize=0x148;
pub const GUEST_SEV_CR3:usize=0x150;
pub const GUEST_SEV_CR0:usize=0x158;
pub const GUEST_SEV_DR7:usize=0x160;
pub const GUEST_SEV_DR6:usize=0x168;
pub const GUEST_SEV_RFLAGS:usize=0x170;
pub const GUEST_SEV_RIP:usize=0x178;
pub const GUEST_SEV_DR0:usize=0x180;
pub const GUEST_SEV_DR1:usize=0x188;
pub const GUEST_SEV_DR2:usize=0x190;
pub const GUEST_SEV_DR3:usize=0x198;
pub const GUEST_SEV_DR0_MASK:usize=0x1A0;
pub const GUEST_SEV_DR1_MASK:usize=0x1A8;
pub const GUEST_SEV_DR2_MASK:usize=0x1B0;
pub const GUEST_SEV_DR3_MASK:usize=0x1B8;
pub const GUEST_SEV_RSP:usize=0x1D8;
pub const GUEST_SEV_RAX:usize=0x1F8;
pub const GUEST_SEV_STAR:usize=0x200;
pub const GUEST_SEV_LSTAR:usize=0x208;
pub const GUEST_SEV_CSTAR:usize=0x210;
pub const GUEST_SEV_SFMASK:usize=0x218;
pub const GUEST_SEV_KERNEL_GS_BASE:usize=0x220;
pub const GUEST_SEV_SYSENTER_CS:usize=0x228;
pub const GUEST_SEV_SYSENTER_ESP:usize=0x230;
pub const GUEST_SEV_SYSENTER_EIP:usize=0x238;
pub const GUEST_SEV_CR2:usize=0x240;
pub const GUEST_SEV_PAT:usize=0x268;
pub const GUEST_SEV_DEBUG_CTRL:usize=0x270;
pub const GUEST_SEV_LAST_BRANCH_FROM:usize=0x278;
pub const GUEST_SEV_LAST_BRANCH_TO:usize=0x280;
pub const GUEST_SEV_LAST_EXCEPTION_FROM:usize=0x288;
pub const GUEST_SEV_LAST_EXCEPTION_TO:usize=0x290;
pub const GUEST_SEV_PKRU:usize=0x2E8;
pub const GUEST_SEV_TSC_AUX:usize=0x2EC;
pub const GUEST_SEV_TSC_SCALE:usize=0x2F0;
pub const GUEST_SEV_TSC_OFFSET:usize=0x2F8;
pub const GUEST_SEV_REG_PROT_NONCE:usize=0x300;
pub const GUEST_SEV_RCX:usize=0x308;
pub const GUEST_SEV_RDX:usize=0x310;
pub const GUEST_SEV_RBX:usize=0x318;
pub const GUEST_SEV_RBP:usize=0x328;
pub const GUEST_SEV_RSI:usize=0x330;
pub const GUEST_SEV_RDI:usize=0x338;
pub const GUEST_SEV_R8:usize=0x340;
pub const GUEST_SEV_R9:usize=0x348;
pub const GUEST_SEV_R10:usize=0x350;
pub const GUEST_SEV_R11:usize=0x358;
pub const GUEST_SEV_R12:usize=0x360;
pub const GUEST_SEV_R13:usize=0x368;
pub const GUEST_SEV_R14:usize=0x370;
pub const GUEST_SEV_R15:usize=0x378;
pub const SW_EXIT_INFO1:usize=0x390;
pub const SW_EXIT_INFO2:usize=0x398;
pub const SW_EXIT_INT_INFO:usize=0x3A0;
pub const SW_NEXT_RIP:usize=0x3A8;
pub const SEV_FEATURES:usize=0x3B0;
pub const SEV_VINTR_CTRL:usize=0x3B8;
pub const SEV_GUEST_EXITCODE:usize=0x3C0;
pub const SEV_VIRTUAL_TOM:usize=0x3C8;
pub const SEV_TLB_ID:usize=0x3D0;
pub const SEV_PCPU_ID:usize=0x3D8;
pub const SEV_EVENT_INJECTION:usize=0x3E0;
pub const GUEST_SEV_XCR0:usize=0x3E8;
pub const GUEST_SEV_X87DP:usize=0x400;
pub const GUEST_SEV_MXCSR:usize=0x408;
pub const GUEST_SEV_X87FTW:usize=0x40C;
pub const GUEST_SEV_X87FSW:usize=0x40E;
pub const GUEST_SEV_X87FCW:usize=0x410;
pub const GUEST_SEV_X87FOP:usize=0x412;
pub const GUEST_SEV_X87DS:usize=0x414;
pub const GUEST_SEV_X87CS:usize=0x416;
pub const GUEST_SEV_X87RIP:usize=0x418;
pub const GUEST_SEV_FPREG_X87:usize=0x420;
pub const GUEST_SEV_FPREG_XMM:usize=0x470;
pub const GUEST_SEV_FPREG_YMM:usize=0x570;