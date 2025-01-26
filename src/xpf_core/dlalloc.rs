/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines global allocator for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{alloc::*, arch::asm, ffi::c_void, fmt::{self,Display}, ptr::null_mut, sync::atomic::*, slice, str};

use portable_dlmalloc::raw::*;
use paste::paste;

use super::{bitmap::{set_bitmap, reset_bitmap, test_bitmap}, nvbdk::*};
use crate::{dbg_print, print, println};

static CHECK_ALLOC:AtomicBool=AtomicBool::new(false);

pub fn set_alloc_checker(v:bool)
{
	CHECK_ALLOC.store(v,Ordering::SeqCst);
}

// Re-implement the allocator in order to intercept global allocations.
struct InternalAllocator;

unsafe impl GlobalAlloc for InternalAllocator
{
	unsafe fn alloc(&self, layout: Layout) -> *mut u8
	{
		if CHECK_ALLOC.load(Ordering::SeqCst)
		{
			panic!("Intercepted unwanted allocation! Alignment: {} bytes. Size: {} bytes.",layout.align(),layout.size());
		}
		dlmemalign(layout.align(),layout.size()).cast()
	}

	unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout)
	{
		dlfree(ptr.cast())
	}

	unsafe fn realloc(&self, ptr: *mut u8, _layout: Layout, new_size: usize) -> *mut u8
	{
		dlrealloc(ptr.cast(),new_size).cast()
	}
}

#[global_allocator] static GLOBAL_ALLOCATOR:InternalAllocator=InternalAllocator;

pub fn get_used()->usize
{
	unsafe
	{
		dlmallinfo().uordblks
	}
}

pub fn get_free()->usize
{
	unsafe 
	{
		dlmallinfo().fordblks
	}
}

#[derive(Clone, Copy)]
enum PageAllocationType
{
	Invalid,
	Full,
	Blank([u64;8]),
}

#[derive(Clone, Copy)]
pub struct PageAllocationInformation
{
	virt:*mut c_void,
	phys:u64,
	alloc_type:PageAllocationType
}

impl Display for PageAllocationInformation
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> fmt::Result
	{
		write!(f,"Virt: {:p}, Phys: 0x{:016X}, ",self.virt,self.phys)?;
		match self.alloc_type
		{
			PageAllocationType::Full=>write!(f,"Full Large Page is occupied."),
			PageAllocationType::Invalid=>write!(f,"This is an invalid entry."),
			PageAllocationType::Blank(info)=>
			{
				writeln!(f,"This is a blank bitmapped entry! Bitmap:")?;
				for i in 0..8
				{
					write!(f,"{:016X}",info[7-i])?;
				}
				Ok(())
			}
		}
	}
}

impl PageAllocationInformation
{
	fn new_blank()->Option<Self>
	{
		let virt=unsafe{noir_alloc_2mb_page()};
		if virt.is_null()
		{
			None
		}
		else
		{
			Some
			(
				Self
				{
					virt,
					phys:unsafe{noir_get_physical_address(virt)},
					alloc_type:PageAllocationType::Blank([0;8])
				}
			)
		}
	}

	fn new_full()->Option<Self>
	{
		Self::new_blank().map
		(
			|info| Self
			{
				virt:info.virt,
				phys:info.phys,
				alloc_type:PageAllocationType::Full
			}
		)
	}

