// NoirVisor CVM Scheduler as UEFI Boot-Service Driver

use efi_helpers::{EfiAllocator, println};
use r_efi::efi::{BOOT_SERVICES_DATA, Handle, SystemTable};
use log::*;

#[global_allocator] static EFI_ALLOCATOR:EfiAllocator=EfiAllocator(BOOT_SERVICES_DATA);

/// You must call this function at the beginning of entry point!
pub fn efi_init(image_handle:Handle,system_table:*mut SystemTable)
{
	unsafe
	{
		efi_helpers::init(image_handle,system_table);
	}
	// Logger
	let _=set_logger(&KERNEL_LOGGER);
	set_max_level(LevelFilter::Trace);
}

struct EfiLogger;

impl Log for EfiLogger
{
	fn enabled(&self, _metadata: &Metadata) -> bool
	{
		true
	}

	fn flush(&self)
	{
		
	}

	fn log(&self, record: &Record)
	{
		let level_char=match record.level()
		{
			Level::Debug=>'D',
			Level::Error=>'E',
			Level::Info=>'I',
			Level::Trace=>'T',
			Level::Warn=>'W'
		};
		println!("|{level_char}| {}",record.args());
	}
}

static KERNEL_LOGGER:EfiLogger=EfiLogger;