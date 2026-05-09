/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file manages VMCS in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{fmt, ops::{BitAndAssign, BitOrAssign}};

use bitfield_struct::bitfield;
use paste::paste;

use crate::{vt_core::VtVcpu, xpf_core::{asm::vt::*, x86::{crdr::Dr6, interrupts::EventType, rflags::Rflags}}, *};

// 16-Bit Control Fields
pub const GUEST_VPID:usize=0x0;
pub const POSTED_INTERRUPT_NOTIFICATION_VECTOR:usize=0x2;
pub const EPT_POINTER_INDEX:usize=0x4;
pub const HLAT_PREFIX_SIZE:usize=0x6;
pub const LAST_PID_POINTER_INDEX:usize=0x8;
// 16-Bit Guest State Fields
pub const GUEST_ES_SELECTOR:usize=0x800;
pub const GUEST_CS_SELECTOR:usize=0x802;
pub const GUEST_SS_SELECTOR:usize=0x804;
pub const GUEST_DS_SELECTOR:usize=0x806;
pub const GUEST_FS_SELECTOR:usize=0x808;
pub const GUEST_GS_SELECTOR:usize=0x80A;
pub const GUEST_LDTR_SELECTOR:usize=0x80C;
pub const GUEST_TR_SELECTOR:usize=0x80E;
pub const GUEST_INTERRUPT_STATUS:usize=0x810;
pub const PML_INDEX:usize=0x812;
pub const GUEST_UINTV:usize=0x814;
// 16-Bit Host State Fields
pub const HOST_ES_SELECTOR:usize=0xC00;
pub const HOST_CS_SELECTOR:usize=0xC02;
pub const HOST_SS_SELECTOR:usize=0xC04;
pub const HOST_DS_SELECTOR:usize=0xC06;
pub const HOST_FS_SELECTOR:usize=0xC08;
pub const HOST_GS_SELECTOR:usize=0xC0A;
pub const HOST_TR_SELECTOR:usize=0xC0C;
// 64-Bit Control Fields
pub const ADDRESS_OF_IO_BITMAP_A:usize=0x2000;
pub const ADDRESS_OF_IO_BITMAP_B:usize=0x2002;
pub const ADDRESS_OF_MSR_BITMAP:usize=0x2004;
pub const VMEXIT_MSR_STORE_ADDRESS:usize=0x2006;
pub const VMEXIT_MSR_LOAD_ADDRESS:usize=0x2008;
pub const VMENTRY_MSR_LOAD_ADDRESS:usize=0x200A;
pub const EXECUTIVE_VMCS_POINTER:usize=0x200C;
pub const PML_ADDRESS:usize=0x200E;
pub const TSC_OFFSET:usize=0x2010;
pub const VIRTUAL_APIC_ADDRESS:usize=0x2012;
pub const APIC_ACCESS_ADDRESS:usize=0x2014;
pub const POSTED_INTERRUPT_DESCRIPTOR_ADDRESS:usize=0x2016;
pub const VM_FUNCTION_CONTROLS:usize=0x2018;
pub const EPT_POINTER:usize=0x201A;
pub const EOI_EXIT_BITMAP0:usize=0x201C;
pub const EOI_EXIT_BITMAP1:usize=0x201E;
pub const EOI_EXIT_BITMAP2:usize=0x2020;
pub const EOI_EXIT_BITMAP3:usize=0x2022;
pub const EPTP_LIST_ADDRESS:usize=0x2024;
pub const VMREAD_BITMAP_ADDRESS:usize=0x2026;
pub const VMWRITE_BITMAP_ADDRESS:usize=0x2028;
pub const VE_INFORMATION_ADDRESS:usize=0x202A;
pub const XSS_EXITING_BITMAP:usize=0x202C;
pub const ENCLS_EXITING_BITMAP:usize=0x202E;
pub const SUB_PAGE_PERMISSION_TABLE:usize=0x2030;
pub const TSC_MULTIPLIER:usize=0x2032;
pub const TERTIARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS:usize=0x2034;
pub const ENCLV_EXITING_BITMAP:usize=0x2036;
pub const LOW_PASID_DIRECTORY_ADDRESS:usize=0x2038;
pub const HIGH_PASID_DIRECTORY_ADDRESS:usize=0x203A;
pub const SHARED_EPT_POINTER:usize=0x203C;
pub const PCONFIG_EXITING_BITMAP:usize=0x203E;
pub const HLAT_POINTER:usize=0x2040;
pub const PID_POINTER_TABLE_ADDRESS:usize=0x2042;
pub const SECONDARY_VMEXIT_CONTROLS:usize=0x2044;
pub const SPEC_CTRL_MASK:usize=0x204A;
pub const SPEC_CTRL_SHADOW:usize=0x204C;
pub const INJECTED_EVENT_DATA:usize=0x2052;
// 64-Bit Read-Only Fields
pub const GUEST_PHYSICAL_ADDRESS:usize=0x2400;
pub const GUEST_MSR_LIST_DATA:usize=0x2402;
pub const ORIGINAL_EVENT_DATA:usize=0x2404;
// 64-Bit Guest State Fields
pub const VMCS_LINK_POINTER:usize=0x2800;
pub const GUEST_MSR_IA32_DEBUG_CTRL:usize=0x2802;
pub const GUEST_MSR_IA32_PAT:usize=0x2804;
pub const GUEST_MSR_IA32_EFER:usize=0x2806;
pub const GUEST_MSR_IA32_PERF_GLOBAL_CTRL:usize=0x2808;
pub const GUEST_PDPTE0:usize=0x280A;
pub const GUEST_PDPTE1:usize=0x280C;
pub const GUEST_PDPTE2:usize=0x280E;
pub const GUEST_PDPTE3:usize=0x2810;
pub const GUEST_MSR_IA32_BOUND_CONFIG:usize=0x2812;
pub const GUEST_MSR_IA32_RTIT_CTRL:usize=0x2814;
pub const GUEST_MSR_IA32_LBR_CTRL:usize=0x2816;
pub const GUEST_MSR_IA32_PKRS:usize=0x2818;
pub const GUEST_MSR_IA32_FRED_CONFIG:usize=0x281A;
pub const GUEST_MSR_IA32_FRED_RSP1:usize=0x281C;
pub const GUEST_MSR_IA32_FRED_RSP2:usize=0x281E;
pub const GUEST_MSR_IA32_FRED_RSP3:usize=0x2820;
pub const GUEST_MSR_IA32_FRED_STKLVLS:usize=0x2822;
pub const GUEST_MSR_IA32_FRED_SSP1:usize=0x2824;
pub const GUEST_MSR_IA32_FRED_SSP2:usize=0x2826;
pub const GUEST_MSR_IA32_FRED_SSP3:usize=0x2828;
// 64-Bit Host State Fields
pub const HOST_MSR_IA32_PAT:usize=0x2C00;
pub const HOST_MSR_IA32_EFER:usize=0x2C02;
pub const HOST_MSR_IA32_PERF_GLOBAL_CTRL:usize=0x2C04;
pub const HOST_MSR_IA32_PKRS:usize=0x2C06;
pub const HOST_MSR_IA32_FRED_CONFIG:usize=0x2C08;
pub const HOST_MSR_IA32_FRED_RSP1:usize=0x2C0A;
pub const HOST_MSR_IA32_FRED_RSP2:usize=0x2C0C;
pub const HOST_MSR_IA32_FRED_RSP3:usize=0x2C0E;
pub const HOST_MSR_IA32_FRED_STKLVLS:usize=0x2C10;
pub const HOST_MSR_IA32_FRED_SSP1:usize=0x2C12;
pub const HOST_MSR_IA32_FRED_SSP2:usize=0x2C14;
pub const HOST_MSR_IA32_FRED_SSP3:usize=0x2C16;
// 32-Bit Control Fields
pub const PIN_BASED_VM_EXECUTION_CONTROLS:usize=0x4000;
pub const PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS:usize=0x4002;
pub const EXCEPTION_BITMAP:usize=0x4004;
pub const PAGE_FAULT_ERROR_CODE_MASK:usize=0x4006;
pub const PAGE_FAULT_ERROR_CODE_MATCH:usize=0x4008;
pub const CR3_TARGET_COUNT:usize=0x400A;
pub const VMEXIT_CONTROLS:usize=0x400C;
pub const VMEXIT_MSR_STORE_COUNT:usize=0x400E;
pub const VMEXIT_MSR_LOAD_COUNT:usize=0x4010;
pub const VMENTRY_CONTROLS:usize=0x4012;
pub const VMENTRY_MSR_LOAD_COUNT:usize=0x4014;
pub const VMENTRY_INTERRUPTION_INFORMATION_FIELD:usize=0x4016;
pub const VMENTRY_EXCEPTION_ERROR_CODE:usize=0x4018;
pub const VMENTRY_INSTRUCTION_LENGTH:usize=0x401A;
pub const TPR_THRESHOLD:usize=0x401C;
pub const SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS:usize=0x401E;
pub const PLE_GAP:usize=0x4020;
pub const PLE_WINDOW:usize=0x4022;
pub const INSTRUCTION_TIMEOUT_CONTROL:usize=0x4024;
pub const SEAM_GUEST_KEY_ID:usize=0x4026;
// 32-Bit Read-Only Fields
pub const VM_INSTRUCTION_ERROR:usize=0x4400;
pub const VMEXIT_REASON:usize=0x4402;
pub const VMEXIT_INTERRUPTION_INFORMATION:usize=0x4404;
pub const VMEXIT_INTERRUPTION_ERROR_CODE:usize=0x4406;
pub const IDT_VECTORING_INFORMATION:usize=0x4408;
pub const IDT_VECTORING_ERROR_CODE:usize=0x440A;
pub const VMEXIT_INSTRUCTION_LENGTH:usize=0x440C;
pub const VMEXIT_INSTRUCTION_INFORMATION:usize=0x440E;
// 32-Bit Guest State Fields
pub const GUEST_ES_LIMIT:usize=0x4800;
pub const GUEST_CS_LIMIT:usize=0x4802;
pub const GUEST_SS_LIMIT:usize=0x4804;
pub const GUEST_DS_LIMIT:usize=0x4806;
pub const GUEST_FS_LIMIT:usize=0x4808;
pub const GUEST_GS_LIMIT:usize=0x480A;
pub const GUEST_LDTR_LIMIT:usize=0x480C;
pub const GUEST_TR_LIMIT:usize=0x480E;
pub const GUEST_GDTR_LIMIT:usize=0x4810;
pub const GUEST_IDTR_LIMIT:usize=0x4812;
pub const GUEST_ES_ACCESS_RIGHTS:usize=0x4814;
pub const GUEST_CS_ACCESS_RIGHTS:usize=0x4816;
pub const GUEST_SS_ACCESS_RIGHTS:usize=0x4818;
pub const GUEST_DS_ACCESS_RIGHTS:usize=0x481A;
pub const GUEST_FS_ACCESS_RIGHTS:usize=0x481C;
pub const GUEST_GS_ACCESS_RIGHTS:usize=0x481E;
pub const GUEST_LDTR_ACCESS_RIGHTS:usize=0x4820;
pub const GUEST_TR_ACCESS_RIGHTS:usize=0x4822;
pub const GUEST_INTERRUPTIBILITY_STATE:usize=0x4824;
pub const GUEST_ACTIVITY_STATE:usize=0x4826;
pub const GUEST_SMBASE:usize=0x4828;
pub const GUEST_MSR_IA32_SYSENTER_CS:usize=0x482A;
pub const VMX_PREEMPTION_TIMER_VALUE:usize=0x482E;
// 32-Bit Host State Fields
pub const HOST_MSR_IA32_SYSENTER_CS:usize=0x4C00;
// Natural-Width Control Fields
pub const CR0_GUEST_HOST_MASK:usize=0x6000;
pub const CR4_GUEST_HOST_MASK:usize=0x6002;
pub const CR0_READ_SHADOW:usize=0x6004;
pub const CR4_READ_SHADOW:usize=0x6006;
pub const CR3_TARGET_VALUE0:usize=0x6008;
pub const CR3_TARGET_VALUE1:usize=0x600A;
pub const CR3_TARGET_VALUE2:usize=0x600C;
pub const CR3_TARGET_VALUE3:usize=0x600E;
// Natural-Width Read-Only Fields
pub const VMEXIT_QUALIFICATION:usize=0x6400;
pub const IO_RCX:usize=0x6402;
pub const IO_RSI:usize=0x6404;
pub const IO_RDI:usize=0x6406;
pub const IO_RIP:usize=0x6408;
pub const GUEST_LINEAR_ADDRESS:usize=0x640A;
// Natural-Width Guest State Fields
pub const GUEST_CR0:usize=0x6800;
pub const GUEST_CR3:usize=0x6802;
pub const GUEST_CR4:usize=0x6804;
pub const GUEST_ES_BASE:usize=0x6806;
pub const GUEST_CS_BASE:usize=0x6808;
pub const GUEST_SS_BASE:usize=0x680A;
pub const GUEST_DS_BASE:usize=0x680C;
pub const GUEST_FS_BASE:usize=0x680E;
pub const GUEST_GS_BASE:usize=0x6810;
pub const GUEST_LDTR_BASE:usize=0x6812;
pub const GUEST_TR_BASE:usize=0x6814;
pub const GUEST_GDTR_BASE:usize=0x6816;
pub const GUEST_IDTR_BASE:usize=0x6818;
pub const GUEST_DR7:usize=0x681A;
pub const GUEST_RSP:usize=0x681C;
pub const GUEST_RIP:usize=0x681E;
pub const GUEST_RFLAGS:usize=0x6820;
pub const GUEST_PENDING_DEBUG_EXCEPTIONS:usize=0x6822;
pub const GUEST_MSR_IA32_SYSENTER_ESP:usize=0x6824;
pub const GUEST_MSR_IA32_SYSENTER_EIP:usize=0x6826;
pub const GUEST_S_CET:usize=0x6828;
pub const GUEST_SSP:usize=0x682A;
pub const GUEST_MSR_IA32_INTERRUPT_SSP_TABLE_ADDR:usize=0x682C;
// Natural-Width Host State Fields
pub const HOST_CR0:usize=0x6C00;
pub const HOST_CR3:usize=0x6C02;
pub const HOST_CR4:usize=0x6C04;
pub const HOST_FS_BASE:usize=0x6C06;
pub const HOST_GS_BASE:usize=0x6C08;
pub const HOST_TR_BASE:usize=0x6C0A;
pub const HOST_GDTR_BASE:usize=0x6C0C;
pub const HOST_IDTR_BASE:usize=0x6C0E;
pub const HOST_MSR_IA32_SYSENTER_ESP:usize=0x6C10;
pub const HOST_MSR_IA32_SYSENTER_EIP:usize=0x6C12;
pub const HOST_RSP:usize=0x6C14;
pub const HOST_RIP:usize=0x6C16;
pub const HOST_S_CET:usize=0x6C18;
pub const HOST_SSP:usize=0x6C1A;
pub const HOST_MSR_IA32_INTERRUPT_SSP_TABLE_ADDR:usize=0x6C1C;

