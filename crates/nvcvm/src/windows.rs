// NoirVisor CVM definitions for Windows.

#[cfg(feature="user")]
use utf16_lit::*;
use windows_sys::Win32::Foundation::*;

#[cfg(feature="user")]
use core::{mem::MaybeUninit, ptr::{null, null_mut}};

#[cfg(feature="user")]
use windows_sys::{Wdk::{Foundation::OBJECT_ATTRIBUTES, Storage::FileSystem::{FILE_NON_DIRECTORY_FILE, FILE_SYNCHRONOUS_IO_NONALERT}, System::SystemServices::{ZwClose, ZwDeviceIoControlFile, ZwOpenFile}}, Win32::{Storage::FileSystem::{FILE_SHARE_READ, FILE_SHARE_WRITE, SYNCHRONIZE}, System::{IO::IO_STATUS_BLOCK, Ioctl::{FILE_ANY_ACCESS, FILE_DEVICE_UNKNOWN, METHOD_BUFFERED}}}};

use crate::status::Status;

#[cfg(feature="user")] static mut DEVICE_HANDLE:HANDLE=INVALID_HANDLE_VALUE;

#[cfg(feature="user")] 
#[inline(always)] const fn ctl_code(dev_type:u32,fn_code:u32,method:u32,access:u32)->u32
{
	(dev_type<<16)|(access<<14)|(fn_code<<2)|method
}

/// Calls into the NoirVisor CVM Scheduler Driver.
/// 
/// ## Safety
/// You must ensure `in_buff` and `out_buff` are valid pointers.
#[cfg(feature="user")] pub unsafe fn do_ioctl<I:Sized,O:Sized>(code:usize,in_buff:*const I,out_buff:*mut O)->Status
{
	let mut iosb:MaybeUninit<IO_STATUS_BLOCK>=MaybeUninit::uninit();
	let io_code=ctl_code(FILE_DEVICE_UNKNOWN,(code|0x800) as u32,METHOD_BUFFERED,FILE_ANY_ACCESS);
	let st:NTSTATUS=unsafe
	{
		ZwDeviceIoControlFile(DEVICE_HANDLE,null_mut(),None,null(),iosb.as_mut_ptr(),io_code,in_buff.cast(),size_of::<I>() as u32,out_buff.cast(),size_of::<O>() as u32)
	};
	Status::from(st)
}

impl From<NTSTATUS> for Status
{
	fn from(value:NTSTATUS)->Self
	{
		match value
		{
			STATUS_SUCCESS=>Self::SUCCESS,
			STATUS_INSUFFICIENT_RESOURCES=>Self::INSUFFICIENT_RESOURCES,
			STATUS_INVALID_PARAMETER=>Self::INVALID_PARAMETER,
			STATUS_BUFFER_TOO_SMALL=>Self::BUFFER_TOO_SMALL,
			_=>Self::UNSUCCESSFUL
		}
	}
}

/// Finalizes the CVM crate.
/// 
/// ## Safety
/// The finalization is not thread-safe. You must ensure no other threads are using CVM.
#[cfg(feature="user")] pub unsafe fn deinit()
{
	unsafe
	{
		ZwClose(DEVICE_HANDLE);
		DEVICE_HANDLE=INVALID_HANDLE_VALUE;
	}
}

/// Initializes the CVM crate.
/// 
/// ## Safety
/// The initialization is not thread-safe. You must ensure no other threads are using CVM.
#[cfg(feature="user")] pub unsafe fn init()->NTSTATUS
{
	const DEVICE_NAME:[u16;17]=utf16!("\\Device\\NoirVisor");
	// Open the device and get the handle.
	let name=UNICODE_STRING
	{
		Length:(DEVICE_NAME.len()<<1) as u16,
		MaximumLength:(DEVICE_NAME.len()<<1) as u16,
		Buffer:DEVICE_NAME.as_ptr() as *mut u16
	};
	let oa=OBJECT_ATTRIBUTES
	{
		Length:size_of::<OBJECT_ATTRIBUTES>() as u32,
		RootDirectory:null_mut(),
		ObjectName:&raw const name,
		Attributes:OBJ_FORCE_ACCESS_CHECK,
		SecurityDescriptor:null_mut(),
		SecurityQualityOfService:null_mut()
	};
	let mut iosb=IO_STATUS_BLOCK::default();
	unsafe
	{
		ZwOpenFile(&raw mut DEVICE_HANDLE,GENERIC_READ|SYNCHRONIZE,&raw const oa,&raw mut iosb,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE)
	}
}