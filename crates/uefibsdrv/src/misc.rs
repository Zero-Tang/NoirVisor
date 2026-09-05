// NoirVisor CVM Scheduler as UEFI Boot-Service Driver

use core::{ffi::c_void, fmt, mem::MaybeUninit, ptr::null_mut, sync::atomic::{AtomicPtr, Ordering}};

use alloc::{alloc::{GlobalAlloc, Layout}, vec::Vec};

use r_efi::{efi::{BOOT_SERVICES_DATA, BootServices, Guid, Handle, RuntimeServices, Status, SystemTable}, protocols::{loaded_image, simple_text_input, simple_text_output}};
use log::*;

struct EfiAllocator;

impl EfiAllocator
{
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

	unsafe fn call_alloc(size:usize)->*mut u8
	{
		unsafe
		{
			let bs=&*BS_TABLE.load(Ordering::Relaxed);
			let mut p:MaybeUninit<*mut c_void>=MaybeUninit::uninit();
			let st=(bs.allocate_pool)(BOOT_SERVICES_DATA,size,p.as_mut_ptr());
			match st
			{
				Status::SUCCESS=>p.assume_init().cast(),
				_=>null_mut()
			}
		}
	}

	unsafe fn call_free(ptr:*mut u8)
	{
		unsafe
		{
			let bs=&*BS_TABLE.load(Ordering::Relaxed);
			(bs.free_pool)(ptr.cast());
		}
	}

	const DEFAULT_ALIGNMENT:usize=size_of::<usize>();
}

unsafe impl GlobalAlloc for EfiAllocator
{
	unsafe fn alloc(&self, layout: Layout) -> *mut u8
	{
		if Self::require_realign(layout)
		{
			// Realignment is required. Allocate an extra alignment block to store the original pointer.
			let new_size=layout.size()+layout.align();
			let p=unsafe{Self::call_alloc(new_size)};
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
				Self::call_alloc(layout.size())
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
				Self::call_free(p);
			}
		}
		else
		{
			unsafe
			{
				Self::call_free(ptr);
			}
		}
	}
}

#[global_allocator] static EFI_ALLOCATOR:EfiAllocator=EfiAllocator;

static STDIN_PROTOCOL:AtomicPtr<simple_text_input::Protocol>=AtomicPtr::new(null_mut());
static STDOUT_PROTOCOL:AtomicPtr<simple_text_output::Protocol>=AtomicPtr::new(null_mut());
pub static BS_TABLE:AtomicPtr<BootServices>=AtomicPtr::new(null_mut());
pub static RT_TABLE:AtomicPtr<RuntimeServices>=AtomicPtr::new(null_mut());
pub static ST_TABLE:AtomicPtr<SystemTable>=AtomicPtr::new(null_mut());
pub static IMAGE_INFO:AtomicPtr<loaded_image::Protocol>=AtomicPtr::new(null_mut());

pub unsafe fn handle_protocol<T>(handle:Handle,mut guid:Guid)->Result<*mut T,Status>
{
	let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
	let mut protocol:*mut T=null_mut();
	let st=unsafe{(bs.handle_protocol)(handle,&raw mut guid,(&raw mut protocol).cast())};
	if st.is_error()
	{
		Err(st)
	}
	else
	{
		Ok(protocol)
	}
}

/// ## Safety
/// You must call this function at the beginning of entry point!
pub unsafe fn efi_init(image_handle:Handle,system_table:*mut SystemTable)
{
	unsafe
	{
		// Standard I/O
		STDIN_PROTOCOL.store((*system_table).con_in,Ordering::Relaxed);
		STDOUT_PROTOCOL.store((*system_table).con_out,Ordering::Relaxed);
		// Logger
		let _=set_logger(&KERNEL_LOGGER);
		set_max_level(LevelFilter::Trace);
		// System Services
		ST_TABLE.store(system_table,Ordering::Relaxed);
		BS_TABLE.store((*system_table).boot_services,Ordering::Relaxed);
		RT_TABLE.store((*system_table).runtime_services,Ordering::Relaxed);
		// Loaded Image Protocol. Useful to get self-image.
		IMAGE_INFO.store(handle_protocol(image_handle,loaded_image::PROTOCOL_GUID).unwrap(),Ordering::Relaxed);
	}
}

struct WString(Vec<u16>);

impl WString
{
	fn with_capacity(capacity:usize)->Self
	{
		Self(Vec::with_capacity(capacity))
	}
}

impl fmt::Write for WString
{
	fn write_str(&mut self,s:&str)->fmt::Result
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

pub fn debug_print(args:fmt::Arguments)
{
	let mut w=WString::with_capacity(256);
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
macro_rules! dprint
{
	($($arg:tt)*)=>
	{
		$crate::misc::debug_print(format_args!($($arg)*))
	};
}

#[macro_export]
macro_rules! dprintln
{
	()=>
	{
		$crate::dprint!("\n")
	};
	($($arg:tt)*)=>
	{
		$crate::dprint!("{}\n",format_args!($($arg)*))
	}
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
		dprintln!("|{level_char}| {}",record.args());
	}
}

static KERNEL_LOGGER:EfiLogger=EfiLogger;