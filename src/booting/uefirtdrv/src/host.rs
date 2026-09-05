/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file wraps UEFI functionalities for NoirVisor in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, fmt, mem::MaybeUninit, ops::RangeInclusive, ptr::null_mut, slice, sync::atomic::{AtomicPtr, Ordering}};

use static_collections::{string::StaticString, vec::StaticVec};
use r_efi::{efi::*, protocols::{loaded_image, mp_services, simple_text_input::{self, InputKey}, simple_text_output}, system::SystemTable};

unsafe extern "C"
{
	fn __isa_available_init();
	fn noir_debug_output(buffer:*const u8,length:usize);
}

struct DebugOutputFormatter;

impl fmt::Write for DebugOutputFormatter
{
	fn write_str(&mut self,s:&str)->fmt::Result
	{
		unsafe
		{
			noir_debug_output(s.as_ptr(),s.len());
		}
		Ok(())
	}
}

pub fn debug_print(args:fmt::Arguments)
{
	let mut o=DebugOutputFormatter;
	let _=fmt::write(&mut o,args);
}

#[macro_export]
macro_rules! dprint
{
	($($args:tt)*) =>
	{
		$crate::host::debug_print(format_args!($($args)*))
	};
}

#[macro_export]
macro_rules! dprintln
{
	() =>
	{
		dprint!("\n")
	};
	($($args:tt)*) =>
	{
		dprint!("{}\n",format_args!($($args)*))
	};
}

static STDIN_PROTOCOL:AtomicPtr<simple_text_input::Protocol>=AtomicPtr::new(null_mut());
static STDOUT_PROTOCOL:AtomicPtr<simple_text_output::Protocol>=AtomicPtr::new(null_mut());
static MP_PROTOCOL:AtomicPtr<mp_services::Protocol>=AtomicPtr::new(null_mut());
pub static BS_TABLE:AtomicPtr<BootServices>=AtomicPtr::new(null_mut());
pub static RT_TABLE:AtomicPtr<RuntimeServices>=AtomicPtr::new(null_mut());
pub static ST_TABLE:AtomicPtr<SystemTable>=AtomicPtr::new(null_mut());
pub static IMAGE_INFO:AtomicPtr<loaded_image::Protocol>=AtomicPtr::new(null_mut());

pub fn set_console_color(color:usize)
{
	let stdout=unsafe{&mut *STDOUT_PROTOCOL.load(Ordering::Relaxed)};
	unsafe
	{
		(stdout.set_attribute)(stdout,color);
	}
}

#[allow(clippy::not_unsafe_ptr_arg_deref)]
pub fn handle_protocol<T>(handle:Handle,mut guid:Guid)->Result<*mut T,Status>
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
	let bs=unsafe
	{
		__isa_available_init();
		// Standard I/O
		STDIN_PROTOCOL.store((*system_table).con_in,Ordering::Relaxed);
		STDOUT_PROTOCOL.store((*system_table).con_out,Ordering::Relaxed);
		// System Services
		ST_TABLE.store(system_table,Ordering::Relaxed);
		BS_TABLE.store((*system_table).boot_services,Ordering::Relaxed);
		RT_TABLE.store((*system_table).runtime_services,Ordering::Relaxed);
		&*(*system_table).boot_services
	};
	// Multi-Processor Protocol. Useful to broadcast a routine to all CPUs.
	let mut mp_guid=mp_services::PROTOCOL_GUID;
	unsafe
	{
		(bs.locate_protocol)(&raw mut mp_guid,null_mut(),MP_PROTOCOL.as_ptr().cast());
	}
	// Loaded Image Protocol. Useful to get self-image.
	IMAGE_INFO.store(handle_protocol(image_handle,loaded_image::PROTOCOL_GUID).unwrap(),Ordering::Relaxed);
}

pub fn block_until_keystroke(unicode:u16)
{
	let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
	let stdin=unsafe{&mut *STDIN_PROTOCOL.load(Ordering::Relaxed)};
	let mut incoming=InputKey::default();
	while incoming.unicode_char!=unicode
	{
		let mut fi=0;
		unsafe
		{
			(bs.wait_for_event)(1,&raw mut stdin.wait_for_key,&raw mut fi);
			(stdin.read_key_stroke)(stdin,&raw mut incoming);
		}
	}
}

pub fn console_print(args:fmt::Arguments)
{
	let mut w:StaticString<512>=StaticString::new();
	if fmt::write(&mut w,args).is_ok()
	{
		noir_system_debugger_write(w.as_ptr(),w.len());
	}
}

#[macro_export]
macro_rules! print
{
	($($args:tt)*) =>
	{
		$crate::host::console_print(format_args!($($args)*))
	};
}

