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

use crate::xpf_core::x86::interrupts::INVALID_OPCODE_FAULT;

use super::VtVcpu;

impl VtVcpu
{
	fn hvcall_unknown(&mut self,code:u32,context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		warn!("Unknown Hypercall Code 0x{code:X} is called! Context={context:p}");
		Err((INVALID_OPCODE_FAULT,None))
	}

	fn hvcall_restore(&mut self,_code:u32,_context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		// Since NoirVisor is strictly Type-I Hypervisor, this hypercall code is reserved now.
		Err((INVALID_OPCODE_FAULT,None))
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