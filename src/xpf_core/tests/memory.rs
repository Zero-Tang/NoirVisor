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

use core::{alloc::Layout, ffi::c_void, ptr::null_mut, sync::atomic::{AtomicPtr, Ordering}};

use crate::xpf_core::nvbdk::{page_4kb_mult, PAGE_4KB_SIZE, PAGE_2MB_SIZE};

// 4MiB of unaligned global variable guarantees a 2MiB-aligned page.
static mut ALLOC_BUFFER:[u8;4<<20]=[0;4<<20];
// If this pointer is null, then it's not allocated.
static ALLOCATED_PTR:AtomicPtr<u8>=AtomicPtr::new(null_mut());

#[unsafe(no_mangle)] extern "C" fn noir_alloc_2mb_page()->*mut c_void
{
	if ALLOCATED_PTR.load(Ordering::SeqCst).is_null()
	{
		#[allow(static_mut_refs)]
		let p=unsafe{ALLOC_BUFFER.as_mut_ptr()};
		ALLOCATED_PTR.store(unsafe{p.map_addr(|x| x & !(PAGE_2MB_SIZE-1)).add(PAGE_2MB_SIZE)},Ordering::SeqCst);
		ALLOCATED_PTR.load(Ordering::SeqCst).cast()
	}
	else
	{
		unsafe{null_mut::<c_void>().byte_sub(1)}
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_free_2mb_page(virt:*mut c_void)
{
	let _=ALLOCATED_PTR.compare_exchange(virt.cast(),null_mut(),Ordering::SeqCst,Ordering::Relaxed);
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
	unsafe
	{
		alloc::alloc::alloc(Layout::from_size_align_unchecked(page_4kb_mult(pages),PAGE_4KB_SIZE)).cast()
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_kmunmap(ptr:*mut c_void,pages:usize)
{
	unsafe
	{
		alloc::alloc::dealloc(ptr.cast(),Layout::from_size_align_unchecked(page_4kb_mult(pages),PAGE_4KB_SIZE));
	}
}