	fn alloc_pages(&mut self,pages:usize)->Option<MemoryDescriptor>
	{
		match &mut self.alloc_type
		{
			PageAllocationType::Blank(info)=>
			{
				let bmp=info.as_mut_ptr() as *mut c_void;
				let mut i:usize=0;
				while i<PAGE_TABLE_ENTRIES64
				{
					if !unsafe{test_bitmap(bmp,PAGE_TABLE_ENTRIES64>>3,i)}
					{
						let mut is_free=true;
						for j in i+1..i+pages
						{
							if j==512
							{
								is_free=false;
								break;
							}
							if unsafe{test_bitmap(bmp,PAGE_TABLE_ENTRIES64>>3,j)}
							{
								is_free=false;
								break;
							}
						}
						if is_free
						{
							unsafe 
							{
								let s=self.virt.byte_add(page_mult(i));
								// Set the bitmap.
								for j in i..i+pages
								{
									set_bitmap(bmp,PAGE_TABLE_ENTRIES64>>3,j);
								}
								// println!("[alloc] Allocated Page {s:p} to {:p}! Allocation Info: {self}",s.byte_add(pages));
								return Some
								(
									MemoryDescriptor
									{
										virt:s,
										phys:noir_get_physical_address(s)
									}
								);
							}
						}
					}
					i+=1;
				}
				None
			}
			_=>None
		}
	}

	const fn empty()->Self
	{
		Self
		{
			virt:null_mut(),
			phys:0,
			alloc_type:PageAllocationType::Invalid
		}
	}
}

pub struct PageAllocationManager
{
	list:[PageAllocationInformation;64],
	count:usize
}

#[no_mangle] unsafe extern "C" fn nvc_free_all_large_pages()
{
	let pa_mgr=&raw mut PAGE_ALLOC_MANAGER;
	for entry in &mut (*pa_mgr).list
	{
		match entry.alloc_type
		{
			PageAllocationType::Blank(_)|PageAllocationType::Full=>
			{
				println!("Freeing {:p} from page allocation manager...",entry.virt);
				noir_free_2mb_page(entry.virt);
			}
			_=>()
		}
	}
	*pa_mgr=PageAllocationManager::empty();
}

macro_rules! build_alloc_manager
{
	($name:tt) =>
	{
		paste!
		{
			fn [<new_ $name>](&mut self)
			{
				if self.count>=64
				{
					panic!("NoirVisor has exceeded 128MiB Internal allocation limit!");
				}
				match PageAllocationInformation::[<new_ $name>]()
				{
					Some(info)=>
					{
						// Insert to the end.
						self.list[self.count]=info;
						self.count+=1;
					}
					None=>panic!("Failed to allocate new 2MiB Page!")
				}
			}
		}
	};
}

impl PageAllocationManager
{
	build_alloc_manager!(blank);
	build_alloc_manager!(full);

	fn alloc_pages(&mut self,pages:usize)->Option<MemoryDescriptor>
	{
		// Try to allocate pages from existing large pages.
		for i in 0..self.count
		{
			if let Some(md)=self.list[i].alloc_pages(pages)
			{
				return Some(md);
			}
		}
		// At this point, we need to allocate new large pages!
		self.new_blank();
		self.list[self.count-1].alloc_pages(pages)
	}

	fn free_pages(&mut self,virt:*mut c_void,pages:usize)
	{
		// Search for large pages.
		for i in 0..self.count
		{
			unsafe
			{
				if virt>=self.list[i].virt && virt<self.list[i].virt.byte_add(PAGE_2MB_SIZE)
				{
					match &mut self.list[i].alloc_type
					{
						PageAllocationType::Full=>println!("Partially freeing full large-page is unsupported!"),
						PageAllocationType::Invalid=>panic!("Freeing invalid entry!"),
						PageAllocationType::Blank(info)=>
						{
							let start=page_count(virt.offset_from(self.list[i].virt) as usize);
							for j in start..start+pages
							{
								reset_bitmap(info.as_mut_ptr().cast(),64,j);
							}
						}
					}
					break;
				}
			}
		}
	}

	const fn empty()->Self
	{
		Self
		{
			list:[PageAllocationInformation::empty();64],
			count:0
		}
	}
}

static mut PAGE_ALLOC_MANAGER:PageAllocationManager=PageAllocationManager::empty();

pub fn alloc_contd_pages(length:usize)->Option<MemoryDescriptor>
{
	unsafe 
	{
		let pa_mgr=&raw mut PAGE_ALLOC_MANAGER;
		(*pa_mgr).alloc_pages(page_count(length))
	}
}

