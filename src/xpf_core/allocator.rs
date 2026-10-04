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

use core::{
	ffi::c_void,
	fmt::{self, Display},
	hint::cold_path,
	sync::atomic::*,
};

use log::info;
use portable_dlmalloc::raw::*;
use spin::{Mutex, MutexGuard};
use static_collections::{bitmap::RefBitmap, vec::StaticVec};

use super::nvbdk::*;
use crate::sysdprintln;

static CHECK_ALLOC: AtomicBool = AtomicBool::new(false);

// Minimum chunk for malloc is (1<<6)=64 pages (256KiB).
const MALLOC_CHUNK_SHIFT: usize = 6;

pub fn set_alloc_checker(v: bool)
{
	CHECK_ALLOC.store(v, Ordering::SeqCst);
}

mod dlmalloc
{
	use core::{
		alloc::*,
		ffi::c_void,
		hint::spin_loop,
		ptr::null_mut,
		sync::atomic::{AtomicUsize, Ordering},
	};

	use portable_dlmalloc::DLMalloc;

	use super::{CHECK_ALLOC, PAGE_ALLOC_MANAGER};
	use crate::{
		sysdprintln,
		xpf_core::nvbdk::{nulstr_from_ptr, page_count},
	};

	struct InternalAllocator;

	// Re-implement the allocator in order to intercept global allocations.
	unsafe impl GlobalAlloc for InternalAllocator
	{
		unsafe fn alloc(&self, layout: Layout) -> *mut u8
		{
			unsafe {
				if CHECK_ALLOC.load(Ordering::SeqCst)
				{
					panic!("Intercepted unwanted allocation! Alignment: {} bytes. Size: {} bytes.", layout.align(), layout.size());
				}
				DLMALLOC.alloc(layout)
			}
		}

		unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout)
		{
			unsafe {
				DLMALLOC.dealloc(ptr, layout);
			}
		}

		unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8
		{
			unsafe { DLMALLOC.realloc(ptr, layout, new_size) }
		}
	}

	#[global_allocator]
	static GLOBAL_ALLOCATOR: InternalAllocator = InternalAllocator;
	static DLMALLOC: DLMalloc = DLMalloc;

	#[unsafe(no_mangle)]
	unsafe extern "C" fn custom_mmap(length: usize) -> *mut c_void
	{
		let mut lk = PAGE_ALLOC_MANAGER.lock();
		match lk.alloc_pages_for_malloc(page_count(length))
		{
			Some(p) => p,
			None =>
			unsafe { null_mut::<c_void>().byte_sub(1) },
		}
	}

	#[unsafe(no_mangle)]
	unsafe extern "C" fn custom_munmap(ptr: *mut c_void, length: usize) -> i32
	{
		sysdprintln!("[munmap] ptr: {ptr:p}, size: 0x{length:X}");
		let mut lk = PAGE_ALLOC_MANAGER.lock();
		lk.free_pages(ptr, page_count(length));
		0
	}

	#[unsafe(no_mangle)]
	unsafe extern "C" fn init_lock(lock: *mut usize)
	{
		unsafe {
			*lock = 0;
		}
	}

	#[unsafe(no_mangle)]
	unsafe extern "C" fn final_lock(_lock: *mut usize)
	{
		// DO NOTHING SINCE SPIN-LOCK DOESN'T NEED FINALIZATION.
	}

	#[unsafe(no_mangle)]
	unsafe extern "C" fn acquire_lock(lock: *mut usize)
	{
		let p = unsafe { AtomicUsize::from_ptr(lock) };
		// Use a TTAS Spin-Lock.
		while p.compare_exchange(0, 1, Ordering::Acquire, Ordering::Relaxed).is_err()
		{
			while p.load(Ordering::Relaxed) == 1
			{
				// The pause instruction is intended to optimize spin-locks.
				spin_loop();
			}
		}
	}

	#[unsafe(no_mangle)]
	unsafe extern "C" fn release_lock(lock: *mut usize)
	{
		let p = unsafe { AtomicUsize::from_ptr(lock) };
		p.store(0, Ordering::Release);
	}

	#[unsafe(no_mangle)]
	unsafe extern "C" fn custom_abort(message: *const u8, src_file: *const u8, src_line: u32) -> !
	{
		unsafe {
			let msg = nulstr_from_ptr(message);
			let sfn = nulstr_from_ptr(src_file);
			panic!("DLMalloc aborted at {sfn}:{src_line}! Reason: {msg}");
		}
	}
}