#[macro_export]
macro_rules! println
{
	() =>
	{
		$crate::print!("\n")
	};
	($($args:tt)*) =>
	{
		$crate::print!("{}\n",format_args!($($args)*))
	};
}

#[unsafe(no_mangle)] extern "C" fn noir_system_debugger_write(string:*const u8,length:usize)
{
	let mut w:StaticVec<512,u16>=StaticVec::new();
	let s=unsafe{str::from_utf8_unchecked(slice::from_raw_parts(string,length))};
	// Convert UTF-8 to UTF-16. Also implicit CR in every LF by the way.
	for c in s.encode_utf16()
	{
		if c==b'\n' as u16
		{
			// Implicit CR in every LF because VGA console won't do that for us.
			let _=w.push(b'\r' as u16);
		}
		let _=w.push(c);
	}
	// In Rust, strings aren't automatically terminated with 0.
	// Append null-terminator.
	if w.len()==w.capacity()
	{
		let end=w.len();
		w[end-1]=0;
	}
	else
	{
		let _=w.push(0);
	}
	let stdout_ptr=STDOUT_PROTOCOL.load(Ordering::Relaxed);
	unsafe
	{
		let stdout=&*stdout_ptr;
		(stdout.output_string)(stdout_ptr,w.as_mut_ptr());
	}
}

const PAGE_SHIFT:u8=12;
const PAGE_2MB_SIZE:u64=2<<20;
const PAGE_2MB_MASK_LO:u64=PAGE_2MB_SIZE-1;
const PAGE_2MB_MASK_HI:u64=!PAGE_2MB_MASK_LO;

