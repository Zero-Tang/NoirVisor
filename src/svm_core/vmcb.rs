/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file manages VMCB in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{arch::asm, ffi::c_void, ops::{BitAndAssign, BitOrAssign, BitXorAssign}};

use paste::paste;

use super::{xpf_core::x86::{interrupts::*, rflags::*}, SegmentRegister};

#[inline] pub fn svm_attrib(attrib:u16)->u16
{
	((attrib&0xFF)|((attrib&0xF000)>>4))&0xfff
}

#[inline] pub fn svm_attrib_inverse(attrib:u16)->u16
{
	((attrib&0xF00)<<4)|(attrib&0xFF)
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn inject_event(vmcb:*mut c_void,vector:u8,event_type:EventType,error_code:Option<u32>,valid:bool)
{
	let evt=match error_code
	{
		Some(ec)=>EventInjection::new(vector,event_type,true,valid,ec),
		None=>EventInjection::new(vector,event_type,false,valid,0)
	};
	vmwrite(vmcb,EVENT_INJECTION,evt.0);
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn advance_rip(vmcb:*mut c_void)
{
	vmwrite(vmcb,GUEST_RIP,vmread::<u64>(vmcb,NEXT_RIP));
	// FIXME: Inject #DB if single-stepping.
	if vmcb_bt32(vmcb,GUEST_RFLAGS,RFLAGS_TF_BIT)
	{
		// In case the guest is single-step debugging, we should inject debug trace trap so that
		// the next instruction won't be skipped in debugger, confusing the debugging personnel.
		// If guest is single-step debugging, slight penalty due to branch predictor is acceptable.
		inject_event(vmcb,DEBUG_FAULT_OR_TRAP,EventType::HardwareException,None,true);
	}
	// FIXME: Check Debug Registers. If rip matches one of DR0-DR3, inject #DB exception.
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmread_segment(vmcb:*mut c_void,offset:usize)->SegmentRegister
{
	SegmentRegister
	{
		selector:vmread(vmcb,offset),
		attrib:svm_attrib_inverse(vmread(vmcb,offset+2)),
		limit:vmread(vmcb,offset+4),
		base:vmread(vmcb,offset+8)
	}
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmwrite_segment(vmcb:*mut c_void,offset:usize,value:SegmentRegister)
{
	vmwrite(vmcb,offset,value.selector);
	vmwrite(vmcb,offset+2,svm_attrib(value.attrib));
	vmwrite(vmcb,offset+4,value.limit);
	vmwrite(vmcb,offset+8,value.base);
}

// Let's abuse generics here!

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmread<T>(vmcb:*mut c_void,offset:usize)->T
{
	vmcb.byte_add(offset).cast::<T>().read()
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmwrite<T>(vmcb:*mut c_void,offset:usize,value:T)
{
	vmcb.byte_add(offset).cast::<T>().write(value)
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcopy<T>(dest:*mut c_void,src:*mut c_void,offset:usize)
{
	vmwrite(dest,offset,vmread::<T>(src,offset))
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcb_and<T:BitAndAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	*vmcb.byte_add(offset).cast::<T>()&=value
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcb_or<T:BitOrAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	*vmcb.byte_add(offset).cast::<T>()|=value
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcb_xor<T:BitXorAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	*vmcb.byte_add(offset).cast::<T>()^=value
}

/// # Safety
/// The `vmcb` argument is not guaranteed to be valid.
#[inline] pub unsafe fn vmcb_bt32(vmcb:*mut c_void,offset:usize,pos:u32)->bool
{
	let p=vmcb.byte_add(offset) as usize;
	let flag:u8;
	asm!
	(
		"bt dword ptr [{ptr}],{pos:e}",
		"setc al",
		ptr=in(reg) p,
		pos=in(reg) pos,
		out("al") flag
	);
	flag!=0
}

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
pub const GUEST_CR4:usize=0x548;
pub const GUEST_CR3:usize=0x550;
pub const GUEST_CR0:usize=0x558;
pub const GUEST_DR7:usize=0x560;
pub const GUEST_DR6:usize=0x568;
pub const GUEST_RFLAGS:usize=0x570;
pub const GUEST_RIP:usize=0x578;
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
pub const GUEST_DEBUG_EXTENED_CONFIG:usize=0x698;
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

// Vector 1 of Control Area
pub const INTERCEPT_VECTOR1_INTR:u32=0x00000001;
pub const INTERCEPT_VECTOR1_NMI:u32=0x00000002;
pub const INTERCEPT_VECTOR1_SMI:u32=0x00000004;
pub const INTERCEPT_VECTOR1_INIT:u32=0x00000008;
pub const INTERCEPT_VECTOR1_VINT:u32=0x00000010;
pub const INTERCEPT_VECTOR1_CR0_TSMP:u32=0x00000020;
pub const INTERCEPT_VECTOR1_SIDT:u32=0x00000040;
pub const INTERCEPT_VECTOR1_SGDT:u32=0x00000080;
pub const INTERCEPT_VECTOR1_SLDT:u32=0x00000100;
pub const INTERCEPT_VECTOR1_STR:u32=0x00000200;
pub const INTERCEPT_VECTOR1_LIDT:u32=0x00000400;
pub const INTERCEPT_VECTOR1_LGDT:u32=0x00000800;
pub const INTERCEPT_VECTOR1_LLDT:u32=0x00001000;
pub const INTERCEPT_VECTOR1_LTR:u32=0x00002000;
pub const INTERCEPT_VECTOR1_RDTSC:u32=0x00004000;
pub const INTERCEPT_VECTOR1_RDPMC:u32=0x00008000;
pub const INTERCEPT_VECTOR1_PUSHF:u32=0x00010000;
pub const INTERCEPT_VECTOR1_POPF:u32=0x00020000;
pub const INTERCEPT_VECTOR1_CPUID:u32=0x00040000;
pub const INTERCEPT_VECTOR1_RSM:u32=0x00080000;
pub const INTERCEPT_VECTOR1_IRET:u32=0x00100000;
pub const INTERCEPT_VECTOR1_INT:u32=0x00200000;
pub const INTERCEPT_VECTOR1_INVD:u32=0x00400000;
pub const INTERCEPT_VECTOR1_PAUSE:u32=0x00800000;
pub const INTERCEPT_VECTOR1_HLT:u32=0x01000000;
pub const INTERCEPT_VECTOR1_INVLPG:u32=0x02000000;
pub const INTERCEPT_VECTOR1_INVLPGA:u32=0x04000000;
pub const INTERCEPT_VECTOR1_IO:u32=0x08000000;
pub const INTERCEPT_VECTOR1_MSR:u32=0x10000000;
pub const INTERCEPT_VECTOR1_TS:u32=0x20000000;
pub const INTERCEPT_VECTOR1_FF:u32=0x40000000;
pub const INTERCEPT_VECTOR1_SHUTDOWN:u32=0x80000000;

// Vector 2 of Control Area
pub const INTERCEPT_VECTOR2_VMRUN:u32=0x00000001;
pub const INTERCEPT_VECTOR2_VMMCALL:u32=0x00000002;
pub const INTERCEPT_VECTOR2_VMLOAD:u32=0x00000004;
pub const INTERCEPT_VECTOR2_VMSAVE:u32=0x00000008;
pub const INTERCEPT_VECTOR2_STGI:u32=0x00000010;
pub const INTERCEPT_VECTOR2_CLGI:u32=0x00000020;
pub const INTERCEPT_VECTOR2_SKINIT:u32=0x00000040;
pub const INTERCEPT_VECTOR2_RDTSCP:u32=0x00000080;
pub const INTERCEPT_VECTOR2_ICEBP:u32=0x00000100;
pub const INTERCEPT_VECTOR2_WBINVD:u32=0x00000200;
pub const INTERCEPT_VECTOR2_MONITOR:u32=0x00000400;
pub const INTERCEPT_VECTOR2_MWAIT:u32=0x00000800;
pub const INTERCEPT_VECTOR2_MWAIT_C:u32=0x00001000;
pub const INTERCEPT_VECTOR2_XSETBV:u32=0x00002000;
pub const INTERCEPT_VECTOR2_RDPRU:u32=0x00004000;
pub const INTERCEPT_VECTOR2_EFER_PW:u32=0x00008000;

// Vector 3 of Control Area
pub const INTERCEPT_VECTOR3_INVLPGB:u32=0x00000001;
pub const INTERCEPT_VECTOR3_ILLEGAL_INVLPGB:u32=0x00000002;
pub const INTERCEPT_VECTOR3_INVPCID:u32=0x00000004;
pub const INTERCEPT_VECTOR3_MCOMMIT:u32=0x00000008;
pub const INTERCEPT_VECTOR3_TLBSYNC:u32=0x00000010;

// TLB Control
pub const TLB_CONTROL_DO_NOTHING:u8=0;
pub const TLB_CONTROL_FLUSH_ENTIRE_TLB:u8=1;
pub const TLB_CONTROL_FLUSH_GUEST_TLB:u8=3;
pub const TLB_CONTROL_FLUSH_GUEST_NON_GLOBAL_TLB:u8=7;

// Use a macro to save bit operation effort.
macro_rules! build_bit_get_set
{
	($pfield:tt,$field:tt) =>
	{
		paste!
		{
			#[inline] pub fn [<get_ $field:lower>](&self)->bool
			{
				(self.0&[<$pfield:upper _ $field:upper>])==[<$pfield:upper _ $field:upper>]
			}

			#[inline] pub fn [<set_ $field:lower>](&mut self,val:bool)
			{
				if val
				{
					self.0|=[<$pfield:upper _ $field:upper>];
				}
				else
				{
					self.0&=![<$pfield:upper _ $field:upper>];
				}
			}
		}
	};
}

macro_rules! build_int_get_set
{
	($pfield:tt,$field:tt,$type:ty,$ptype:ty) =>
	{
		paste!
		{
			#[inline] pub fn [<get_ $field:lower>](&self)->$type
			{
				((self.0&[<$pfield:upper _ $field:upper _MASK>])>>[<$pfield:upper _ $field:upper _BIT_START>]) as $type
			}
	
			#[allow(arithmetic_overflow)]
			#[inline] pub fn[<set_ $field:lower>](&mut self,val:$type)
			{
				// Clear via logical and.
				self.0&=[<$pfield:upper _ $field:upper _MASK>];
				// Set via logical or.
				self.0|=(val<<[<$pfield:upper _ $field:upper _BIT_START>]) as $ptype;
			}
		}
	};
}

// Offset 0x060: AVIC Control
#[derive(Default)]
#[repr(C)] pub struct AvicControl(u64);

pub const AVIC_CONTROL_V_TPR_BIT_START:u64=0;
pub const AVIC_CONTROL_V_TPR_MASK:u64=!0xFF;
pub const AVIC_CONTROL_V_IRQ:u64=1<<8;
pub const AVIC_CONTROL_V_GIF:u64=1<<9;
pub const AVIC_CONTROL_V_NMI:u64=1<<11;
pub const AVIC_CONTROL_V_NMI_MASK:u64=1<<12;
pub const AVIC_CONTROL_V_INTR_PRIO_BIT_START:u64=16;
pub const AVIC_CONTROL_V_INTR_PRIO_MASK:u64=!0xF0000;
pub const AVIC_CONTROL_V_IGN_TPR:u64=1<<20;
pub const AVIC_CONTROL_V_INTR_MASKING:u64=1<<24;
pub const AVIC_CONTROL_V_GIF_ENABLE:u64=1<<25;
pub const AVIC_CONTROL_V_NMI_ENABLE:u64=1<<26;
pub const AVIC_CONTROL_X2AVIC_ENABLE:u64=1<<30;
pub const AVIC_CONTROL_AVIC_ENABLE:u64=1<<31;
pub const AVIC_CONTROL_V_INTR_VECTOR_BIT_START:u64=32;
pub const AVIC_CONTROL_V_INTR_VECTOR_MASK:u64=!0xFF00000000;

impl AvicControl
{
	build_int_get_set!(AVIC_CONTROL,V_TPR,u8,u64);
	build_bit_get_set!(AVIC_CONTROL,V_IRQ);
	build_bit_get_set!(AVIC_CONTROL,V_GIF);
	build_bit_get_set!(AVIC_CONTROL,V_NMI);
	build_bit_get_set!(AVIC_CONTROL,V_NMI_MASK);
	build_int_get_set!(AVIC_CONTROL,V_INTR_PRIO,u8,u64);
	build_bit_get_set!(AVIC_CONTROL,V_IGN_TPR);
	build_bit_get_set!(AVIC_CONTROL,V_INTR_MASKING);
	build_bit_get_set!(AVIC_CONTROL,V_GIF_ENABLE);
	build_bit_get_set!(AVIC_CONTROL,V_NMI_ENABLE);
	build_bit_get_set!(AVIC_CONTROL,X2AVIC_ENABLE);
	build_bit_get_set!(AVIC_CONTROL,AVIC_ENABLE);
	build_int_get_set!(AVIC_CONTROL,V_INTR_VECTOR,u8,u64);
}

// Offset 0x068: Interrupt Control
#[derive(Default)]
#[repr(C)] pub struct InterruptControl(u64);

pub const INTERRUPT_CONTROL_SHADOW:u64=1<<0;
pub const INTERRUPT_CONTROL_GUEST_RFLAGS_IF:u64=1<<1;

impl InterruptControl
{
	pub fn new()->Self
	{
		Self(0)
	}

	build_bit_get_set!(INTERRUPT_CONTROL,SHADOW);
	build_bit_get_set!(INTERRUPT_CONTROL,GUEST_RFLAGS_IF);
}

// Offset 0x090: Nested Paing
#[derive(Default)]
#[repr(C)] pub struct NptControl(u64);

pub const NPT_CONTROL_ENABLE:u64=1<<0;
pub const NPT_CONTROL_SEV_ENABLE:u64=1<<1;
pub const NPT_CONTROL_SEV_ES_ENABLE:u64=1<<2;
pub const NPT_CONTROL_GMET_ENABLE:u64=1<<3;
pub const NPT_CONTROL_SSS_CHECK_ENABLE:u64=1<<4;
pub const NPT_CONTROL_VTE_ENABLE:u64=1<<5;
pub const NPT_CONTROL_RO_GPT_ENABLE:u64=1<<6;
pub const NPT_CONTROL_INVLPGB_ENABLE:u64=1<<7;

impl NptControl
{
	build_bit_get_set!(NPT_CONTROL,ENABLE);
	build_bit_get_set!(NPT_CONTROL,SEV_ENABLE);
	build_bit_get_set!(NPT_CONTROL,SEV_ES_ENABLE);
	build_bit_get_set!(NPT_CONTROL,GMET_ENABLE);
	build_bit_get_set!(NPT_CONTROL,SSS_CHECK_ENABLE);
	build_bit_get_set!(NPT_CONTROL,VTE_ENABLE);
	build_bit_get_set!(NPT_CONTROL,RO_GPT_ENABLE);
	build_bit_get_set!(NPT_CONTROL,INVLPGB_ENABLE);
}

// Offset 0x0A8: Event Injection
#[derive(Default)]
#[repr(C)] pub struct EventInjection(u64);

pub const EVENT_INJECTION_VECTOR_BIT_START:u64=0;
pub const EVENT_INJECTION_VECTOR_MASK:u64=!0xFF;
pub const EVENT_INJECTION_TYPE_BIT_START:u64=8;
pub const EVENT_INJECTION_TYPE_MASK:u64=!0x700;
pub const EVENT_INJECTION_ERROR_CODE_VALID_BIT:u64=11;
pub const EVENT_INJECTION_ERROR_CODE_VALID:u64=1<<EVENT_INJECTION_ERROR_CODE_VALID_BIT;
pub const EVENT_INJECTION_VALID_BIT:u64=31;
pub const EVENT_INJECTION_VALID:u64=1<<EVENT_INJECTION_VALID_BIT;
pub const EVENT_INJECTION_ERROR_CODE_BIT_START:u64=32;
pub const EVENT_INJECTION_ERROR_CODE_MASK:u64=!0xFFFFFFFF00000000;

impl EventInjection
{
	#[inline] pub fn new(vector:u8,event_type:EventType,has_error_code:bool,valid:bool,error_code:u32)->Self
	{
		let code=	vector as u64 |
					((event_type as u64)<<EVENT_INJECTION_TYPE_BIT_START) |
					((has_error_code as u64)<<EVENT_INJECTION_ERROR_CODE_VALID_BIT) |
					((valid as u64)<<EVENT_INJECTION_VALID_BIT) |
					((error_code as u64)<<EVENT_INJECTION_ERROR_CODE_BIT_START);
		Self(code)
	}

	build_int_get_set!(EVENT_INJECTION,VECTOR,u8,u64);
	build_int_get_set!(EVENT_INJECTION,TYPE,u8,u64);
	build_bit_get_set!(EVENT_INJECTION,ERROR_CODE_VALID);
	build_bit_get_set!(EVENT_INJECTION,VALID);
	build_int_get_set!(EVENT_INJECTION,ERROR_CODE,u32,u64);
}

// Offset 0x0B8: LBR Virtualization
#[derive(Default)]
#[repr(C)] pub struct LbrVirtualization(u64);

pub const LBR_VIRT_ENABLE:u64=1<<0;
pub const LBR_VIRT_VMLS_ENABLE:u64=1<<1;
pub const LBR_VIRT_IBS_ENABLE:u64=1<<2;

impl LbrVirtualization
{
	build_bit_get_set!(LBR_VIRT,ENABLE);
	build_bit_get_set!(LBR_VIRT,VMLS_ENABLE);
	build_bit_get_set!(LBR_VIRT,IBS_ENABLE);
}

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