pub fn get_used() -> usize
{
	unsafe { dlmallinfo().uordblks }
}

pub fn get_free() -> usize
{
	unsafe { dlmallinfo().fordblks }
}

struct PageAllocationInformation
{
	descriptor: MemoryDescriptor<PAGE_TABLE_ENTRIES64, c_void>,
	bitmap: [u64; 8],
}

impl Display for PageAllocationInformation
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> fmt::Result
	{
		write!(f, "Virt: {:p}, Phys: 0x{:016X}, Bitmap: ", self.descriptor.virt, self.descriptor.phys)?;
		for x in self.bitmap
		{
			write!(f, "{x:016X}")?;
		}
		Ok(())
	}
}

impl PageAllocationInformation
{
	fn new() -> Option<Self>
	{
		let virt = unsafe { noir_alloc_2mb_page() };
		if virt.is_null()
		{
			None
		}
		else
		{
			Some(Self { descriptor: unsafe { MemoryDescriptor::new(virt, noir_get_physical_address(virt)) }, bitmap: [0; 8] })
		}
	}

	fn alloc_pages(&mut self, pages: usize) -> Option<(*mut c_void, u64)>
	{
		let bmp: &mut RefBitmap<PAGE_TABLE_ENTRIES64> = unsafe { RefBitmap::from_raw_mut_ptr(self.bitmap.as_mut_ptr().cast()) };
		let mut i: usize = 0;
		while i < PAGE_TABLE_ENTRIES64
		{
			if !bmp.test(i).unwrap()
			{
				let mut is_free = true;
				for j in i + 1..i + pages
				{
					if j == PAGE_TABLE_ENTRIES64
					{
						is_free = false;
						break;
					}
					if bmp.test(j).unwrap()
					{
						is_free = false;
						break;
					}
				}
				if is_free
				{
					unsafe {
						let s = self.descriptor.virt.byte_add(page_mult(i));
						// Set the bitmap.
						for j in i..i + pages
						{
							let _ = bmp.set(j);
						}
						// println!("[alloc] Allocated Page {s:p} to {:p}! Allocation Info: {self}",s.byte_add(pages));
						return Some((s, noir_get_physical_address(s)));
					}
				}
			}
			i += 1;
		}
		None
	}

	fn alloc_pages_for_malloc(&mut self, pages: usize) -> Option<*mut c_void>
	{
		// 256KiB-per-chunk
		let chunks = pages >> MALLOC_CHUNK_SHIFT;
		sysdprintln!("Allocating {chunks} chunks for dlmalloc...");
		for i in (0..=8 - chunks).rev()
		{
			let info = &mut self.bitmap;
			// Search for a free chunk.
			if info[i] == 0
			{
				let mut is_free = true;
				let chunks = if i < chunks { i } else { chunks };
				for j in 0..chunks
				{
					if info[i - j] != 0
					{
						is_free = false;
						break;
					}
				}
				if is_free
				{
					for j in 0..chunks
					{
						info[i - j] = u64::MAX;
					}
					let p: *mut c_void = unsafe { self.descriptor.virt.byte_add(page_mult(i - chunks + 1) << MALLOC_CHUNK_SHIFT) };
					sysdprintln!("malloc-chunk: {p:p}");
					return Some(p);
				}
			}
		}
		None
	}

