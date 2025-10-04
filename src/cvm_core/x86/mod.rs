/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines x86 CVM of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;
use nvcvm::interface::{CvmX86VcpuStateOnExit, ExitContext, ExitContextUnion, InterceptCode, CVM_MAPPING_ASID_DEFAULT};

use crate::{xpf_core::allocator::SystemPageAllocator, *};
use xpf_core::nvbdk::*;

/// This type indicates what fields should be synchronized between VMCS or VMCB. \
/// If the scheduler doesn't have to load/store them from/to VMCS/VMCB, don't list them here.
#[bitfield(u64)] pub struct CvmVcpuCache
{
	/// Dirty field for General-Purpose Registers (`rip` and `rflags`).
	pub gpr:bool,
	/// Dirty field for Control Registers (`cr0`, `cr2`, `cr3`, `cr4`, `cr8` and `efer`).
	pub cr:bool,
	/// Dirty field for Debug Registers (`dr6` and `dr7`).
	pub dr:bool,
	/// Dirty field for `cs`, `ds`, `es` and `ss` segments.
	pub sr:bool,
	/// Dirty field for `fs` and `gs` segments and `kgs_base` MSR.
	pub fg:bool,
	/// Dirty field for `tr` and `ldtr` registers.
	pub lt:bool,
	/// Dirty field for `idtr` and `gdtr` registers.
	pub dt:bool,
	/// Dirty field for `star`, `lstar`, `cstar`, `sfmask` and `ststar` registers.
	pub sc:bool,
	/// Dirty field for `esp`, `eip` and `cs` registers for `sysenter`.
	pub se:bool,
	/// Dirty field for `br_from/to`, `ex_from/to` and `debug_ctl` registers.
	pub lb:bool,
	/// Dirty field for `ssp`, `pln_ssp`, `u/s_cet` and `isst` registers.
	pub ss:bool,
	/// Dirty field for Time-Stamp Counter (TSC).
	pub ts:bool,
	#[bits(51)] reserved:u64,
	/// Indicates whether VMCS/VMCB is already updated to the `CvmVcpu` structure. \
	/// Must be cleared after a VM-Exit. Must be set before reading/writing vCPU state.
	pub synchronized:bool
}

pub struct CvmX86Vcpu
{
	// Processor State
	pub gpr:GprState,
	pub seg:SegmentState,
	pub crs:CrState,
	pub drs:DrState,
	pub msrs:MsrState,
	pub xcrs:XcrState,
	pub rflags:u64,
	pub rip:u64,
	pub tsc_offset:u64,
	pub xsaves_state:MemoryDescriptor<1,u8,SystemPageAllocator>,
	pub current_as_id:u32,
	pub state_cache:CvmVcpuCache,
	pub exit_context:ExitContext
}

impl CvmX86Vcpu
{
	pub fn new()->Option<Self>
	{
		let Some(xsave)=MemoryDescriptor::alloc_in(SystemPageAllocator) else
		{
			error!("Failed to allocate XSAVE area!");
			return None;
		};
		Some
		(
			Self
			{
				gpr:GprState
				{
					rax:0,rcx:0,rdx:0,rbx:0,rsp:0,rbp:0,rsi:0,rdi:0,
					r8:0,r9:0,r10:0,r11:0,r12:0,r13:0,r14:0,r15:0
				},
				seg:SegmentState
				{
					es:SegmentRegister::reset_data(),
					cs:SegmentRegister::reset_code(),
					ss:SegmentRegister::reset_data(),
					ds:SegmentRegister::reset_data(),
					fs:SegmentRegister::reset_data(),
					gs:SegmentRegister::reset_data(),
					tr:SegmentRegister::reset_tss(),
					gdtr:SegmentRegister::reset_dt(),
					idtr:SegmentRegister::reset_dt(),
					ldtr:SegmentRegister::reset_ldt()
				},
				crs:CrState
				{
					cr0:0x60000010,
					cr2:0,cr3:0,cr4:0,cr8:0
				},
				drs:DrState
				{
					dr0:0,dr1:0,dr2:0,dr3:0,
					dr6:0xFFFF0FF0,dr7:0x400
				},
				msrs:MsrState
				{
					sysenter_cs:0,sysenter_esp:0,sysenter_eip:0,
					pat:0,
					efer:0,
					star:0,lstar:0,cstar:0,sfmask:0,ststar:0,gsswap:0,
					debug_ctrl:0
				},
				xcrs:XcrState{xcr0:0},
				rflags:2,
				rip:0xFFF0,
				tsc_offset:0,
				xsaves_state:xsave,
				current_as_id:CVM_MAPPING_ASID_DEFAULT,
				state_cache:CvmVcpuCache::from_bits(0),
				exit_context:ExitContext
				{
					intercept_code:InterceptCode::INVALID_STATE,
					next_rip:0,
					vp_state:CvmX86VcpuStateOnExit::from_bits(0),
					context:ExitContextUnion::default()
				}
			}
		)
	}
}