#[derive(Debug)]
pub struct VmcsSegment
{
	pub selector:u16,
	pub acces_rights:u32,
	pub limit:u32,
	pub base:usize
}

macro_rules! read_guest_segment
{
	($name:tt) =>
	{
		paste!
		{
			VmcsSegment
			{
				selector:vmread16([<GUEST_ $name:upper _SELECTOR>]).unwrap(),
				acces_rights:vmread32([<GUEST_ $name:upper _ACCESS_RIGHTS>]).unwrap(),
				limit:vmread32([<GUEST_ $name:upper _LIMIT>]).unwrap(),
				base:vmreadptr([<GUEST_ $name:upper _BASE>]).unwrap()
			}
		}
	};
}

// Used as LazyCell-like indicators.
// The cached items represented in this bitfield become invalid after VM-Exit, thus reducing the number of `vmread` to once per field.
#[bitfield(u64)] struct CleanExitFields
{
	rflags:bool,
	cs_ar:bool,
	exit_instruction_length:bool,
	interruptibility:bool,
	#[bits(60)] rsvd:u64
}

// Used as LazyCell-like indicators.
// The cached items represented in this bitfield are still valid after VM-Exit.
// Can be used to reduce number of `vmwrite` to once per-field and prevent unnecessary `vmread`.
#[bitfield(u64)] struct DirtyExitFields
{
	proc_ctrl1:bool,
	interruptibility:bool,
	#[bits(62)] rsvd:u64
}

