/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the UEFI Runtime Driver for NoirVisor in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![no_std]
#![no_main]

use r_efi::efi::{Handle, Status, SystemTable};

use host::{efi_init,block_until_keystroke};
use cfgmgr::ConfigurationList;
use uefihvm::{init_internal_debugger,init_disasm,init_logger,init_ci,test_ci,build_hypervisor,register_exit_boot_services_event,suppress_image_relocation};

pub mod host;
#[allow(non_camel_case_types,non_snake_case)]
pub mod pe;
mod cfgmgr;
mod uefihvm;

unsafe extern "C"
{
	fn nvc_acpi_initialize()->u32;
	fn nvc_hpet_initialize()->u32;
	fn noir_get_virtualization_supportability()->u32;
	fn noir_is_virtualization_enabled()->bool;
}

#[unsafe(no_mangle)] extern "efiapi" fn NoirDriverEntry(image_handle:Handle,system_table:*mut SystemTable)->Status
{
	unsafe
	{
		efi_init(image_handle,system_table);
	}
	println!("Welcome to NoirVisor UEFI Runtime Driver!");
	ConfigurationList::init();
	suppress_image_relocation();
	init_internal_debugger();
	init_logger();
	init_disasm();
	unsafe
	{
		let sup=noir_get_virtualization_supportability();
		if (sup&3)!=3
		{
			println!("Required features of Hardware-Accelerated Virtualization is unsupported!");
			return Status::UNSUPPORTED;
		}
		if !noir_is_virtualization_enabled()
		{
			println!("Hardware-Accelerated Virtualization is disabled! Check your firmware settings!");
			return Status::UNSUPPORTED;
		}
	}
	println!("Press Enter Key to continue subversion!");
	block_until_keystroke(b'\r' as u16);
	register_exit_boot_services_event();
	init_ci();
	unsafe
	{
		nvc_acpi_initialize();
		nvc_hpet_initialize();
	}
	println!("Subverting the system...");
	build_hypervisor();
	test_ci();
	Status::SUCCESS
}

// Panic handler is required for no_std crates. But it is NOT FOR tests!
// Put it under non-test conditional-compilation, or otherwise
// the rust-analyzer of VSCode will report duplicate panic_impl.

#[cfg(not(test))]
mod panicking
{
	use crate::{host::set_console_color, println};
	use core::panic::PanicInfo;

	#[panic_handler] fn panic(panic: &PanicInfo)->!
	{
		set_console_color(0xC);		// Toggle to red foreground.
		println!("[PANIC] NoirVisor UEFI-RT-Driver {}",panic);
		loop{}
	}
}