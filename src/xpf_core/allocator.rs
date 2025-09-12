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
#[cfg(not(test))] use core::mem::ManuallyDrop;

use log::info;
#[cfg(not(test))]
use portable_dlmalloc::raw::*;
use paste::paste;
use spin::Mutex;
use static_collections::bitmap::RefBitmap;

use super::nvbdk::*;
use crate::{system_print, sysdprint, sysdprintln};

static CHECK_ALLOC:AtomicBool=AtomicBool::new(false);

pub fn set_alloc_checker(v:bool)
{
	CHECK_ALLOC.store(v,Ordering::SeqCst);
}

/// ## The `kmalloc` module
/// This module wraps the OS kernel's memory allocator and implements `Allocator` trait.
pub mod kmalloc
{
    use core::{alloc::*,ptr::NonNull,slice};

	unsafe extern "C"
	{
		fn noir_kmalloc(length:usize,alignment:usize)->*mut u8;
		fn noir_kfree(ptr:*mut u8,length:usize,alignment:usize);
	}

	#[derive(Clone, Copy)]
	pub struct KernelAllocator;

	unsafe impl Allocator for KernelAllocator
	{
		fn allocate(&self,layout:Layout)->Result<NonNull<[u8]>,AllocError>
		{
			let ptr=unsafe{noir_kmalloc(layout.size(),layout.align())};
			let s=unsafe{slice::from_raw_parts_mut(ptr,layout.size())};
			match NonNull::new(&raw mut *s)
			{
				Some(nn)=>Ok(nn),
				None=>Err(AllocError)
			}
		}

		unsafe fn deallocate(&self,ptr:NonNull<u8>,layout:Layout)
		{
			unsafe
			{
				noir_kfree(ptr.as_ptr(),layout.size(),layout.align());
			}
		}
	}

	pub static KERNEL_ALLOCATOR:KernelAllocator=KernelAllocator;
}

#[cfg(not(test))]
mod dlmalloc
{
	use core::{alloc::*, ffi::c_void, hint::spin_loop, ptr::null_mut, sync::atomic::{AtomicUsize, Ordering}};

	use portable_dlmalloc::DLMalloc;
	
	use super::CHECK_ALLOC;
	use crate::{sysdprint, sysdprintln, system_print, xpf_core::{allocator::PAGE_ALLOC_MANAGER, nvbdk::{nulstr_from_ptr, MemoryDescriptor, PAGE_2MB_SIZE}}};

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
		match MemoryDescriptor::alloc_2mb_page_manual_drop()
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
				let p=ptr.byte_add(i);
				let mut lk=PAGE_ALLOC_MANAGER.lock();
				lk.free_large_page(p);
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

#[cfg(not(test))]
pub fn get_used()->usize
{
	unsafe
	{
		dlmallinfo().uordblks
	}
}

#[cfg(not(test))]
pub fn get_free()->usize
{
	unsafe 
	{
		dlmallinfo().fordblks
	}
}

enum PageAllocationType
{
	Invalid,
	Full,
	Blank([u64;8]),
}

struct PageAllocationInformation
{
	descriptor:MemoryDescriptor<PAGE_TABLE_ENTRIES64,c_void>,
	alloc_type:PageAllocationType
}

impl Display for PageAllocationInformation
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> fmt::Result
	{
		write!(f,"Virt: {:p}, Phys: 0x{:016X}, ",self.descriptor.virt,self.descriptor.phys)?;
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
					descriptor:unsafe{MemoryDescriptor::new(virt,noir_get_physical_address(virt))},
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
				descriptor:info.descriptor,
				alloc_type:PageAllocationType::Full
			}
		)
	}

	fn alloc_pages(&mut self,pages:usize)->Option<(*mut c_void,u64)>
	{
		match &mut self.alloc_type
		{
			PageAllocationType::Blank(info)=>
			{
				let bmp:&mut RefBitmap<PAGE_TABLE_ENTRIES64>=unsafe{RefBitmap::from_raw_mut_ptr(info.as_mut_ptr().cast())};
				let mut i:usize=0;
				while i<PAGE_TABLE_ENTRIES64
				{
					if !bmp.test(i).unwrap()
					{
						let mut is_free=true;
						for j in i+1..i+pages
						{
							if j==PAGE_TABLE_ENTRIES64
							{
								is_free=false;
								break;
							}
							if bmp.test(j).unwrap()
							{
								is_free=false;
								break;
							}
						}
						if is_free
						{
							unsafe 
							{
								let s=self.descriptor.virt.byte_add(page_mult(i));
								// Set the bitmap.
								for j in i..i+pages
								{
									let _=bmp.set(j);
								}
								// println!("[alloc] Allocated Page {s:p} to {:p}! Allocation Info: {self}",s.byte_add(pages));
								return Some((s,noir_get_physical_address(s)));
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
			descriptor:MemoryDescriptor::null(),
			alloc_type:PageAllocationType::Invalid
		}
	}
}

struct PageAllocationManager
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
				sysdprintln!("Freeing {:p} from page allocation manager...",entry.descriptor.virt);
				unsafe{noir_free_2mb_page(entry.descriptor.virt)};
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

	fn alloc_pages(&mut self,pages:usize)->Option<(*mut c_void,u64)>
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
				let v=self.list[i].descriptor.virt;
				if virt>=v && virt<v.byte_add(PAGE_2MB_SIZE)
				{
					match &mut self.list[i].alloc_type
					{
						PageAllocationType::Full=>sysdprintln!("Partially freeing full large-page is unsupported!"),
						PageAllocationType::Invalid=>panic!("Freeing invalid entry!"),
						PageAllocationType::Blank(info)=>
						{
							let bmp:&mut RefBitmap<64>=RefBitmap::from_raw_mut_ptr(info.as_mut_ptr().cast());
							let start=page_count(virt.offset_from(v) as usize);
							for j in start..start+pages
							{
								let _=bmp.reset(j);
							}
						}
					}
					break;
				}
			}
		}
	}

	fn free_large_page(&mut self,virt:*mut c_void)
	{
		// Search for large pages.
		for x in &mut self.list
		{
			if core::ptr::eq(virt,x.descriptor.virt as *const c_void)
			{
				match x.alloc_type
				{
					PageAllocationType::Full=>x.alloc_type=PageAllocationType::Blank([0;8]),
					PageAllocationType::Blank(_)=>panic!("Freeing large page from blank-entry!"),
					PageAllocationType::Invalid=>panic!("Freeing invalid entry!")
				}
			}
		}
	}

	const fn empty()->Self
	{
		Self
		{
			list:[const{PageAllocationInformation::empty()};64],
			count:0
		}
	}
}

