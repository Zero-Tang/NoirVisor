// NoirVisor CVM Scheduler as UEFI Boot-Service Driver
// This crate is only intended for testing CVM functionality without complex OS environment.

#![no_std]
#![feature(allocator_api)]

extern crate alloc;

use core::{ffi::c_void, ptr::null_mut, sync::atomic::Ordering};

use nvcvm::{status::Status as NvStatus, uefi::NoirVisorCvmSchedulingProtocol};
use r_efi::efi::{Handle, NATIVE_INTERFACE, Status, SystemTable};

use misc::*;

mod misc;
pub mod kmap;
pub mod sync;

unsafe extern "system"
{
	pub fn noir_cvsched_init()->bool;
	pub fn noir_cvsched_deinit();
	fn noir_dispatch_ioctl(ioctl_code:usize,in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->NvStatus;
}

unsafe extern "efiapi" fn cvm_scheduler_ioctl(ioctl_code:usize,in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->NvStatus
{
	unsafe
	{
		noir_dispatch_ioctl(ioctl_code,in_buff,in_size,out_buff,out_size)
	}
}

pub static mut SCHEDULER_HANDLE:Handle=null_mut();
pub static mut SCHEDULER_PROTOCOL:NoirVisorCvmSchedulingProtocol=NoirVisorCvmSchedulingProtocol
{
	revision:0,
	ioctl:cvm_scheduler_ioctl
};

unsafe extern "efiapi" fn driver_unload(_image_handle:Handle)->Status
{
	unsafe
	{
		noir_cvsched_deinit();
		if !SCHEDULER_HANDLE.is_null()
		{
			// Uninstall the CVM protocol from the firmware.
			let bs=&*BS_TABLE.load(Ordering::Relaxed);
			let mut protocol_guid=NoirVisorCvmSchedulingProtocol::GUID;
			(bs.uninstall_protocol_interface)(SCHEDULER_HANDLE,&raw mut protocol_guid,(&raw mut SCHEDULER_PROTOCOL).cast())
		}
		else
		{
			Status::SUCCESS
		}
	}
}

#[unsafe(no_mangle)] extern "efiapi" fn NoirDriverEntry(image_handle:Handle,system_table:*mut SystemTable)->Status
{
	unsafe
	{
		efi_init(image_handle,system_table);
		noir_cvsched_init();
		// Register Image-Unload Handler
		let image=&mut *IMAGE_INFO.load(Ordering::Relaxed);
		image.unload=Some(driver_unload);
		log::info!("NoirVisor CVM Scheduler Driver is loaded at {:p}!",image.image_base);
		// Install the CVM protocol into the firmware.
		let bs=&*BS_TABLE.load(Ordering::Relaxed);
		let mut protocol_guid=NoirVisorCvmSchedulingProtocol::GUID;
		(bs.install_protocol_interface)(&raw mut SCHEDULER_HANDLE,&raw mut protocol_guid,NATIVE_INTERFACE,(&raw mut SCHEDULER_PROTOCOL).cast())
	}
}

#[cfg(not(test))] mod panicking
{
	use core::panic::PanicInfo;

	#[panic_handler] fn panic(info:&PanicInfo)->!
	{
		log::error!("{info}");
		loop{}
	}
}