	fn compare_pa(&self, pa: u64) -> core::cmp::Ordering
	{
		use core::cmp::Ordering;
		if pa < self.descriptor.phys
		{
			Ordering::Greater
		}
		else if pa >= self.descriptor.phys + PAGE_2MB_SIZE as u64
		{
			Ordering::Less
		}
		else
		{
			Ordering::Equal
		}
	}
}

pub struct PageAllocationManager
{
	list: StaticVec<64, PageAllocationInformation>,
}

pub struct PageAllocIter<'a>
{
	index: usize,
	source: &'a PageAllocationManager,
}

impl<'a> Iterator for PageAllocIter<'a>
{
	type Item = u64;

	fn next(&mut self) -> Option<Self::Item>
	{
		let i = self.index;
		self.index += 1;
		self.source.list.get(i).map(|x| x.descriptor.phys)
	}
}

impl PageAllocationManager
{
	pub fn iter(&self) -> PageAllocIter<'_>
	{
		PageAllocIter { index: 0, source: self }
	}

	fn new_blank(&mut self) -> usize
	{
		if self.list.len() == self.list.capacity()
		{
			panic!("NoirVisor has exceeded 128MiB Internal allocation limit!");
		}
		let info = PageAllocationInformation::new().expect("Failed to allocate new 2MiB Page!");
		match self.list.binary_search_by(|x| x.descriptor.phys.cmp(&info.descriptor.phys))
		{
			Ok(i) =>
			{
				cold_path();
				panic!(
					"System allocator returned physical address 0x{:X}. However, it's already allocated at index {i}!",
					info.descriptor.phys
				);
			}
			Err(i) =>
			{
				self.list.insert(i, info);
				i
			}
		}
	}

	fn new_full(&mut self) -> usize
	{
		if self.list.len() == self.list.capacity()
		{
			panic!("NoirVisor has exceeded 128MiB Internal allocation limit!");
		}
		let mut info = PageAllocationInformation::new().expect("Failed to allocate new 2MiB Page!");
		info.bitmap = [u64::MAX; 8];
		match self.list.binary_search_by(|x| x.descriptor.phys.cmp(&info.descriptor.phys))
		{
			Ok(i) =>
			{
				cold_path();
				panic!(
					"System allocator returned physical address 0x{:X}. However, it's already allocated at index {i}!",
					info.descriptor.phys
				);
			}
			Err(i) =>
			{
				self.list.insert(i, info);
				i
			}
		}
	}

	fn alloc_pages(&mut self, pages: usize) -> Option<(*mut c_void, u64)>
	{
		// Try to allocate pages from existing large pages.
		for x in self.list.iter_mut()
		{
			if let Some(md) = x.alloc_pages(pages)
			{
				return Some(md);
			}
		}
		// At this point, we need to allocate new large pages!
		self.new_blank();
		self.alloc_pages(pages)
	}

	fn alloc_pages_for_malloc(&mut self, pages: usize) -> Option<*mut c_void>
	{
		sysdprintln!("Trying to allocate {pages} pages for dlmalloc chunk...");
		// Try to allocate pages from existing large pages.
		for x in self.list.iter_mut()
		{
			if let Some(md) = x.alloc_pages_for_malloc(pages)
			{
				sysdprintln!("Allocated {md:p} for dlmalloc chunk!");
				return Some(md);
			}
		}
		// At this point, we need to allocate new large pages!
		self.new_blank();
		self.alloc_pages_for_malloc(pages)
	}

	fn free_pages(&mut self, virt: *mut c_void, pages: usize)
	{
		// Search for large pages.
		for x in self.list.iter_mut()
		{
			unsafe {
				let v = x.descriptor.virt;
				if virt >= v && virt < v.byte_add(PAGE_2MB_SIZE)
				{
					let bmp: &mut RefBitmap<64> = RefBitmap::from_raw_mut_ptr(x.bitmap.as_mut_ptr().cast());
					let start = page_count(virt.offset_from(v) as usize);
					for j in start..start + pages
					{
						let _ = bmp.reset(j);
					}
					break;
				}
			}
		}
	}

