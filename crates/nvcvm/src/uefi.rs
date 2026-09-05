// NoirVisor CVM definitions for UEFI.

use core::{ffi::c_void, mem::MaybeUninit, ptr::null_mut, sync::atomic::{AtomicPtr, Ordering}};

use r_efi::efi::{Guid, Status as EfiStatus, SystemTable};

use crate::status::Status;

#[repr(C)] pub struct NoirVisorCvmSchedulingProtocol
{
	pub revision:u32,
	/// The routine that emulate IOCTL for CVM Scheduler routines.
	pub ioctl:unsafe extern "efiapi" fn (ioctl_code:usize,in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
}

impl NoirVisorCvmSchedulingProtocol
{
	// GUID: {850D29B7-33DF-4FDD-B30D-19519F042440}
	pub const GUID:Guid=Guid::from_fields(0x850d29b7,0x33df,0x4fdd,0xb3,0x0d,&[0x19,0x51,0x9f,0x4,0x24,0x40]);
}

#[cfg(feature="user")] static CVM_PROTOCOL:AtomicPtr<NoirVisorCvmSchedulingProtocol>=AtomicPtr::new(null_mut());

/// Emulates the IOCTL API exposed in regular OS.
/// 
/// ## Safety
/// You must ensure that `in_buff` and `out_buff` are valid pointers.
#[cfg(feature="user")] pub unsafe fn do_ioctl<I:Sized,O:Sized>(code:usize,in_buff:*const I,out_buff:*mut O)->Status
{
	unsafe
	{
		let protocol=&*CVM_PROTOCOL.load(Ordering::Relaxed);
		(protocol.ioctl)(code,in_buff.cast(),size_of::<I>(),out_buff.cast(),size_of::<O>())
	}
}

/// Initializes the CVM crate.
/// 
/// ## Safety
/// You must ensure `system_table` points to a valid table.
#[cfg(feature="user")] pub unsafe fn init(system_table:*const SystemTable)->EfiStatus
{
	let mut guid=NoirVisorCvmSchedulingProtocol::GUID;
	let mut protocol:MaybeUninit<*mut c_void>=MaybeUninit::uninit();
	unsafe
	{
		let bs=&*(*system_table).boot_services;
		let st=(bs.locate_protocol)(&raw mut guid,null_mut(),protocol.assume_init_mut());
		if st==EfiStatus::SUCCESS
		{
			CVM_PROTOCOL.store(protocol.assume_init().cast(),Ordering::Relaxed);
		}
		st
	}
}