/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file mocks CPU-related APIs for testing NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

use crate::xpf_core::nvbdk::BroadcastWorker;

#[unsafe(no_mangle)] extern "C" fn noir_get_processor_count()->u32
{
	1
}

#[unsafe(no_mangle)] extern "C" fn noir_generic_call(_worker:BroadcastWorker,_context:*mut c_void)
{

}