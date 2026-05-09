/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file mocks C/ASM APIs for testing NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, ptr::null_mut};

extern crate std;

mod cpu;
mod memory;
pub mod svm_hv;

#[unsafe(no_mangle)] extern "C" fn nvc_store_image_info(_base:*mut *mut c_void,_size:*mut u32)
{

}

#[unsafe(no_mangle)] extern "C" fn noir_locate_acpi_rsdt(_length:*mut usize)->*mut c_void
{
	null_mut()
}

#[unsafe(no_mangle)] extern "C" fn noir_query_enabled_features_in_system()->i64
{
	0
}

#[unsafe(no_mangle)] extern "C" fn noir_system_debugger_write(_string:*const u8,_maximum_length:usize)
{

}