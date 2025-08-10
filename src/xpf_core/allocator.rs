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

use core::{ffi::c_void, fmt::{self,Display}, sync::atomic::*};

use portable_dlmalloc::raw::*;
use paste::paste;
use spin::Mutex;

use super::{bitmap::{set_bitmap, reset_bitmap, test_bitmap}, nvbdk::*};
use crate::{system_print, sysdprint, sysdprintln};

static CHECK_ALLOC:AtomicBool=AtomicBool::new(false);

pub fn set_alloc_checker(v:bool)
{
	CHECK_ALLOC.store(v,Ordering::SeqCst);
}

#[cfg(not(test))]
mod dlmalloc
{
	use core::{alloc::*, ffi::c_void, hint::spin_loop, ptr::null_mut, sync::atomic::{AtomicUsize, Ordering}};

	use portable_dlmalloc::DLMalloc;
	
	use super::{CHECK_ALLOC, alloc_2mb_page, free_2mb_page};
	use crate::{system_print, sysdprint, sysdprintln, xpf_core::nvbdk::{PAGE_2MB_SIZE, nulstr_from_ptr}};

	struct InternalAllocator;

	// Re-implement the allocator in order to intercept global allocations.
	unsafe impl GlobalAlloc for InternalAllocator
	{
		unsafe fn alloc(&self, layout: Layout) -> *mut u8
		{
			unsafe
			{
				if CHECK_ALLOC.load(Ordering::SeqCst)
				{
					panic!("Intercepted unwanted allocation! Alignment: {} bytes. Size: {} bytes.",layout.align(),layout.size());
				}
				DLMALLOC.alloc(layout)
			}
		}

		unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout)
		{
			unsafe
			{
				DLMALLOC.dealloc(ptr,layout);
			}
		}

		unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8
		{
			unsafe
			{
				DLMALLOC.realloc(ptr,layout,new_size)
			}
		}
	}

	#[global_allocator] static GLOBAL_ALLOCATOR:InternalAllocator=InternalAllocator;
	static DLMALLOC:DLMalloc=DLMalloc;

	#[unsafe(no_mangle)] unsafe extern "C" fn custom_mmap(length:usize)->*mut c_void
	{
		match alloc_2mb_page()
		{
			Some(md)=>
			{
				sysdprintln!("[mmap] ptr: {:p}, size: 0x{length:X}",md.virt);
				md.virt
			}
			None=>unsafe{null_mut::<c_void>().byte_sub(1)}
		}
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn custom_munmap(ptr:*mut c_void,length:usize)->i32
	{
		sysdprintln!("[munmap] ptr: {ptr:p}, size: 0x{length:X}");
		for i in (0..length).step_by(PAGE_2MB_SIZE)
		{
			unsafe
			{
				free_2mb_page(ptr.byte_add(i));
			}
		}
		0
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn custom_direct_mmap(_length:usize)->*mut c_void
	{
		unsafe
		{
			null_mut::<c_void>().byte_sub(1)
		}
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn init_lock(lock:*mut usize)
	{
		unsafe
		{
			*lock=0;
		}
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn final_lock(_lock:*mut usize)
	{
		// DO NOTHING SINCE SPIN-LOCK DOESN'T NEED FINALIZATION.
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn acquire_lock(lock:*mut usize)
	{
		let p=unsafe{AtomicUsize::from_ptr(lock)};
		// Use a TTAS Spin-Lock.
		while p.compare_exchange(0,1,Ordering::Acquire,Ordering::Relaxed).is_err()
		{
			while p.load(Ordering::Relaxed)==1
			{
				// The pause instruction is intended to optimize spin-locks.
				spin_loop();
			}
		}
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn release_lock(lock:*mut usize)
	{
		let p=unsafe{AtomicUsize::from_ptr(lock)};
		p.store(0,Ordering::Release);
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn custom_abort(message:*const u8,src_file:*const u8,src_line:u32)->!
	{
		unsafe
		{
			let msg=nulstr_from_ptr(message);
			let sfn=nulstr_from_ptr(src_file);
			panic!("DLMalloc aborted! Reason: {msg}\n{sfn}@{src_line}");
		}
	}
}

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
	virt:u64,
	phys:u64,
	alloc_type:PageAllocationType
}

impl Display for PageAllocationInformation
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> fmt::Result
	{
		write!(f,"Virt: {:016X}, Phys: 0x{:016X}, ",self.virt,self.phys)?;
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
					virt:virt as u64,
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
								let s=self.virt+page_mult(i) as u64;
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
										virt:s as *mut c_void,
										phys:noir_get_physical_address(s as *mut c_void)
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
			virt:0,
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

#[unsafe(no_mangle)] unsafe extern "C" fn nvc_free_all_large_pages()
{
	let mut lk=PAGE_ALLOC_MANAGER.lock();
	for entry in &mut lk.list
	{
		match entry.alloc_type
		{
			PageAllocationType::Blank(_)|PageAllocationType::Full=>
			{
				sysdprintln!("Freeing {:016X} from page allocation manager...",entry.virt);
				unsafe{noir_free_2mb_page(entry.virt as *mut c_void)};
			}
			_=>()
		}
	}
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
				let v=self.list[i].virt as *mut c_void;
				if virt>=v && virt<v.byte_add(PAGE_2MB_SIZE)
				{
					match &mut self.list[i].alloc_type
					{
						PageAllocationType::Full=>sysdprintln!("Partially freeing full large-page is unsupported!"),
						PageAllocationType::Invalid=>panic!("Freeing invalid entry!"),
						PageAllocationType::Blank(info)=>
						{
							let start=page_count(virt.offset_from(v) as usize);
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

static PAGE_ALLOC_MANAGER:Mutex<PageAllocationManager>=Mutex::new(PageAllocationManager::empty());

pub fn alloc_contd_pages(length:usize)->Option<MemoryDescriptor>
{
	let mut lk=PAGE_ALLOC_MANAGER.lock();
	lk.alloc_pages(page_count(length))
}

pub fn free_contd_pages(virt:*mut c_void,length:usize)
{
	let mut lk=PAGE_ALLOC_MANAGER.lock();
	lk.free_pages(virt,page_count(length));
}

pub fn alloc_2mb_page()->Option<MemoryDescriptor>
{
	let mut lk=PAGE_ALLOC_MANAGER.lock();
	lk.new_full();
	Some
	(
		MemoryDescriptor
		{
			virt:lk.list[lk.count-1].virt as *mut c_void,
			phys:lk.list[lk.count-1].phys
		}
	)
}

pub fn free_2mb_page(ptr:*mut c_void)
{
	let mut lk=PAGE_ALLOC_MANAGER.lock();
	for i in 0..lk.count
	{
		if core::ptr::eq(lk.list[i].virt as *mut c_void,ptr)
		{
			lk.list[i].alloc_type=PageAllocationType::Blank([0;8]);
			break;
		}
	}
}

pub fn enum_allocated_large_pages(callback_rt:PhysicalRangeCallback,context:*mut c_void)
{
	let mut i:usize=0;
	loop
	{
		let count={PAGE_ALLOC_MANAGER.lock().count};
		if i>=count {break;}
		let phys={PAGE_ALLOC_MANAGER.lock().list[i].phys};
		// The callback routine may allocate memories.
		// Therefore, there mustn't be any lock holders on allocation manager.
		callback_rt(phys,PAGE_2MB_SIZE as u64,context);
		i+=1;
	}
}