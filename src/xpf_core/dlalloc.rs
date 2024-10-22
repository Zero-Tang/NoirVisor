/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file defines global allocator for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{alloc::{GlobalAlloc,Layout}, ffi::c_void};
use crate::{print,println,dbg_print};

struct DLMalloc;

extern "C"
{
	pub fn dlmalloc(length:usize)->*mut c_void;
	pub fn dlfree(ptr:*mut c_void)->();
	pub fn dlrealloc(ptr:*mut c_void,length:usize)->*mut c_void;
}

unsafe impl GlobalAlloc for DLMalloc
{
	unsafe fn alloc(&self, layout:Layout) -> *mut u8
	{
		println!("[alloc] length={}",layout.size());
		dlmalloc(layout.size()).cast()
	}

	unsafe fn dealloc(&self, ptr: *mut u8, _layout:Layout)
	{
		println!("[free] pointer=0x{:p}",ptr);
		dlfree(ptr.cast())
	}

	unsafe fn realloc(&self, ptr: *mut u8, _layout:Layout, new_size: usize) -> *mut u8
	{
		dlrealloc(ptr.cast(),new_size).cast()
	}
}

#[global_allocator] static GLOBAL_ALLOCATOR:DLMalloc=DLMalloc;