static PAGE_ALLOC_MANAGER:Mutex<PageAllocationManager>=Mutex::new(PageAllocationManager::empty());

impl<const N:usize,T:Sized> MemoryDescriptor<N,T>
{
	pub fn alloc()->Option<Self>
	{
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		lk.alloc_pages(N).map(|(virt,phys)| unsafe{MemoryDescriptor::new(virt.cast(),phys)})
	}
}

impl<const N:usize,T:Sized> Drop for MemoryDescriptor<N,T>
{
	fn drop(&mut self)
	{
		if self.virt.is_null()
		{
			return;
		}
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		if N==PAGE_TABLE_ENTRIES64
		{
			// This is probably 2MiB page.
			lk.free_large_page(self.virt.cast());
			info!("Freeing Large Page at {:p}...",self.virt);
		}
		else
		{
			// This is normal contiguous page.
			lk.free_pages(self.virt.cast(),N);
			info!("Freeing Page at {:p}...",self.virt);
		}
	}
}

impl<T:Sized> MemoryDescriptor<PAGE_TABLE_ENTRIES64,T>
{
	pub fn alloc_2mb_page()->Option<Self>
	{
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		lk.new_full();
		Some(unsafe{MemoryDescriptor::new(lk.list[lk.count-1].descriptor.virt.cast(),lk.list[lk.count-1].descriptor.phys)})
	}

	// This routine can only be used by dynamic allocator's large-page allocations.
	#[cfg(not(test))]
	fn alloc_2mb_page_manual_drop()->Option<ManuallyDrop<Self>>
	{
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		lk.new_full();
		Some(ManuallyDrop::new(unsafe{MemoryDescriptor::new(lk.list[lk.count-1].descriptor.virt.cast(),lk.list[lk.count-1].descriptor.phys)}))
	}
}

pub fn enum_allocated_large_pages(callback_rt:PhysicalRangeCallback,context:*mut c_void)
{
	let mut i:usize=0;
	loop
	{
		let count={PAGE_ALLOC_MANAGER.lock().count};
		if i>=count {break;}
		let phys={PAGE_ALLOC_MANAGER.lock().list[i].descriptor.phys};
		// The callback routine may allocate memories.
		// Therefore, there mustn't be any lock holders on allocation manager.
		callback_rt(phys,PAGE_2MB_SIZE as u64,context);
		i+=1;
	}
}