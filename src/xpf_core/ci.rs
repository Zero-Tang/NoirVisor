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

use core::{ffi::*, slice};
use alloc::vec::Vec;
use spin::RwLock;

use crate::{dbg_print, print, println, xpf_core::nvbdk::page_4kb_base};

use super::nvbdk::{bytes_to_pages, noir_get_physical_address, page_mult};

pub struct CiManager
{
	pages:Vec<u64>
}

impl CiManager
{
	fn add_page(&mut self,phys:u64)
	{
		self.pages.push(phys);
	}

	fn activate(&mut self)
	{
		self.pages.sort();
	}

	fn in_ci(&self,phys:u64)->bool
	{
		let phys=page_4kb_base(phys);
		self.pages.binary_search(&phys).is_ok()
	}

	const fn new()->Self
	{
		Self
		{
			pages:Vec::new()
		}
	}
}

impl<'a> IntoIterator for &'a CiManager
{
	type Item = &'a u64;
	type IntoIter = slice::Iter<'a,u64>;
	fn into_iter(self) -> Self::IntoIter
	{
		self.pages.iter()
	}
}

pub static CI_MANAGER:RwLock<CiManager>=RwLock::new(CiManager::new());

// We will use this routine to check if a page is protected in Code-Integrity.
pub fn is_ci_phys_page(phys:u64)->bool
{
	CI_MANAGER.read().in_ci(phys)
}

/// # Safety
/// `noir_add_section_to_ci` must be called by C functions.
#[unsafe(no_mangle)] unsafe extern "C" fn noir_add_section_to_ci(base:*mut c_void,size:usize,_enable_scan:bool)->bool
{
	let page_num=bytes_to_pages(size);
	let mut ci=CI_MANAGER.write();
	for i in 0..page_num
	{
		let virt=((base as usize)+page_mult(i)) as *mut c_void;
		unsafe
		{
			let phys=noir_get_physical_address(virt) as u64;
			ci.add_page(phys);
		}
	}
	true
}

/// # Safety
/// `noir_activate_ci` must be called by C functions.
#[unsafe(no_mangle)] unsafe extern "C" fn noir_activate_ci()->bool
{
	CI_MANAGER.write().activate();
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