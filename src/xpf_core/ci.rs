/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the CI (Code Integrity) Component of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::*;
use alloc::vec::Vec;

use crate::{println,print,dbg_print};

use super::nvbdk::{bytes_to_pages, noir_get_physical_address, page_mult, PAGE_SIZE};

static mut CI_PAGES:Vec<u64>=Vec::new();

pub fn enum_ci_phys_page()->*const Vec<u64>
{
	&raw const CI_PAGES
}

// We will use this routine to check if a page is protected in Code-Integrity.
pub fn is_ci_phys_page(phys:u64)->bool
{
	let mut lo:isize=0;
	// Use binary search to reduce running time complexity.
	let ci=&raw const CI_PAGES;
	let mut hi=unsafe{(*ci).len()-1} as isize;
	while hi>=lo
	{
		let mid=(lo+hi)>>1;
		let cur_page=unsafe{CI_PAGES[mid as usize]};
		if phys<cur_page
		{
			hi=mid-1;
		}
		else if phys>=cur_page+PAGE_SIZE as u64
		{
			lo=mid+1;
		}
		else
		{
			return true;
		}
	}
	false
}

/// # Safety
/// `noir_add_section_to_ci` must be called by C functions.
#[unsafe(no_mangle)] unsafe extern "C" fn noir_add_section_to_ci(base:*mut c_void,size:usize,_enable_scan:bool)->bool
{
	let page_num=bytes_to_pages(size);
	let ci=&raw mut CI_PAGES;
	for i in 0..page_num
	{
		let virt=((base as usize)+page_mult(i)) as *mut c_void;
		unsafe
		{
			let phys=noir_get_physical_address(virt) as u64;
			(*ci).push(phys);
		}
	}
	true
}

/// # Safety
/// `noir_activate_ci` must be called by C functions.
#[unsafe(no_mangle)] unsafe extern "C" fn noir_activate_ci()->bool
{
	let ci=&raw mut CI_PAGES;
	unsafe
	{
		// Sort the list since we will use binary search to confirm if a page belongs to CI.
		(*ci).sort();
	}
	true
}

#[unsafe(no_mangle)] extern "C" fn noir_initialize_ci(soft_ci:bool,hard_ci:bool)->bool
{
	if soft_ci
	{
		println!("Software-based Code-Integrity is deprecated!\nIgnoring software CI request...");
	}
	if !hard_ci
	{
		println!("Hardware-based Code-Integrity is required!");
		false
	}
	else
	{
		// There is nothing to do in Rust when we initialize CI.
		true
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_finalize_ci()
{
	// There is nothing to do in Rust when we finalize CI.
}