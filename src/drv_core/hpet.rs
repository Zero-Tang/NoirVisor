/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the High-Precision Event Timer Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::xpf_core::nvstatus::*;

#[allow(non_upper_case_globals)]
#[no_mangle] pub static mut hpet_period:u64=0;

#[no_mangle] pub extern "C" fn nvc_hpet_read_counter()->u64
{
	0
}

#[no_mangle] pub extern "C" fn nvc_hpet_initialize()->Status
{
	NOIR_NOT_IMPLEMENTED
}