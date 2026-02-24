/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines global allocator for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, fmt::{self,Display}, sync::atomic::*};

use log::info;
use portable_dlmalloc::raw::*;
use spin::Mutex;
use static_collections::bitmap::RefBitmap;

use super::nvbdk::*;
use crate::sysdprintln;

static CHECK_ALLOC:AtomicBool=AtomicBool::new(false);

// Minimum chunk for malloc is (1<<6)=64 pages (256KiB).
const MALLOC_CHUNK_SHIFT:usize=6;

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

mod dlmalloc
{
	use core::{alloc::*, ffi::c_void, hint::spin_loop, ptr::null_mut, sync::atomic::{AtomicUsize, Ordering}};

	use portable_dlmalloc::DLMalloc;
	
	use super::{CHECK_ALLOC,PAGE_ALLOC_MANAGER};
	use crate::{sysdprintln, xpf_core::nvbdk::{nulstr_from_ptr, page_count}};

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
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		match lk.alloc_pages_for_malloc(page_count(length))
		{
			Some(p)=>p,
			None=>unsafe{null_mut::<c_void>().byte_sub(1)}
		}
	}

	#[unsafe(no_mangle)] unsafe extern "C" fn custom_munmap(ptr:*mut c_void,length:usize)->i32
	{
		sysdprintln!("[munmap] ptr: {ptr:p}, size: 0x{length:X}");
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		lk.free_pages(ptr,page_count(length));
		0
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

enum PageAllocationType
{
	Invalid,
	Valid([u64;8]),
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
			PageAllocationType::Invalid=>write!(f,"This is an invalid entry."),
			PageAllocationType::Valid(info)=>
			{
				writeln!(f,"This is a valid entry! Bitmap:")?;
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
	fn new()->Option<Self>
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
					alloc_type:PageAllocationType::Valid([0;8])
				}
			)
		}
	}

	fn alloc_pages(&mut self,pages:usize)->Option<(*mut c_void,u64)>
	{
		match &mut self.alloc_type
		{
			PageAllocationType::Valid(info)=>
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

	fn alloc_pages_for_malloc(&mut self,pages:usize)->Option<*mut c_void>
	{
		match &mut self.alloc_type
		{
			PageAllocationType::Valid(info)=>
			{
				// 256KiB-per-chunk
				let chunks=pages>>MALLOC_CHUNK_SHIFT;
				sysdprintln!("Allocating {chunks} chunks for dlmalloc...");
				for i in (0..=8-chunks).rev()
				{
					// Search for a free chunk.
					if info[i]==0
					{
						let mut is_free=true;
						let chunks=if i<chunks {i} else {chunks};
						for j in 0..chunks
						{
							if info[i-j]!=0
							{
								is_free=false;
								break;
							}
						}
						if is_free
						{
							for j in 0..chunks
							{
								info[i-j]=u64::MAX;
							}
							let p:*mut c_void=unsafe{self.descriptor.virt.byte_add(page_mult(i-chunks+1)<<MALLOC_CHUNK_SHIFT)};
							sysdprintln!("malloc-chunk: {p:p}");
							return Some(p);
						}
					}
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
		if let PageAllocationType::Valid(_)=entry.alloc_type
		{
			sysdprintln!("Freeing {:p} from page allocation manager...",entry.descriptor.virt);
			unsafe{noir_free_2mb_page(entry.descriptor.virt)};
		}
	}
}

impl PageAllocationManager
{
	fn new_blank(&mut self)
	{
		if self.count>=self.list.len()
		{
			panic!("NoirVisor has exceeded 128MiB Internal allocation limit!");
		}
		match PageAllocationInformation::new()
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

	fn new_full(&mut self)
	{
		if self.count>=self.list.len()
		{
			panic!("NoirVisor has exceeded 128MiB Internal allocation limit!");
		}
		match PageAllocationInformation::new()
		{
			Some(mut info)=>
			{
				info.alloc_type=PageAllocationType::Valid([u64::MAX;8]);
				// Insert to the end.
				self.list[self.count]=info;
				self.count+=1;
			}
			None=>panic!("Failed to allocate new 2MiB Page!")
		}
	}

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

	fn alloc_pages_for_malloc(&mut self,pages:usize)->Option<*mut c_void>
	{
		sysdprintln!("Trying to allocate {pages} pages for dlmalloc chunk...");
		// Try to allocate pages from existing large pages.
		for i in 0..self.count
		{
			if let Some(md)=self.list[i].alloc_pages_for_malloc(pages)
			{
				sysdprintln!("Allocated {md:p} for dlmalloc chunk!");
				return Some(md);
			}
		}
		// At this point, we need to allocate new large pages!
		self.new_blank();
		self.list[self.count-1].alloc_pages_for_malloc(pages)
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
						PageAllocationType::Invalid=>panic!("Freeing pages from invalid entry! Address={virt:p}"),
						PageAllocationType::Valid(info)=>
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
					PageAllocationType::Valid(_)=>info!("Freeing large page from valid-entry at {virt:p}..."),
					PageAllocationType::Invalid=>panic!("Freeing invalid entry! Address={virt:p}")
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

pub fn print_allocation()
{
	let lk=PAGE_ALLOC_MANAGER.lock();
	for i in 0..lk.count
	{
		sysdprintln!("Large Page {i}: {}",lk.list[i]);
	}
}

pub fn get_large_page_count()->usize
{
	let lk=PAGE_ALLOC_MANAGER.lock();
	let mut count=0;
	for x in &lk.list
	{
		count+=match x.alloc_type
		{
			PageAllocationType::Valid(_)=>1,
			PageAllocationType::Invalid=>0,
		};
	}
	count
}

/// # The `ContiguousAllocator` trait
/// This trait defines the trait for page allocators.
/// 
/// # Safety
/// It is your duty to guarantee the validity of pointers.
pub unsafe trait ContiguousAllocator
{
	/// # The `alloc` method
	/// Allocates `pages` of memory in page-granularity.
	/// 
	/// # Safety
	/// You must make sure the returned pointer is page-aligned.
	unsafe fn alloc(&self,pages:usize)->Option<(*mut c_void,u64)>;
	/// # The `free` method
	/// Free `pages` of memory in page-granularity.
	/// 
	/// # Safety
	/// It is your duty to guarantee the release procedure is safe.
	unsafe fn free(&self,virt:*mut c_void,pages:usize);
}

pub struct InternalPageAllocator;

unsafe impl ContiguousAllocator for InternalPageAllocator
{
	unsafe fn alloc(&self,pages:usize)->Option<(*mut c_void,u64)>
	{
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		lk.alloc_pages(pages)
	}

	unsafe fn free(&self,virt:*mut c_void,pages:usize)
	{
		let mut lk=PAGE_ALLOC_MANAGER.lock();
		if pages==PAGE_TABLE_ENTRIES64
		{
			// This is probably 2MiB page.
			lk.free_large_page(virt);
			info!("Freeing Large Page at {virt:p}...");
		}
		else
		{
			// This is normal contiguous page.
			lk.free_pages(virt,pages);
			info!("Freeing Page at {virt:p}...");
		}
	}
}

unsafe extern "C"
{
	fn noir_kmmap(pages:usize)->*mut c_void;
	fn noir_kmunmap(virt:*mut c_void,pages:usize);
}

pub struct SystemPageAllocator;

unsafe impl ContiguousAllocator for SystemPageAllocator
{
	unsafe fn alloc(&self,pages:usize)->Option<(*mut c_void,u64)>
	{
		unsafe
		{
			let virt=noir_kmmap(pages);
			if virt.is_null()
			{
				None
			}
			else
			{
				Some((virt,noir_get_physical_address(virt)))
			}
		}
	}

	unsafe fn free(&self,virt:*mut c_void,pages:usize)
	{
		unsafe
		{
			noir_kmunmap(virt,pages);
		}
	}
}

impl<const N:usize,T:Sized> MemoryDescriptor<N,T>
{
	pub fn alloc()->Option<Self>
	{
		let a=InternalPageAllocator;
		unsafe
		{
			a.alloc(N).map(|(virt,phys)| Self::new(virt.cast(),phys))
		}
	}
}

impl<const N:usize,T:Sized,A:ContiguousAllocator> MemoryDescriptor<N,T,A>
{
	pub fn alloc_in(alloc:A)->Option<Self>
	{
		unsafe
		{
			alloc.alloc(N).map(|(virt,phys)| Self::new_in(virt.cast(),phys,alloc))
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