#[derive(Default)]
pub struct CachedExitContext
{
	clean_fields:CleanExitFields,
	dirty_fields:DirtyExitFields,
	pub rip:u64,
	pub rsp:u64,
	rflags:Rflags,
	cs_ar:SegmentAccessRights,
	pub exit_reason:VmxExitReason,
	interruptibility:InterruptibilityState,
	exit_instruction_length:u32,
	proc_ctrl1:VmxPrimaryProcessorControls
}

macro_rules! build_clean_method
{
	($name:tt,$const:expr,$type:ty,$bitness:tt)=>
	{
		paste!
		{
			#[inline(always)] pub fn [<force_eval_ $name>](&mut self)
			{
				if !self.clean_fields.$name()
				{
					self.clean_fields.[<set_ $name>](true);
					self.$name=$type::from(unsafe{vmread_unchecked($const)} as [<u $bitness>]);
				}
			}

			#[inline(always)] pub fn $name(&mut self)->$type
			{
				self.[<force_eval_ $name>]();
				self.$name
			}
		}
	};
}

macro_rules! build_dirty_method
{
	($name:tt,$type:ty)=>
	{
		paste!
		{
			#[inline(always)] pub fn [<write_ $name>](&mut self,value:$type)
			{
				self.$name=value;
				self.dirty_fields.[<set_ $name>](true);
			}

			#[inline(always)] pub fn [<get_ $name _mut>](&mut self)->&mut $type
			{
				self.dirty_fields.[<set_ $name>](true);
				&mut self.$name
			}
		}
	};
	($name:tt,$type:ty,$reeval:tt)=>
	{
		paste!
		{
			#[inline(always)] pub fn [<write_ $name>](&mut self,value:$type)
			{
				self.$name=value;
				self.dirty_fields.[<set_ $name>](true);
			}

			#[inline(always)] pub fn [<get_ $name _mut>](&mut self)->&mut $type
			{
				self.[<force_eval_ $name>]();
				self.dirty_fields.[<set_ $name>](true);
				&mut self.$name
			}
		}
	}
}

