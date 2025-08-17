/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file implements try-tasks of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, mem::offset_of};

use crate::xpf_core::{asm::msr::rdmsr, hv_host::x86::{PerCpuGsException,PerCpuGsState}, x86::msr::MSR_GS_BASE};

#[derive(Debug)]
pub struct FailResult
{
	/// The vector of exception that happened.
	pub vector:u8,
	pub error_code:Option<u32>
}

impl FailResult
{
	const fn new(vector:u8,error_code:Option<u32>)->Self
	{
		Self
		{
			vector,
			error_code
		}
	}
}

pub const HANDLER_RSP_OFFSET:usize=offset_of!(PerCpuGsException,handler_rsp);
pub const HANDLER_RIP_OFFSET:usize=offset_of!(PerCpuGsException,handler_rip);

unsafe extern "C"
{
	fn nvc_call_try_task(procedure:extern "C" fn(*mut c_void),context:*mut c_void)->u32;
}

/// ## `try_task` function
/// Any exceptions happened within the `procedure` function will be captured and be returned as Err(FailResult).
/// 
/// The `context` argument will be directly passed to `procedure` function.
/// 
/// ## Safety
/// You should guarantee `context` is valid so that `procedure` receives correct context.
pub unsafe fn try_task(procedure:extern "C" fn(*mut c_void),context:*mut c_void)->Result<(),FailResult>
{
	let gs_ctxt=unsafe{&mut *(rdmsr(MSR_GS_BASE) as *mut PerCpuGsException)};
	gs_ctxt.reset();
	unsafe
	{
		nvc_call_try_task(procedure,context);
	}
	use PerCpuGsState::*;
	match gs_ctxt.state
	{
		AwaitExecution=>
		{
			gs_ctxt.state=Successful;
			Ok(())
		}
		Successful=>panic!("GS-Context was not properly reset!"),
		Failed{vector,error_code}=>Err(FailResult::new(vector,error_code))
	}
}