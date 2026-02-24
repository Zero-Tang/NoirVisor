/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file mocks Memory-related APIs for testing NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{alloc::Layout, ffi::c_void, ptr::{self, null_mut}, sync::atomic::{AtomicPtr, Ordering}};

use alloc::vec::Vec;

use crate::xpf_core::nvbdk::{PAGE_2MB_SIZE, PAGE_4KB_SIZE, page_4kb_mult, page_count, page_offset};

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

struct MockMemorySlot
{
	uva:*mut c_void,
	length:usize,
	pfn_list:Vec<usize>
}

#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)] static noir_maximum_memslot_shift:u8=31;

#[unsafe(no_mangle)] extern "C" fn noir_create_memory_slot(uva:*mut c_void,length:usize,slot:*mut *mut MockMemorySlot)->bool
{
	if page_offset(uva as usize)==0 && page_offset(length)==0
	{
		let mut v:Vec<usize>=Vec::with_capacity(page_count(length));
		for i in (0..length).step_by(0x1000)
		{
			v.push(uva as usize+i);
		}
		let new_slot:*mut MockMemorySlot=unsafe{alloc::alloc::alloc(Layout::from_size_align_unchecked(size_of::<MockMemorySlot>(),align_of::<MockMemorySlot>())).cast()};
		if new_slot.is_null()
		{
			false
		}
		else
		{
			unsafe
			{
				(*new_slot).length=length;
				(*new_slot).uva=uva;
				ptr::write(&raw mut (*new_slot).pfn_list,v);
				*slot=new_slot;
			}
			true
		}
	}
	else
	{
		false
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_remove_memory_slot(slot:*mut MockMemorySlot)
{
	unsafe
	{
		// Read it out so that it can be dropped by RAII.
		let pfn_list=ptr::read(&raw const (*slot).pfn_list);
		alloc::alloc::dealloc(slot.cast(),Layout::from_size_align_unchecked(size_of::<MockMemorySlot>(),align_of::<MockMemorySlot>()));
		// While we don't really need to manually drop it by virtue of RAII,
		// this manual drop is for the sake of getting rid of compiler warning.
		drop(pfn_list);
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_get_pfn_from_memory_slot(slot:*const MockMemorySlot,offset:usize,result:*mut u64)->bool
{
	let pfn_list=unsafe{&(*slot).pfn_list};
	let index=page_count(offset);
	match pfn_list.get(index)
	{
		Some(pfn)=>
		{
			unsafe
			{
				*result=*pfn as u64;
			}
			true
		}
		None=>false
	}
}