impl CachedExitContext
{
	pub fn reset(&mut self)
	{
		self.clean_fields=CleanExitFields::new();
		// The following fields are forced to reevaluate on every VM-Exit!
		self.rip=vmread64(GUEST_RIP).unwrap();
		self.rsp=vmread64(GUEST_RSP).unwrap();
		self.exit_reason=VmxExitReason::from_bits(vmread32(VMEXIT_REASON).unwrap());
	}

	pub fn flush(&mut self)
	{
		macro_rules! flush_field
		{
			($field:tt,$const:expr,$value:expr)=>
			{
				if self.dirty_fields.$field()
				{
					unsafe
					{
						vmwrite_unchecked($const,$value as usize);
					}
				}
			};
		}
		flush_field!(proc_ctrl1,PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,self.proc_ctrl1.0);
		flush_field!(interruptibility,GUEST_INTERRUPTIBILITY_STATE,self.interruptibility.0);
		self.dirty_fields=DirtyExitFields::new();
	}

	build_clean_method!(cs_ar,GUEST_CS_ACCESS_RIGHTS,SegmentAccessRights,32);
	build_clean_method!(rflags,GUEST_RFLAGS,Rflags,64);
	build_clean_method!(exit_instruction_length,VMEXIT_INSTRUCTION_LENGTH,u32,32);
	build_clean_method!(interruptibility,GUEST_INTERRUPTIBILITY_STATE,InterruptibilityState,32);
	build_dirty_method!(proc_ctrl1,VmxPrimaryProcessorControls);
	build_dirty_method!(interruptibility,InterruptibilityState,reeval);
}