pub fn free_contd_pages(virt:*mut c_void,length:usize)
{
	unsafe 
	{
		let pa_mgr=&raw mut PAGE_ALLOC_MANAGER;
		(*pa_mgr).free_pages(virt,page_count(length));
	}
}

pub fn alloc_2mb_page()->Option<MemoryDescriptor>
{
	unsafe
	{
		let pa_mgr=&raw mut PAGE_ALLOC_MANAGER;
		(*pa_mgr).new_full();
		Some
		(
			MemoryDescriptor
			{
				virt:(*pa_mgr).list[(*pa_mgr).count-1].virt,
				phys:(*pa_mgr).list[(*pa_mgr).count-1].phys
			}
		)
	}
}

pub fn free_2mb_page(ptr:*mut c_void)
{
	unsafe
	{
		let pa_mgr=&raw mut PAGE_ALLOC_MANAGER;
		for i in 0..(*pa_mgr).count
		{
			if (*pa_mgr).list[i].virt==ptr
			{
				(*pa_mgr).list[i].alloc_type=PageAllocationType::Blank([0;8]);
				break;
			}
		}
	}
}

pub fn enum_allocated_large_pages(callback_rt:PhysicalRangeCallback,context:*mut c_void)
{
	unsafe
	{
		let pa_mgr=&raw mut PAGE_ALLOC_MANAGER;
		for i in 0..(*pa_mgr).count
		{
			println!("{}",(*pa_mgr).list[i]);
			callback_rt((*pa_mgr).list[i].phys,PAGE_2MB_SIZE as u64,context);
		}
	}
}

#[no_mangle] unsafe extern "C" fn custom_mmap(length:usize)->*mut c_void
{
	let pa_mgr=&raw mut PAGE_ALLOC_MANAGER;
	(*pa_mgr).new_full();
	let index=(*pa_mgr).count-1;
	let p=(*pa_mgr).list[index].virt;
	println!("[mmap] ptr: {p:p}, size: 0x{length:X}, index: {index}");
	p
}

#[no_mangle] unsafe extern "C" fn custom_munmap(ptr:*mut c_void,length:usize)->i32
{
	println!("[munmap] ptr: {ptr:p}, size: 0x{length:X}");
	for i in (0..length).step_by(PAGE_2MB_SIZE)
	{
		free_2mb_page(ptr.byte_add(i));
	}
	0
}

#[no_mangle] unsafe extern "C" fn custom_direct_mmap(_length:usize)->*mut c_void
{
	null_mut::<c_void>().byte_sub(1)
}

#[no_mangle] unsafe extern "C" fn init_lock(lock:*mut usize)
{
	*lock=0;
}

#[no_mangle] unsafe extern "C" fn final_lock(_lock:*mut usize)
{
	// DO NOTHING SINCE SPIN-LOCK DOESN'T NEED FINALIZATION.
}

#[no_mangle] unsafe extern "C" fn acquire_lock(lock:*mut usize)
{
	let p=AtomicUsize::from_ptr(lock);
	// Use a TTAS Spin-Lock.
	while p.compare_exchange(0,1,Ordering::Acquire,Ordering::Relaxed).is_err()
	{
		while p.load(Ordering::Relaxed)==1
		{
			// The pause instruction is intended to optimize spin-locks.
			asm!("pause");
		}
	}
}

#[no_mangle] unsafe extern "C" fn release_lock(lock:*mut usize)
{
	let p=AtomicUsize::from_ptr(lock);
	p.store(0,Ordering::Release);
}

#[no_mangle] unsafe extern "C" fn custom_abort(message:*const u8,src_file:*const u8,src_line:u32)->!
{
	let msg=nulstr_from_ptr(message);
	let sfn=nulstr_from_ptr(src_file);
	panic!("DLMalloc aborted! Reason: {msg}\n{sfn}@{src_line}");
}

unsafe fn nulstr_from_ptr<'a>(ptr:*const u8)->&'a str
{
	let str_slice=slice::from_raw_parts(ptr,strlen(ptr));
	str::from_utf8_unchecked(str_slice)
}