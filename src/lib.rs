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

extern crate alloc;

pub mod drv_core;
pub mod xpf_core;
pub mod vt_core;

use xpf_core::nvstatus::*;
use xpf_core::debug::*;

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

// This global variable is required to export to driver framework written in C.
// Public core variables in NoirVisor are required to be named in lower-snake-case.
#[allow(non_upper_case_globals)]
#[no_mangle] pub static mut system_cr3:u64=0;

#[no_mangle] pub extern "C" fn noir_get_virtualization_supportability()->u32
{
	3
}

#[no_mangle] pub extern "C" fn noir_is_virtualization_enabled()->bool
{
	true
}

#[no_mangle] pub extern "C" fn nvc_teardown_hypervisor()->()
{

}

#[no_mangle] pub extern "C" fn nvc_build_hypervisor()->Status
{
	println!("Subverting the system...");
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