	fn free_large_page(&mut self, virt: *mut c_void)
	{
		// Search for large pages.
		for x in self.list.iter_mut()
		{
			if core::ptr::eq(virt, x.descriptor.virt.cast())
			{
				info!("Freeing large page from valid-entry at {virt:p}...");
			}
		}
	}

	const fn empty() -> Self
	{
		Self { list: StaticVec::new() }
	}
}

pub static PAGE_ALLOC_MANAGER: Mutex<PageAllocationManager> = Mutex::new(PageAllocationManager::empty());

pub fn is_allocated_page(addr: u64) -> bool
{
	let lk = PAGE_ALLOC_MANAGER.lock();
	lk.list.binary_search_by(|x| x.compare_pa(addr)).is_ok()
}

pub fn print_allocation()
{
	let lk = PAGE_ALLOC_MANAGER.lock();
	for (i, x) in lk.list.iter().enumerate()
	{
		sysdprintln!("Large Page {i}: {x}");
	}
}

pub fn get_large_page_count() -> usize
{
	let lk = PAGE_ALLOC_MANAGER.lock();
	lk.list.len()
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
	unsafe fn alloc(&self, pages: usize) -> Option<(*mut c_void, u64)>;
	/// # The `free` method
	/// Free `pages` of memory in page-granularity.
	///
	/// # Safety
	/// It is your duty to guarantee the release procedure is safe.
	unsafe fn free(&self, virt: *mut c_void, pages: usize);
}

pub struct InternalPageAllocator;

unsafe impl ContiguousAllocator for InternalPageAllocator
{
	unsafe fn alloc(&self, pages: usize) -> Option<(*mut c_void, u64)>
	{
		let mut lk = PAGE_ALLOC_MANAGER.lock();
		lk.alloc_pages(pages)
	}

	unsafe fn free(&self, virt: *mut c_void, pages: usize)
	{
		let mut lk = PAGE_ALLOC_MANAGER.lock();
		if pages == PAGE_TABLE_ENTRIES64
		{
			// This is probably 2MiB page.
			lk.free_large_page(virt);
			info!("Freeing Large Page at {virt:p}...");
		}
		else
		{
			// This is normal contiguous page.
			lk.free_pages(virt, pages);
			info!("Freeing Page at {virt:p}...");
		}
	}
}

impl<const N: usize, T: Sized> MemoryDescriptor<N, T>
{
	pub fn alloc() -> Option<Self>
	{
		let a = InternalPageAllocator;
		unsafe { a.alloc(N).map(|(virt, phys)| Self::new(virt.cast(), phys)) }
	}

	pub fn alloc_with_lock(lk: &mut MutexGuard<'_, PageAllocationManager>) -> Option<Self>
	{
		unsafe { lk.alloc_pages(N).map(|(virt, phys)| Self::new(virt.cast(), phys)) }
	}
}

impl<const N: usize, T: Sized, A: ContiguousAllocator> MemoryDescriptor<N, T, A>
{
	pub fn alloc_in(alloc: A) -> Option<Self>
	{
		unsafe { alloc.alloc(N).map(|(virt, phys)| Self::new_in(virt.cast(), phys, alloc)) }
	}
}

impl<T: Sized> MemoryDescriptor<PAGE_TABLE_ENTRIES64, T>
{
	pub fn alloc_2mb_page() -> Option<Self>
	{
		let mut lk = PAGE_ALLOC_MANAGER.lock();
		let i = lk.new_full();
		Some(unsafe { MemoryDescriptor::new(lk.list[i].descriptor.virt.cast(), lk.list[i].descriptor.phys) })
	}
}
