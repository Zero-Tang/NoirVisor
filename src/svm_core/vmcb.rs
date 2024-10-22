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

// Let's abuse generics here!
#[inline] pub unsafe fn vmread<T>(vmcb:*mut c_void,offset:usize)->T
{
	vmcb.byte_add(offset).cast::<T>().read()
}

#[inline] pub unsafe fn vmwrite<T>(vmcb:*mut c_void,offset:usize,value:T)
{
	vmcb.byte_add(offset).cast::<T>().write(value)
}

#[inline] pub unsafe fn vmcopy<T>(dest:*mut c_void,src:*mut c_void,offset:usize)
{
	vmwrite(dest,offset,vmread::<T>(src,offset))
}

#[inline] pub unsafe fn vmcb_and<T:BitAndAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	*vmcb.byte_add(offset).cast::<T>()&=value
}

#[inline] pub unsafe fn vmcb_or<T:BitOrAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	*vmcb.byte_add(offset).cast::<T>()|=value
}

#[inline] pub unsafe fn vmcb_xor<T:BitXorAssign>(vmcb:*mut c_void,offset:usize,value:T)
{
	*vmcb.byte_add(offset).cast::<T>()^=value
}

#[inline] pub unsafe fn vmcb_bt32(vmcb:*mut c_void,offset:usize,pos:u32)->bool
{
	let p=vmcb.byte_add(offset) as usize;
	let flag:u8;
	asm!
	(
		"bt dword [{ptr}],{pos:e}",
		"setc al",
		ptr=in(reg) p,
		pos=in(reg) pos,
		out("al") flag
	);
	return flag!=0;
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
pub const INTERCEPT_INSTRUCTION1:usize=0xC;
pub const INTERCEPT_INSTRUCTION2:usize=0x10;
pub const INTERCEPT_WRITE_CR_POST:usize=0x12;
pub const INTERCEPT_INSTRUCTION3:usize=0x14;
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