/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the CI (Code Integrity) Component of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::*;

#[no_mangle] pub extern "C" fn noir_add_section_to_ci(_base:*const c_void,_size:usize,_enable_scan:bool)->bool
{
	false
}

#[no_mangle] pub extern "C" fn noir_activate_ci()->bool
{
	false
}

#[no_mangle] pub extern "C" fn noir_initialize_ci(_soft_ci:bool,_hard_ci:bool)->bool
{
	false
}

#[no_mangle] pub extern "C" fn noir_finalize_ci()->()
{

}