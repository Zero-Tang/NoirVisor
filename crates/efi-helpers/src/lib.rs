// NoirVisor UEFI Helper Library

#![no_std]

use core::{alloc::{GlobalAlloc, Layout}, ffi::c_void, fmt::{self, Write}, mem::MaybeUninit, ptr::{NonNull, null_mut}, slice, sync::atomic::{AtomicPtr, Ordering}};

use alloc::{alloc::{AllocError, Allocator, Global}, format, string::String, vec::Vec};
use r_efi::{efi::{ALLOCATE_ANY_PAGES, BOOT_SERVICES_DATA, BY_PROTOCOL, Boolean, BootServices, Guid, Handle, RuntimeServices, Status, SystemTable}, protocols::{device_path, device_path_to_text, device_path_utilities, loaded_image, simple_text_input::{self, InputKey}, simple_text_output}};

extern crate alloc;

pub mod ata_passthru;
#[allow(non_camel_case_types,non_snake_case)]
pub mod pe;

pub static STDIN_PROTOCOL:AtomicPtr<simple_text_input::Protocol>=AtomicPtr::new(null_mut());
pub static STDOUT_PROTOCOL:AtomicPtr<simple_text_output::Protocol>=AtomicPtr::new(null_mut());
pub static BS_TABLE:AtomicPtr<BootServices>=AtomicPtr::new(null_mut());
pub static RT_TABLE:AtomicPtr<RuntimeServices>=AtomicPtr::new(null_mut());
pub static ST_TABLE:AtomicPtr<SystemTable>=AtomicPtr::new(null_mut());
pub static DEVICE_PATH_UTILITIES:AtomicPtr<device_path_utilities::Protocol>=AtomicPtr::new(null_mut());
pub static DEVICE_PATH_TO_TEXT:AtomicPtr<device_path_to_text::Protocol>=AtomicPtr::new(null_mut());
pub static IMAGE_INFO:AtomicPtr<loaded_image::Protocol>=AtomicPtr::new(null_mut());

pub struct EfiAllocator(pub u32);

impl EfiAllocator
{
	const DEFAULT_ALIGNMENT:usize=8;

	fn require_realign(layout:Layout)->bool
	{
		layout.align()>Self::DEFAULT_ALIGNMENT
	}

	fn realign(ptr:*mut u8,align:usize)->*mut *mut u8
	{
		let align_mask=!(align-1);
		let mut q=ptr as usize;
		q&=align_mask;
		q+=align;
		q as *mut *mut u8
	}

	/// ## Safety
	/// This function allocates memory not managed by RAII. You must `call_free` on your own!
	pub unsafe fn call_alloc(&self,size:usize)->*mut u8
	{
		let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
		let mut p:MaybeUninit<*mut c_void>=MaybeUninit::uninit();
		let st=unsafe{(bs.allocate_pool)(self.0,size,p.as_mut_ptr())};
		match st
		{
			Status::SUCCESS=>unsafe{p.assume_init().cast()}
			_=>null_mut()
		}
	}

	/// ## Safety
	/// This function should only be used for memories not managed by RAII!
	pub unsafe fn call_free(&self,ptr:*mut u8)
	{
		unsafe
		{
			let bs=&*BS_TABLE.load(Ordering::Relaxed);
			(bs.free_pool)(ptr.cast());
		}
	}
}

unsafe impl GlobalAlloc for EfiAllocator
{
	unsafe fn alloc(&self, layout: Layout) -> *mut u8
	{
		if Self::require_realign(layout)
		{
			// Realignment is required. Allocate an extra alignment block to store the original pointer.
			let new_size=layout.size()+layout.align();
			let p=unsafe{self.call_alloc(new_size)};
			if p.is_null()
			{
				null_mut()
			}
			else
			{
				// Obtain an aligned pointer.
				let q=Self::realign(p,layout.align());
				unsafe
				{
					// Store the original pointer.
					q.sub(1).write(p);
				}
				q.cast()
			}
		}
		else
		{
			unsafe
			{
				self.call_alloc(layout.size())
			}
		}
	}

	unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout)
	{
		if Self::require_realign(layout)
		{
			// The `ptr` is a realigned pointer.
			// Resolve the original pointer before freeing it.
			let q=ptr as *mut *mut u8;
			unsafe
			{
				let p=q.sub(1).read();
				self.call_free(p);
			}
		}
		else
		{
			unsafe
			{
				self.call_free(ptr);
			}
		}
	}
}

unsafe impl Allocator for EfiAllocator
{
	fn allocate(&self,layout:Layout)->Result<NonNull<[u8]>,AllocError>
	{
		let x=unsafe{self.alloc(layout)};
		if x.is_null()
		{
			Err(AllocError)
		}
		else
		{
			unsafe
			{
				Ok(NonNull::new_unchecked(slice::from_raw_parts_mut(x,layout.size())))
			}
		}
	}