impl VtVcpu
{
	/// ## `advance_rip_manually` method
	/// This method advances `rip` with a specified instruction length.
	/// 
	/// This method **will not modify** the `rip` in the `CachedExitContext`.
	#[inline(always)] pub fn advance_rip_manually(&mut self,length:u32)
	{
		let rflags=self.cached_ctxt.rflags();
		let mut rip=self.cached_ctxt.rip+length as u64;
		if rflags.tf()
		{
			// Single-Stepping is enabled! Injecting #DB exception...
			let mut new_dr6=Dr6::from_bits(vmreadptr(GUEST_PENDING_DEBUG_EXCEPTIONS).unwrap() as u64);
			new_dr6.set_bs(true);
			vmwriteptr(GUEST_PENDING_DEBUG_EXCEPTIONS,new_dr6.into_bits() as usize);
			// Remove the interrupt shadowing.
			let interruptibility=self.cached_ctxt.get_interruptibility_mut();
			interruptibility.set_blocking_by_sti(false);
			interruptibility.set_blocking_by_mov_ss(false);
		}
		let cs_ar=self.cached_ctxt.cs_ar();
		if !cs_ar.long_mode()
		{
			// The rip might overflow if the guest is not in long mode.
			rip&=u32::MAX as u64;
		}
		vmwrite64(GUEST_RIP,rip);
	}

	/// ## `advance_rip` method
	/// This method advances `rip` with instruction length specified in VMCS.
	/// 
	/// This method **will not modify** the `rip` in the `CachedExitContext`.
	pub fn advance_rip(&mut self)
	{
		let len=self.cached_ctxt.exit_instruction_length();
		self.advance_rip_manually(len);
	}
}

#[inline] pub unsafe fn inject_event(vector:u8,event_type:EventType,error_code:Option<u32>,valid:bool,length:u32)
{
	let mut evt=VmxEntryInterruptionInformation(0);
	evt.set_vector(vector);
	evt.set_interruption_type(event_type as u32);
	evt.set_valid(valid);
	if let Some(code)=error_code
	{
		evt.set_deliver_error_code(true);
		vmwrite32(VMENTRY_EXCEPTION_ERROR_CODE,code);
	}
	vmwrite32(VMENTRY_INTERRUPTION_INFORMATION_FIELD,evt.0);
	vmwrite32(VMENTRY_INSTRUCTION_LENGTH,length);
}

impl<T:fmt::Display> fmt::Display for VmxResult<T>
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		match self
		{
			VmxResult::Ok(v)=>write!(f,"VMX Instruction succeeded and returned {v}!"),
			VmxResult::Err(e)=>match VMX_INSTRUCTION_ERROR_MESSAGE.get(*e as usize)
			{
				Some(Some(v))=>write!(f,"VMX Instruction failed! Reason: {v}"),
				_=>write!(f,"VMX Instruction failed with invalid number {e}!")
			},
			VmxResult::NoVmcs=>f.write_str("VMX Instruction failed while no VMCS was loaded!")
		}
	}
}

pub const VMX_INSTRUCTION_ERROR_MESSAGE:[Option<&str>;29]=
[
	Some("Invalid Error, Number=0!"),								// Error=0
	Some("vmcall is executed in VMX Root Operation!"),				// Error=1
	Some("vmclear is given invalid physical address as operand!"),	// Error=2
	Some("vmclear is given vmxon region as operand!"),				// Error=3
	Some("vmlaunch is given non-clear vmcs as operand!"),			// Error=4
	Some("vmresume is given non-launched vmcs as operand!"),		// Error=5
	Some("vmresume is executed after vmxoff!"),						// Error=6
	Some("VM-Entry failed due to invalid control fields!"),			// Error=7
	Some("VM-Entry failed due to invalid host state!"),				// Error=8
	Some("vmptrld is given invalid physical address as operand!"),	// Error=9
	Some("vmptrld is given vmxon region as operand!"),				// Error=10
	Some("vmptrld is given vmcs with incorrect revision id!"),		// Error=11
	Some("vmread/vmwrite is given unsupported field as operand!"),	// Error=12
	Some("vmwrite is given read-only field as operand!"),			// Error=13
	None,															// Error=14
	Some("vmxon is executed in VMX Root Operation!"),				// Error=15
	Some("VM-Entry failed due to invalid executive-vmcs!"),			// Error=16
	Some("VM-Entry failed due to non-launched executive-vmcs!"),	// Error=17
	// Error=18
	Some("VM-Entry failed due to executive-vmcs not vmxon region! (Are you attempting to deactivate dual-monitor treatment?)"),
	// Error=19
	Some("VM-Entry failed due to non-clear vmcs! (Are you attempting to deactivate dual-monitor treatment?)"),
	Some("vmcall is given invalid vmexit control fields!"),			// Error=20
	None,															// Error=21
	// Error=22
	Some("vmcall is given incorrect mseg revision id! (Are you attempting to deactivate dual-monitor treatment?)"),
	Some("vmxoff is executed under dual-monitor treatment!"),		// Error=23
	// Error=24
	Some("vmcall is given invalid SMM-Monitor features! (Are you attempting to activate dual-monitor treatment?)"),
	// Error=25
	Some("VM-Entry failed due to invalid VM-Execution Control! (Are attempting to return from SMM?)"),
	Some("VM-Entry failed due to events blocked by mov ss!"),		// Error=26,
	None,															// Error=27
	Some("Invalid Operand to invept/invvpid Instructions!"),		// Error=28
];

