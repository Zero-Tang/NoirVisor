/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the entry point of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![no_std]

mod xpf_core;
mod vt_core;

use xpf_core::nvstatus::*;

struct Hypervisor
{
	cpu_count:u32,
}

pub trait VirtualCpu
{
	fn init(self);
}

impl Hypervisor
{
	const fn default() -> Self
	{
		Hypervisor
		{
			cpu_count:0,
		}
	}
}

static mut HVM:Hypervisor=Hypervisor::default();

#[no_mangle] extern "C" fn nvc_teardown_hypervisor()->()
{

}

#[no_mangle] extern "C" fn nvc_build_hypervisor()->Status
{
	NOIR_SUCCESS
}

// Panic handler is required for no_std crates. But it is NOT FOR tests!
// Put it under non-test conditional-compilation, or otherwise
// the rust-analyzer of VSCode will report duplicate panic_impl.

#[cfg(not(test))]
use core::panic::PanicInfo;

#[cfg(not(test))]
#[panic_handler] fn panic(_panic: &PanicInfo)->!
{
	loop{}
}