	unsafe fn deallocate(&self,ptr:NonNull<u8>,layout:Layout)
	{
		unsafe
		{
			self.dealloc(ptr.addr().get() as *mut u8,layout);
		}
	}
}

/// ## Safety
/// This function allocates memory not managed by RAII. You must `call_free` on your own!
pub unsafe fn allocate_pages(mem_type:u32,pages:usize)->Result<u64,Status>
{
	let mut p:MaybeUninit<u64>=MaybeUninit::uninit();
	let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
	let st=unsafe{(bs.allocate_pages)(ALLOCATE_ANY_PAGES,mem_type,pages,p.as_mut_ptr())};
	if st.is_error()
	{
		Err(st)
	}
	else
	{
		Ok(unsafe{p.assume_init()})
	}
}

/// ## Safety
/// This function should only be used on memories allocated from `allocate_pages`!
pub unsafe fn free_pages(ptr:u64,size:usize)
{
	unsafe
	{
		let bs=&*BS_TABLE.load(Ordering::Relaxed);
		(bs.free_pages)(ptr,size);
	}
}

pub struct CStr16(*const u16);

impl CStr16
{
	/// ## Safety
	/// You must ensure `p` points to a valid null-terminated UTF-16 string. \
	/// May panic if not aligned.
	pub unsafe fn from_ptr(p:*const u16)->Self
	{
		Self(p)
	}
}

impl fmt::Display for CStr16
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		for r in char::decode_utf16(CStr16Iter(self.0))
		{
			f.write_char(r.unwrap_or(char::REPLACEMENT_CHARACTER))?;
		}
		Ok(())
	}
}

pub struct CStr16Iter(*const u16);

impl Iterator for CStr16Iter
{
	type Item = u16;
	fn next(&mut self) -> Option<Self::Item>
	{
		unsafe
		{
			let c=self.0.read();
			if c==0
			{
				None
			}
			else
			{
				self.0=self.0.add(1);
				Some(c)
			}
		}
	}
}

pub struct WString<A:Allocator=Global>(Vec<u16,A>);

impl WString<Global>
{
	pub fn with_capacity(capacity:usize)->Self
	{
		Self(Vec::with_capacity(capacity))
	}
}

impl<A:Allocator> WString<A>
{
	pub fn with_capacity_in(capacity:usize,allocator:A)->Self
	{
		Self(Vec::with_capacity_in(capacity,allocator))
	}
}

impl<A:Allocator> fmt::Write for WString<A>
{
	fn write_str(&mut self, s: &str) -> fmt::Result
	{
		for c in s.encode_utf16()
		{
			if c==b'\n' as u16
			{
				self.0.push(b'\r' as u16);
			}
			self.0.push(c);
		}
		Ok(())
	}
}

pub fn set_console_color(color:usize)
{
	let stdout=unsafe{&mut *STDOUT_PROTOCOL.load(Ordering::Relaxed)};
	unsafe
	{
		(stdout.set_attribute)(stdout,color);
	}
}

pub fn internal_print(args:fmt::Arguments)
{
	// Force using alternate allocator provided by UEFI.
	let a=EfiAllocator(BOOT_SERVICES_DATA);
	let mut w=WString::with_capacity_in(768,a);
	if fmt::write(&mut w,args).is_ok()
	{
		// Make sure it's null-terminated.
		w.0.push(0);
		unsafe
		{
			let stdout=&mut *STDOUT_PROTOCOL.load(Ordering::Relaxed);
			(stdout.output_string)(stdout,w.0.as_mut_ptr());
		}
	}
}

#[macro_export]
macro_rules! print
{
	($($arg:tt)*)=>
	{
		$crate::internal_print(format_args!($($arg)*))
	};
}

#[macro_export]
macro_rules! println
{
	()=>
	{
		$crate::print!("\n")
	};
	($($arg:tt)*)=>
	{
		$crate::print!("{}\n",format_args!($($arg)*))
	}
}

pub fn block_until_keystroke()->char
{
	unsafe
	{
		let mut i:usize=0;
		let stdin=&mut *STDIN_PROTOCOL.load(Ordering::Relaxed);
		let bs=&*BS_TABLE.load(Ordering::Relaxed);
		let st=(bs.wait_for_event)(1,&raw mut stdin.wait_for_key,&raw mut i);
		assert_eq!(st,Status::SUCCESS);
		let mut key=InputKey::default();
		let st=(stdin.read_key_stroke)(stdin,&raw mut key);
		assert_eq!(st,Status::SUCCESS);
		char::decode_utf16([key.unicode_char]).next().unwrap().unwrap()
	}
}