#[derive(Default, Clone, Copy)]
#[repr(C,align(16))] pub struct VmxMsrAutoItem
{
	pub index:u32,
	reserved:u32,
	pub data:u64
}

impl VmxMsrAutoItem
{
	pub fn new(index:u32,data:u64)->Self
	{
		Self
		{
			index,
			reserved:0,
			data
		}
	}
}

#[bitfield(u32)] pub struct SegmentAccessRights
{
	#[bits(4)] pub segment_type:u32,
	pub descriptor_type:bool,
	#[bits(2)] pub dpl:u32,
	pub present:bool,
	#[bits(4)] rsvd0:u32,
	pub avl:bool,
	pub long_mode:bool,
	pub default_size:bool,
	pub granularity:bool,
	pub unusable:bool,
	#[bits(15)] rsvd1:u32
}

impl SegmentAccessRights
{
	pub fn from_raw(selector:u16,attrib:u16)->Self
	{
		let mut v=Self(attrib as u32);
		v.set_rsvd0(0);
		if selector==0
		{
			v.set_unusable(true);
		}
		v
	}
}

#[bitfield(u32)] pub struct InterruptibilityState
{
	pub blocking_by_sti:bool,
	pub blocking_by_mov_ss:bool,
	pub blocking_by_smi:bool,
	pub blocking_by_nmi:bool,
	pub enclave_interruption:bool,
	#[bits(27)] pub rsvd:u32
}

macro_rules! derive_bit_ops
{
	($type_name:tt) =>
	{
		impl BitAndAssign for $type_name
		{
			fn bitand_assign(&mut self,rhs:Self)
			{
				self.0&=rhs.0;
			}
		}

		impl BitOrAssign for $type_name
		{
			fn bitor_assign(&mut self,rhs:Self)
			{
				self.0|=rhs.0;
			}
		}
	};
}

#[bitfield(u32)] pub struct VmxPinBasedControls
{
	pub external_interrupt_exiting:bool,
	#[bits(2)] rsvd0:u32,
	pub nmi_exiting:bool,
	rsvd1:bool,
	pub virtual_nmi:bool,
	pub activate_vmx_preemption_timer:bool,
	pub process_posted_interrupts:bool,
	#[bits(24)] rsvd2:u32
}

derive_bit_ops!(VmxPinBasedControls);

#[bitfield(u32)] pub struct VmxPrimaryProcessorControls
{
	#[bits(2)] rsvd0:u32,
	pub interrupt_window_exiting:bool,
	pub use_tsc_offsetting:bool,
	#[bits(3)] rsvd1:u32,
	pub hlt_exiting:bool,
	rsvd2:bool,
	pub invlpg_exiting:bool,
	pub mwait_exiting:bool,
	pub rdpmc_exiting:bool,
	pub rdtsc_exiting:bool,
	#[bits(2)] rsvd3:u32,
	pub cr3_load_exiting:bool,
	pub cr3_store_exiting:bool,
	pub activate_tertiary_controls:bool,
	rsvd4:bool,
	pub cr8_load_exiting:bool,
	pub cr8_store_exiting:bool,
	pub use_tpr_shadow:bool,
	pub nmi_window_exiting:bool,
	pub mov_dr_exiting:bool,
	pub unconditional_io_exiting:bool,
	pub use_io_bitmap:bool,
	rsvd5:bool,
	pub monitor_trap_flag:bool,
	pub use_msr_bitmap:bool,
	pub monitor_exiting:bool,
	pub pause_exiting:bool,
	pub activate_secondary_controls:bool
}

derive_bit_ops!(VmxPrimaryProcessorControls);

