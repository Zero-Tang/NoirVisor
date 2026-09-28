/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file implements miscellaneous services for the UEFI boot-service driver.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use efi_helpers::{EfiAllocator, println};
use log::*;
use r_efi::efi::{BOOT_SERVICES_DATA, Handle, SystemTable};

#[global_allocator]
static EFI_ALLOCATOR: EfiAllocator = EfiAllocator(BOOT_SERVICES_DATA);

/// You must call this function at the beginning of entry point!
pub fn efi_init(image_handle: Handle, system_table: *mut SystemTable)
{
	unsafe {
		efi_helpers::init(image_handle, system_table);
	}
	// Logger
	let _ = set_logger(&KERNEL_LOGGER);
	set_max_level(LevelFilter::Trace);
}

struct EfiLogger;

impl Log for EfiLogger
{
	fn enabled(&self, _metadata: &Metadata) -> bool
	{
		true
	}

	fn flush(&self) {}

	fn log(&self, record: &Record)
	{
		let level_char = match record.level()
		{
			Level::Debug => 'D',
			Level::Error => 'E',
			Level::Info => 'I',
			Level::Trace => 'T',
			Level::Warn => 'W',
		};
		println!("|{level_char}| {}", record.args());
	}
}

static KERNEL_LOGGER: EfiLogger = EfiLogger;