pub fn block_until_keystroke2()->Result<char,u16>
{
	unsafe
	{
		let mut i:usize=0;
		let stdin=&mut *STDIN_PROTOCOL.load(Ordering::Relaxed);
		let bs=&*BS_TABLE.load(Ordering::Relaxed);
		let st=(bs.wait_for_event)(1,&raw mut stdin.wait_for_key,&raw mut i);
		assert_eq!(st,Status::SUCCESS);
		let mut key=InputKey::default();
		let st=(stdin.read_key_stroke)(stdin,&raw mut key);
		assert_eq!(st,Status::SUCCESS);
		if key.scan_code==0
		{
			Ok(char::decode_utf16([key.unicode_char]).next().unwrap().unwrap())
		}
		else
		{
			Err(key.scan_code)
		}
	}
}

pub fn clear_screen()
{
	unsafe
	{
		let stdout=&mut *STDOUT_PROTOCOL.load(Ordering::Relaxed);
		(stdout.clear_screen)(stdout);
	}
}

#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub fn handle_protocol<T>(handle:Handle,mut guid:Guid)->Result<*mut T,Status>
{
	let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
	let mut protocol:MaybeUninit<*mut T>=MaybeUninit::uninit();
	let st=unsafe{(bs.handle_protocol)(handle,&raw mut guid,protocol.as_mut_ptr().cast())};
	if st.is_error()
	{
		Err(st)
	}
	else
	{
		Ok(unsafe{protocol.assume_init()})
	}
}

pub fn locate_protocol<T>(mut guid:Guid)->Result<*mut T,Status>
{
	let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
	let mut protocol:MaybeUninit<*mut T>=MaybeUninit::uninit();
	let st=unsafe{(bs.locate_protocol)(&raw mut guid,null_mut(),protocol.as_mut_ptr().cast())};
	if st.is_error()
	{
		Err(st)
	}
	else
	{
		Ok(unsafe{protocol.assume_init()})
	}
}

#[derive(Debug)]
pub struct ProtocolBuffer<T:Sized>(Vec<*mut T>);

impl<T:Sized> ProtocolBuffer<T>
{
	pub fn locate_by_protocol(mut protocol:Guid)->Result<Self,Status>
	{
		let mut size=0;
		let mut p=null_mut();
		let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
		let mut st=unsafe{(bs.locate_handle_buffer)(BY_PROTOCOL,&raw mut protocol,null_mut(),&raw mut size,&raw mut p)};
		if st!=Status::SUCCESS
		{
			return Err(st);
		}
		let mut v=Vec::with_capacity(size);
		let handle_slice:&[Handle]=unsafe{slice::from_raw_parts(p,size)};
		for &h in handle_slice
		{
			let mut interface=null_mut();
			st=unsafe{(bs.handle_protocol)(h,&raw mut protocol,&raw mut interface)};
			if st!=Status::SUCCESS
			{
				break;
			}
			v.push(interface.cast());
		}
		unsafe
		{
			(bs.free_pool)(p.cast());
		}
		if st!=Status::SUCCESS
		{
			Err(st)
		}
		else
		{
			Ok(Self(v))
		}
	}

	pub fn as_slice(&self)->&[*mut T]
	{
		&self.0
	}
}

/// ## Safety
/// You must ensure `path` points to a valid device path instance!
pub unsafe fn convert_device_path_to_text(path:*mut device_path::Protocol)->String
{
	unsafe
	{
		let dp2text=&*DEVICE_PATH_TO_TEXT.load(Ordering::Relaxed);
		let p=(dp2text.convert_device_path_to_text)(path,Boolean::FALSE,Boolean::FALSE);
		let cs=CStr16::from_ptr(p);
		let s=format!("{cs}");
		let bs=&*BS_TABLE.load(Ordering::Relaxed);
		(bs.free_pool)(p.cast());
		s
	}
}

/// ## Safety
/// You must call this function at the beginning of entry point!
pub unsafe fn init(image_handle:Handle,system_table:*mut SystemTable)
{
	unsafe
	{
		// Standard I/O
		STDIN_PROTOCOL.store((*system_table).con_in,Ordering::Relaxed);
		STDOUT_PROTOCOL.store((*system_table).con_out,Ordering::Relaxed);
		// System Services
		ST_TABLE.store(system_table,Ordering::Relaxed);
		BS_TABLE.store((*system_table).boot_services,Ordering::Relaxed);
		RT_TABLE.store((*system_table).runtime_services,Ordering::Relaxed);
		// Loaded Image Protocol. Useful to get self-image.
		IMAGE_INFO.store(handle_protocol(image_handle,loaded_image::PROTOCOL_GUID).unwrap(),Ordering::Relaxed);
		// Device-Path Utilities.
		DEVICE_PATH_UTILITIES.store(locate_protocol(device_path_utilities::PROTOCOL_GUID).unwrap(),Ordering::Relaxed);
		DEVICE_PATH_TO_TEXT.store(locate_protocol(device_path_to_text::PROTOCOL_GUID).unwrap(),Ordering::Relaxed);
	}
}