#[bitfield(u32)] pub struct VmxSecondaryProcessorControls
{
	pub virtualize_apic_accesses:bool,
	pub enable_ept:bool,
	pub descriptor_table_exiting:bool,
	pub enable_rdtscp:bool,
	pub virtualize_x2apic_mode:bool,
	pub enable_vpid:bool,
	pub wbinvd_exiting:bool,
	pub unrestricted_guest:bool,
	pub apic_register_virtualization:bool,
	pub virtual_interrupt_delivery:bool,
	pub pause_loop_exiting:bool,
	pub rdrand_exiting:bool,
	pub enable_invpcid:bool,
	pub enable_vmfunc:bool,
	pub vmcs_shadowing:bool,
	pub encls_exiting:bool,
	pub rdseed_exiting:bool,
	pub enable_pml:bool,
	pub ept_violation_to_ve:bool,
	pub conceal_vmx_from_pt:bool,
	pub enable_xsaves_xrstors:bool,
	pub enable_pasid_transnlation:bool,
	pub ept_mbec:bool,
	pub ept_spp:bool,
	pub pt_use_gpa:bool,
	pub use_tsc_scaling:bool,
	pub enable_umwait:bool,
	pub enable_pconfig:bool,
	pub enclv_exiting:bool,
	rsvd0:bool,
	pub vmm_buslock_detection:bool,
	pub instruction_timeout:bool
}

derive_bit_ops!(VmxSecondaryProcessorControls);

#[bitfield(u64)] pub struct VmxTertiaryProcessorControls
{
	pub loadiwkey_exiting:bool,
	pub enable_hlat:bool,
	pub ept_paging_write_control:bool,
	pub guest_paging_verification:bool,
	pub ipi_virtualization:bool,
	rsvd0:bool,
	pub enable_msr_list_instruction:bool,
	pub virtualize_spec_ctrl:bool,
	#[bits(56)] rsvd1:u64
}

derive_bit_ops!(VmxTertiaryProcessorControls);

#[bitfield(u64)] pub struct VmxEptPointer
{
	#[bits(3)] pub ept_memory_type:u64,
	#[bits(3)] pub page_walk_length:u64,
	pub enable_ad_flags:bool,
	pub enable_sss_enforcement:bool,
	#[bits(4)] pub rsvd:u64,
	#[bits(52)] pub eptp_pa:u64
}

#[bitfield(u32)] pub struct VmxExitControls
{
	#[bits(2)] rsvd0:u32,
	pub save_debug_controls:bool,
	#[bits(6)] rsvd1:u32,
	pub host_address_space_size:bool,
	#[bits(2)] rsvd2:u32,
	pub load_perf_global_ctrl:bool,
	#[bits(2)] rsvd3:u32,
	pub acknowledge_interrupt_on_exit:bool,
	#[bits(2)] rsvd4:u32,
	pub save_pat:bool,
	pub load_pat:bool,
	pub save_efer:bool,
	pub load_efer:bool,
	pub save_vmx_preemption_timer_value:bool,
	pub clear_bndcfgs:bool,
	pub conceal_vmx_from_pt:bool,
	pub clear_rtit_ctrl:bool,
	pub clear_lbr_ctrl:bool,
	pub clear_uinv:bool,
	pub load_cet:bool,
	pub load_pkrs:bool,
	pub save_perf_global_ctrl:bool,
	pub activate_secondary_controls:bool,
}

derive_bit_ops!(VmxExitControls);

#[bitfield(u64)] pub struct VmxExitControls2
{
	#[bits(3)] rsvd0:u64,
	pub prematurely_busy_shadow_stack:bool,
	#[bits(60)] rsvd1:u64
}

derive_bit_ops!(VmxExitControls2);

#[bitfield(u32)] pub struct VmxEntryControls
{
	#[bits(2)] rsvd0:u32,
	pub load_debug_controls:bool,
	#[bits(6)] rsvd1:u32,
	pub ia32e_mode_guest:bool,
	pub entry_to_smm:bool,
	pub deactivate_dual_monitor:bool,
	rsvd2:bool,
	pub load_perf_global_ctrl:bool,
	pub load_pat:bool,
	pub load_efer:bool,
	pub load_bndcfgs:bool,
	pub conceal_vmx_from_pt:bool,
	pub load_rtit_ctrl:bool,
	pub load_uinv:bool,
	pub load_cet:bool,
	pub load_lbr_ctrl:bool,
	pub load_pkrs:bool,
	#[bits(9)] rsvd3:u32
}

derive_bit_ops!(VmxEntryControls);

#[bitfield(u32)] pub struct VmxEntryInterruptionInformation
{
	pub vector:u8,
	#[bits(3)] pub interruption_type:u32,
	pub deliver_error_code:bool,
	#[bits(19)] rsvd:u32,
	pub valid:bool
}

#[bitfield(u32)] pub struct VmxExitReason
{
	pub basic_exit_reason:u16,
	#[bits(9)] rsvd0:u32,
	pub exit_causes_prematurely_busy_shadow_stack:bool,
	pub exit_after_buslock_assertion:bool,
	pub exit_in_enclave:bool,
	pub pending_mtf_exit:bool,
	pub exit_from_root_operation:bool,
	rsvd1:bool,
	pub entry_failure:bool
}

