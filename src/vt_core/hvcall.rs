/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file handles Hypercalls in Intel VT-x of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

use log::{info, warn};
use nvcvm::status::Status;

use crate::xpf_core::{x86::{interrupts::INVALID_OPCODE_FAULT,descriptors::*,msr::*},asm::{vt::*,crdr::*,seg::*,msr::wrmsr},nvbdk::GprState};

use super::{VtVcpu,vmcs::*,nvc_vt_resume_without_entry};

impl VtVcpu
{
	fn hvcall_unknown(&mut self,code:u32,context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		warn!("Unknown Hypercall Code 0x{code:X} is called! Context={context:p}");
		Err((INVALID_OPCODE_FAULT,None))
	}

	fn hvcall_restore(&mut self,_code:u32,_context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		let nrip=self.cached_ctxt.rip+vmread32(VMEXIT_INSTRUCTION_LENGTH).unwrap() as u64;
		let gflags=vmreadptr(GUEST_RFLAGS).unwrap();
		let gcr3=vmreadptr(GUEST_CR3).unwrap() as u64;
		let gpr_state=&mut self.get_stack_top_mut().gpr_state;
		let saved_state:GprState=GprState
		{
			rax:nrip,
			rcx:gflags as u64,
			rdx:gpr_state.rsp,
			rbx:gpr_state.rbx,
			rsp:gpr_state.rsp,
			rbp:gpr_state.rbp,
			rsi:gpr_state.rsi,
			rdi:gpr_state.rdi,
			r8:gpr_state.r8,
			r9:gpr_state.r9,
			r10:gpr_state.r10,
			r11:gpr_state.r11,
			r12:gpr_state.r12,
			r13:gpr_state.r13,
			r14:gpr_state.r14,
			r15:gpr_state.r15,
		};
		// Switch to Restored Control Registers.
		let gcr4=vmreadptr(GUEST_CR4).unwrap() as u64;
		write_cr3(gcr3);
		write_cr4(gcr4);
		unsafe
		{
			// Switch to Restored IDT.
			let gidtr=DescriptorTable
			{
				limit:vmread32(GUEST_IDTR_LIMIT).unwrap() as u16,
				base:vmreadptr(GUEST_IDTR_BASE).unwrap() as u64
			};
			write_idtr(&raw const gidtr);
			// Switch to Restored GDT.
			let ggdtr=DescriptorTable
			{
				limit:vmread32(GUEST_GDTR_LIMIT).unwrap() as u16,
				base:vmreadptr(GUEST_GDTR_BASE).unwrap() as u64
			};
			write_gdtr(&raw const ggdtr);
			// Switch to Restored TSS.
			let tr_sel=vmread32(GUEST_TR_SELECTOR).unwrap() as u16;
			// Before actually switching TSS, make it available.
			let tss_entry=(ggdtr.base+(tr_sel as u64 & 0xFFF8)) as *mut SystemSegmentDescriptor;
			(*tss_entry).flags=SegmentFlags::AVAILABLE_TSS;
			((ggdtr.base+tr_sel as u64+0x5) as *mut u8).write(0x89);
			// Switch FS/GS Bases
			wrmsr(MSR_FS_BASE,vmreadptr(GUEST_FS_BASE).unwrap() as u64);
			wrmsr(MSR_GS_BASE,vmreadptr(GUEST_GS_BASE).unwrap() as u64);
			// Switch it.
			write_tr(tr_sel);
		}
		// Return to the caller in Host Mode.
		unsafe
		{
			nvc_vt_resume_without_entry(&raw const saved_state);
		}
		// Never reaches here!
	}

	fn hvcall_alloc_tlb_tag(&mut self,_code:u32,_context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		Ok(Status::NOT_IMPLEMENTED)
	}

	fn hvcall_exit_boot_services(&mut self,_code:u32,_context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		info!("ExitBootServices event is triggered! UEFI now enters Runtime Stage!");
		Ok(Status::SUCCESS)
	}
}

pub(super) type VtHypercallHandler=fn(&mut VtVcpu,code:u32,context:*mut c_void)->Result<Status,(u8,Option<u32>)>;

static VT_HYPERCALL_HANDLER_BASE:[VtHypercallHandler;4]=
[
	VtVcpu::hvcall_unknown,
	VtVcpu::hvcall_restore,
	VtVcpu::hvcall_alloc_tlb_tag,
	VtVcpu::hvcall_exit_boot_services
];

static VT_HYPERCALL_HANDLER_CVM:[VtHypercallHandler;6]=
[
	
	VtVcpu::hvcall_unknown,
	VtVcpu::hvcall_unknown,
	VtVcpu::hvcall_unknown,
	VtVcpu::hvcall_unknown,
	VtVcpu::hvcall_unknown,
	VtVcpu::hvcall_unknown
];

static VT_HYPERCALL_HANDLER_GROUPS:[&[VtHypercallHandler];2]=[&VT_HYPERCALL_HANDLER_BASE,&VT_HYPERCALL_HANDLER_CVM];

#[inline] pub(super) fn dispatch_hypercall(code:u32)->VtHypercallHandler
{
	let group=(code>>16) as usize;
	match VT_HYPERCALL_HANDLER_GROUPS.get(group)
	{
		Some(&g)=>
		{
			let index=(code&0xffff) as usize;
			match g.get(index)
			{
				Some(&h)=>h,
				None=>VtVcpu::hvcall_unknown
			}
		}
		None=>VtVcpu::hvcall_unknown
	}
}