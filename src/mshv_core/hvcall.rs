/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines Microsoft TLFS hypercalls for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::*;

use paste::paste;

#[repr(C)] pub struct TlfsHypercallCode(pub u64);

impl TlfsHypercallCode
{
	build_int_mut_method!(call_code,0,16,u64);
	build_bit_mut_method!(fast,16);
	build_int_mut_method!(var_header_size,17,9,u64);
	build_bit_mut_method!(is_nested,26);
	build_int_mut_method!(rep_count,32,12,u64);
	build_int_mut_method!(rep_start_index,48,12,u64);
}