#[bitfield(u32)] pub struct VmxExitInterruptionInformation
{
	pub vector:u8,
	#[bits(3)] pub interruption_type:u32,
	pub error_code_valid:bool,
	pub nmi_unblocking_due_to_iret:bool,
	#[bits(18)] rsvd:u32,
	pub valid:bool
}

#[bitfield(u32)] pub struct VmxIdtVectoringInformation
{
	pub vector:u8,
	#[bits(3)] pub interruption_type:u32,
	pub error_code_valid:bool,
	#[bits(19)] rsvd:u32,
	pub valid:bool
}

macro_rules! derive_qualification_reader
{
	() =>
	{
		#[inline] pub fn read()->Self
		{
			Self::from_bits(vmread32(VMEXIT_QUALIFICATION).unwrap())
		}
	};
}

#[bitfield(u32)] pub struct DebugExceptionQualification
{
	pub b0:bool,
	pub b1:bool,
	pub b2:bool,
	pub b3:bool,
	#[bits(9)] rsvd0:u32,
	pub bd:bool,
	pub bs:bool,
	rsvd1:bool,
	pub rtm:bool,
	#[bits(15)] rsvd2:u32
}

impl DebugExceptionQualification
{
	derive_qualification_reader!();
}

#[bitfield(u32)] pub struct TaskSwitchQualification
{
	pub tss_selector:u16,
	#[bits(14)] rsvd:u32,
	#[bits(2)] pub source:u32
}

impl TaskSwitchQualification
{
	derive_qualification_reader!();

	pub const CALL_INSTRUCTION:usize=0;
	pub const IRET_INSTRUCTION:usize=1;
	pub const JMP_INSTRUCTION:usize=2;
	pub const TASK_GATE:usize=3;
}

#[bitfield(u32)] pub struct ControlRegisterQualification
{
	#[bits(4)] pub cr_index:usize,
	#[bits(2)] pub access_type:usize,
	pub is_lmsw_memory_op:bool,
	rsvd0:bool,
	#[bits(4)] pub gpr_index:usize,
	#[bits(4)] rsvd1:usize,
	pub lmsw_data:u16
}
impl ControlRegisterQualification
{
	derive_qualification_reader!();

	pub const WRITE_CR:usize=0;
	pub const READ_CR:usize=1;
	pub const CLTS_OP:usize=2;
	pub const LMSW_OP:usize=3;
}

#[bitfield(u32)] pub struct DebugRegisterQualification
{
	#[bits(3)] pub dr_index:usize,
	pub is_read:bool,
	#[bits(4)] rsvd0:u32,
	#[bits(4)] pub gpr_index:usize,
	#[bits(20)] rsvd1:u32
}

impl DebugRegisterQualification
{
	derive_qualification_reader!();
}

#[bitfield(u32)] pub struct IoQualification
{
	#[bits(3)] pub access_size:usize,
	pub is_input:bool,
	pub is_string:bool,
	pub is_repeat:bool,
	pub is_immediate:bool,
	#[bits(9)] rsvd:u32,
	pub port_number:u16
}

impl IoQualification
{
	derive_qualification_reader!();
}

#[bitfield(u32)] pub struct ApicAccessQualification
{
	#[bits(12)] pub apic_offset:u64,
	#[bits(4)] pub access_type:usize,
	pub async_op:bool,
	#[bits(15)] rsvd:u32
}

impl ApicAccessQualification
{
	derive_qualification_reader!();

	pub const LINEAR_READ:usize=0;
	pub const LINEAR_WRITE:usize=1;
	pub const LINEAR_EXECUTE:usize=2;
	pub const LINEAR_ACCESS_DURING_EVENT_DELIVERY:usize=3;
	pub const LINEAR_ACCESS_FOR_MONITORING:usize=4;
	pub const GUEST_PHYSICAL_ACCESS_DURING_EVENT_DELIVERY:usize=10;
	pub const GUEST_PHYSICAL_ACCESS_FOR_MONITORING:usize=11;
	pub const GUEST_PHYSICAL_EXECUTE:usize=15;
}

#[bitfield(u32)] pub struct EptViolationQualification
{
	pub is_read:bool,
	pub is_write:bool,
	pub is_execute:bool,
	pub is_readable:bool,
	pub is_writable:bool,
	pub is_executable:bool,
	pub is_user_executable:bool,
	pub linear_address_valid:bool,
	pub caused_by_gpa_to_hpa:bool,
	pub linear_address_is_user:bool,
	pub linear_address_is_writable:bool,
	pub linear_address_is_no_execute:bool,
	pub nmi_unblocking_due_to_iret:bool,
	pub is_shadow_stack:bool,
	#[bits(18)] rsvd:u32
}

impl EptViolationQualification
{
	derive_qualification_reader!();
}

pub struct ActivityState(pub u32);
impl ActivityState
{
	pub const ACTIVE:u32=0;
	pub const HLT:u32=1;
	pub const SHUTDOWN:u32=2;
	pub const WAIT_FOR_SIPI:u32=3;
}