// Miscellaneous Services

use core::{ffi::{c_int, c_void}, fmt, ptr::null_mut};
use alloc::alloc::{GlobalAlloc, Layout};

use log::*;
use static_collections::{format_static, string::StaticString};
use windows_sys::{Wdk::{Foundation::{ERESOURCE, FAST_MUTEX, NonPagedPool}, System::SystemServices::*}, Win32::Foundation::{NTSTATUS, STATUS_SUCCESS}};


#[macro_export]
macro_rules! dprint
{
	($level:expr,$($arg:tt)*)=>
	{
		$crate::misc::debug_print($level,format_args!($($arg)*))
	};
}

#[macro_export]
macro_rules! dprintln
{
	($level:expr)=>
	{
		$crate::dprint!($level,"\n")
	};
	($level:expr,$($arg:tt)*)=>
	{
		$crate::dprint!($level,"{}\n",format_args!($($arg)*))
	}
}

// If the windows-sys provides incorrect bindings, redefine them here.
#[link(name="ntoskrnl.exe",kind="raw-dylib",modifiers="+verbatim")]
unsafe extern "C"
{
	fn DbgPrintEx(ComponentId:u32,Level:u32,Format:*const i8,...);
}

#[link(name="ntoskrnl.exe",kind="raw-dylib",modifiers="+verbatim")]
unsafe extern "system"
{
	fn ExInitializeFastMutex(fastmutex:*mut FAST_MUTEX);
}

const DPFLTR_ERROR_LEVEL:u32=0;
const DPFLTR_WARNING_LEVEL:u32=1;
const DPFLTR_TRACE_LEVEL:u32=2;
const DPFLTR_INFO_LEVEL:u32=3;
const DPFLTR_IHVDRIVER_ID:u32=77;

pub fn debug_print(level:u32,args:fmt::Arguments)
{
	let mut w:StaticString<512>=StaticString::new();
	if fmt::write(&mut w,args).is_ok()
	{
		unsafe
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,level,c"%.*s".as_ptr(),w.len(),w.as_ptr());
		}
	}
}

struct KernelLogger;

impl Log for KernelLogger
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
		let level=match record.level()
		{
			Level::Debug|Level::Trace=>DPFLTR_INFO_LEVEL,
			Level::Info=>DPFLTR_TRACE_LEVEL,
			Level::Warn=>DPFLTR_WARNING_LEVEL,
			Level::Error=>DPFLTR_ERROR_LEVEL
		};
		let level_char=match record.level()
		{
			Level::Debug=>'D',
			Level::Error=>'E',
			Level::Info=>'I',
			Level::Trace=>'T',
			Level::Warn=>'W'
		};
		let mut prefix=format_static!(40,"{}:{}",record.file().unwrap_or("unknown-file"),record.line().unwrap_or(0)).unwrap();
		while prefix.len()<prefix.capacity()
		{
			let _=prefix.push(' ');
		}
		dprintln!(level,"{prefix} |{level_char}| {}",record.args());
	}
}

static KERNEL_LOGGER:KernelLogger=KernelLogger;

pub fn init_logger()
{
	let _=set_logger(&KERNEL_LOGGER);
	set_max_level(LevelFilter::Trace);
}

struct KernelAllocator;

impl KernelAllocator
{
	fn require_realign(layout:Layout)->bool
	{
		// ExAllocatePoolWithTag has its own alignment features.
		if layout.align()<=Self::DEFAULT_ALIGNMENT
		{
			// The default alignment is sufficient.
			// No need for realignment.
			false
		}
		else if layout.align()<Self::PAGE_ALIGNMENT
		{
			// If alignment is big but less than page size, then
			// realignment is needed if size is less than a page.
			layout.size()<Self::PAGE_ALIGNMENT
		}
		else
		{
			// If alignment is bigger than the page, then realignment is required.
			layout.align()>Self::PAGE_ALIGNMENT
		}
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
			ExAllocatePoolWithTag(NonPagedPool,size,Self::TAG).cast()
		}
	}

	unsafe fn call_free(ptr:*mut u8)
	{
		unsafe
		{
			ExFreePoolWithTag(ptr.cast(),Self::TAG);
		}
	}

	const TAG:u32=u32::from_le_bytes(*b"NCVM");
	const DEFAULT_ALIGNMENT:usize=size_of::<usize>()<<1;
	const PAGE_ALIGNMENT:usize=PAGE_SIZE as usize;
}

unsafe impl GlobalAlloc for KernelAllocator
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

#[global_allocator] static KERNEL_ALLOCATOR:KernelAllocator=KernelAllocator;

const ERESOURCE_LAYOUT:Layout=unsafe{Layout::from_size_align_unchecked(size_of::<ERESOURCE>(),align_of::<ERESOURCE>())};
const MUTEX_LAYOUT:Layout=unsafe{Layout::from_size_align_unchecked(size_of::<FAST_MUTEX>(),align_of::<FAST_MUTEX>())};

#[unsafe(no_mangle)] unsafe extern "system" fn noir_create_reslock()->*mut c_void
{
	let p:*mut ERESOURCE=unsafe{alloc::alloc::alloc(ERESOURCE_LAYOUT).cast()};
	if !p.is_null()
	{
		let st:NTSTATUS=unsafe{ExInitializeResourceLite(p)};
		if st!=STATUS_SUCCESS
		{
			unsafe
			{
				alloc::alloc::dealloc(p.cast(),ERESOURCE_LAYOUT);
			}
			return null_mut();
		}
	}
	p.cast()
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_delete_reslock(reslock:*mut c_void)
{
	unsafe
	{
		ExDeleteResourceLite(reslock.cast());
		alloc::alloc::dealloc(reslock.cast(),ERESOURCE_LAYOUT);
	}
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_acquire_reslock_exclusive(reslock:*mut c_void)
{
	unsafe
	{
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite(reslock.cast(),true);
	}
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_acquire_reslock_shared(reslock:*mut c_void)
{
	unsafe
	{
		KeEnterCriticalRegion();
		ExAcquireResourceSharedLite(reslock.cast(),true);
	}
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_release_reslock_exclusive(reslock:*mut c_void)
{
	unsafe
	{
		ExReleaseResourceLite(reslock.cast());
		KeLeaveCriticalRegion();
	}
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_release_reslock_shared(reslock:*mut c_void)
{
	unsafe
	{
		ExReleaseResourceLite(reslock.cast());
		KeLeaveCriticalRegion();
	}
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_create_mutex()->*mut c_void
{
	let p:*mut FAST_MUTEX=unsafe{alloc::alloc::alloc(MUTEX_LAYOUT).cast()};
	if !p.is_null()
	{
		unsafe
		{
			ExInitializeFastMutex(p);
		}
	}
	p.cast()
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_delete_mutex(mutex:*mut c_void)
{
	unsafe
	{
		alloc::alloc::dealloc(mutex.cast(),MUTEX_LAYOUT);
	}
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_acquire_mutex(mutex:*mut c_void)
{
	unsafe
	{
		ExAcquireFastMutex(mutex.cast());
	}
}

#[unsafe(no_mangle)] unsafe extern "system" fn noir_release_mutex(mutex:*mut c_void)
{
	unsafe
	{
		ExReleaseFastMutex(mutex.cast());
	}
} 

#[unsafe(no_mangle)] unsafe extern "system" fn __CxxFrameHandler3()
{

}

#[unsafe(no_mangle)] static _fltused:c_int=0;