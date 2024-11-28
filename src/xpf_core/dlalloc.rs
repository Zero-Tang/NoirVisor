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

use portable_dlmalloc::{DLMalloc,raw::*};

#[global_allocator] static GLOBAL_ALLOCATOR:DLMalloc=DLMalloc;

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