#[unsafe(no_mangle)] extern "C" fn noir_alloc_2mb_page()->*mut c_void
{
	let mut p=0;
	let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
	let st=unsafe{(bs.allocate_pages)(ALLOCATE_ANY_PAGES,RUNTIME_SERVICES_DATA,1024,&raw mut p)};
	if st.is_error()
	{
		println!("BootService->AllocatePages failed! Status=0x{:X}",st.as_usize());
		null_mut()
	}
	else
	{
		unsafe
		{
			// Get aligned address at 2MiB boundary.
			let aligned_ptr=(p&PAGE_2MB_MASK_HI)+PAGE_2MB_SIZE;
			// Release left-side pages.
			let left_size=(aligned_ptr-p)>>PAGE_SHIFT;
			if left_size!=0
			{
				(bs.free_pages)(p,left_size as usize);
			}
			// Release right-side pages.
			let right_size=(p+PAGE_2MB_SIZE-aligned_ptr)>>PAGE_SHIFT;
			if right_size!=0
			{
				let right_ptr=p+PAGE_2MB_SIZE*2-aligned_ptr;
				(bs.free_pages)(right_ptr,right_size as usize);
			}
			// Return.
			aligned_ptr as *mut c_void
		}
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_free_2mb_page(virtual_address:*mut c_void)
{
	let bs=unsafe{&*BS_TABLE.load(Ordering::Relaxed)};
	unsafe
	{
		(bs.free_pages)(virtual_address as u64,PAGE_2MB_SIZE as usize);
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_get_physical_address(virtual_address:*mut c_void)->u64
{
	virtual_address as u64
}

#[unsafe(no_mangle)] extern "C" fn noir_find_virt_by_phys(physical_address:u64)->*mut c_void
{
	physical_address as *mut c_void
}

#[unsafe(no_mangle)] extern "C" fn noir_map_physical_memory(physical_address:u64,_length:usize)->*mut c_void
{
	physical_address as *mut c_void
}

#[unsafe(no_mangle)] extern "C" fn noir_map_uncached_memory(physical_address:u64,_length:usize)->*mut c_void
{
	physical_address as *mut c_void
}

#[unsafe(no_mangle)] extern "C" fn noir_unmap_physical_memory(_physical_address:u64,_length:usize)
{
	// Nothing to do to unmap stuff in UEFI.
}

#[repr(C)] struct GenericWorkerInfo
{
	worker:extern "C" fn(*mut c_void,u32),
	context:*mut c_void
}

extern "efiapi" fn noir_generic_call_rt(argument:*mut c_void)
{
	let context:&GenericWorkerInfo=unsafe{&*argument.cast()};
	let mp_ptr=MP_PROTOCOL.load(Ordering::Relaxed);
	let proc_id=if mp_ptr.is_null()
	{
		0
	}
	else
	{
		let mp=unsafe{&*mp_ptr};
		let mut n:usize=0;
		unsafe
		{
			(mp.who_am_i)(mp_ptr,&raw mut n);
		}
		n
	};
	(context.worker)(context.context,proc_id as u32);
}

#[unsafe(no_mangle)] extern "C" fn noir_generic_call(worker:extern "C" fn(*mut c_void,u32),context:*mut c_void)
{
	let mut worker_context=GenericWorkerInfo
	{
		worker,
		context
	};
	// Do it in BSP before other APs.
	noir_generic_call_rt((&raw mut worker_context).cast());
	let mp_ptr=MP_PROTOCOL.load(Ordering::Relaxed);
	if !mp_ptr.is_null()
	{
		let mp=unsafe{&*mp_ptr};
		unsafe
		{
			(mp.startup_all_aps)(mp_ptr,noir_generic_call_rt,Boolean::TRUE,null_mut(),0,(&raw mut worker_context).cast(),null_mut());
		}
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_get_processor_count()->u32
{
	let mp_ptr=MP_PROTOCOL.load(Ordering::Relaxed);
	if mp_ptr.is_null()
	{
		1
	}
	else
	{
		let mut n1=0;
		let mut n2=0;
		let mp=unsafe{&*mp_ptr};
		let st=unsafe{(mp.get_number_of_processors)(mp_ptr,&raw mut n1,&raw mut n2)};
		if !st.is_error() {n1 as u32} else {1}
	}
}

#[derive(Debug, Clone, Copy)]
struct MemoryRange
{
	start:u64,
	length:u64
}

impl MemoryRange
{
	fn end(&self)->u64
	{
		self.start+self.length
	}

	fn as_range(&self)->RangeInclusive<u64>
	{
		self.start..=self.end()
	}
}

impl Eq for MemoryRange {}

impl Ord for MemoryRange
{
	fn cmp(&self, other: &Self) -> core::cmp::Ordering
	{
		self.start.cmp(&other.start)
	}
}

impl PartialEq for MemoryRange
{
	fn eq(&self, other: &Self) -> bool
	{
		self.start==other.start
	}
}

impl PartialOrd for MemoryRange
{
	fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering>
	{
		Some(self.cmp(other))
	}
}

impl From<&MemoryDescriptor> for MemoryRange
{
	fn from(value: &MemoryDescriptor) -> Self
	{
		Self
		{
			start:value.physical_start,
			length:value.number_of_pages<<PAGE_SHIFT
		}
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_enum_physical_memory_ranges(callback:extern "C" fn(u64,u64,*mut c_void),context:*mut c_void)
{
	let mut descriptors:MaybeUninit<[MemoryDescriptor;256]>=MaybeUninit::uninit();
	let mut map_size=size_of::<MemoryDescriptor>()*256;
	let mut key:usize=0;
	let mut desc_size:usize=0;
	let mut desc_ver:u32=0;
	let st=unsafe
	{
		let bs=&*BS_TABLE.load(Ordering::Relaxed);
		let d=descriptors.assume_init_mut();
		(bs.get_memory_map)(&raw mut map_size,d.as_mut_ptr(),&raw mut key,&raw mut desc_size,&raw mut desc_ver)
	};
	if !st.is_error()
	{
		let mut offset:usize=0;
		let mut ranges:StaticVec<192,MemoryRange>=StaticVec::new();
		// Enumerate the memory map.
		while offset<map_size
		{
			let map:&MemoryDescriptor=unsafe{&*descriptors.as_ptr().byte_add(offset).cast()};
			if matches!(map.r#type,LOADER_CODE|LOADER_DATA|BOOT_SERVICES_CODE|BOOT_SERVICES_DATA|RUNTIME_SERVICES_CODE|RUNTIME_SERVICES_DATA|CONVENTIONAL_MEMORY|ACPI_RECLAIM_MEMORY)
			{
				// These types of memory can be treated as general-purpose memory.
				// Add them to ranges.
				if ranges.push(MemoryRange::from(map)).is_err()
				{
					panic!("More ranges are expected than the static buffer!");
				}
			}
			offset+=desc_size;
		}
		// Sort the ranges so that we can merge them.
		// We don't have global-allocator here, so only sort_unstable is available.
		ranges.sort_unstable();
		// Merge the ranges.
		let mut i=0;
		while i<ranges.len()-1
		{
			if ranges[i].as_range().contains(&ranges[i+1].start) && ranges[i+1].end()>ranges[i].end()
			{
				ranges[i].length=ranges[i+1].end()-ranges[i].start;
				ranges.remove(i+1);
			}
			else
			{
				i+=1;
			}
		}
		// Call the callback
		for r in ranges.iter()
		{
			callback(r.start,r.length,context);
		}
	}
	else
	{
		dprintln!("BootServices->GetMemoryMap failed! Status=0x{:X}",st.as_usize());
	}
}