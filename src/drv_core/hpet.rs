/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the High-Precision Event Timer Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::xpf_core::nvstatus::*;

#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)] static mut hpet_period:u64=0;

#[unsafe(no_mangle)] extern "C" fn nvc_hpet_read_counter()->u64
{
	0
}

#[unsafe(no_mangle)] extern "C" fn nvc_hpet_initialize()->Status
{
	NOIR_NOT_IMPLEMENTED
}