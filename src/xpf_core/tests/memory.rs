/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file mocks Memory-related APIs for testing NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{alloc::Layout, ffi::c_void, ptr::null_mut, sync::atomic::{AtomicBool, Ordering}};

#[cfg(windows)]
use windows::Win32::System::Memory::{VirtualAlloc, VirtualFree, MEM_COMMIT, MEM_DECOMMIT, PAGE_READWRITE};

use crate::xpf_core::nvbdk::page_4kb_mult;

static mut ALLOC_BUFFER:[u8;2<<20]=[0;2<<20];
static ALLOCATED:AtomicBool=AtomicBool::new(false);

#[unsafe(no_mangle)] extern "C" fn noir_alloc_2mb_page()->*mut c_void
{
	if ALLOCATED.load(Ordering::SeqCst)
	{
		unsafe{null_mut::<c_void>().byte_sub(1)}
	}
	else
	{
		#[allow(static_mut_refs)]
		let p:*mut c_void=unsafe{ALLOC_BUFFER.as_mut_ptr().cast()};
		(p as u64 & !((1<<20)-1)) as *mut c_void
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_free_2mb_page(_virt:*mut c_void)
{

}

#[unsafe(no_mangle)] extern "C" fn noir_get_physical_address(virt:*mut c_void)->u64
{
	virt as u64
}

#[unsafe(no_mangle)] extern "C" fn noir_find_virt_by_phys(phys:u64)->*mut c_void
{
	phys as *mut c_void
}

#[unsafe(no_mangle)] extern "C" fn noir_map_uncached_memory(phys:u64,_size:usize)->*mut c_void
{
	phys as *mut c_void
}

#[unsafe(no_mangle)] extern "C" fn noir_map_physical_memory(phys:u64,_size:usize)->*mut c_void
{
	phys as *mut c_void
}

#[unsafe(no_mangle)] extern "C" fn noir_unmap_physical_memory(_virt:*mut c_void,_size:usize)
{

}

pub type PhysicalRangeCallback=extern "C" fn(start:u64,length:u64,context:*mut c_void);

#[unsafe(no_mangle)] extern "C" fn noir_enum_physical_memory_ranges(_callback_routine:PhysicalRangeCallback,_context:*mut c_void)
{

}

#[unsafe(no_mangle)] extern "C" fn noir_kmalloc(length:usize,alignment:usize)->*mut u8
{
	unsafe
	{
		// Just forward to default memory allocator.
		alloc::alloc::alloc(Layout::from_size_align_unchecked(length,alignment))
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_kfree(ptr:*mut u8,length:usize,alignment:usize)
{
	unsafe
	{
		// Just forward to default memory allocator.
		alloc::alloc::dealloc(ptr,Layout::from_size_align_unchecked(length,alignment))
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_kmmap(pages:usize)->*mut c_void
{
	#[cfg(windows)]
	unsafe
	{
		VirtualAlloc(None,page_4kb_mult(pages),MEM_COMMIT,PAGE_READWRITE)
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_kmunmap(ptr:*mut c_void,pages:usize)
{
	#[cfg(windows)]
	unsafe
	{
		let _=VirtualFree(ptr,page_4kb_mult(pages),MEM_DECOMMIT);
	}
}