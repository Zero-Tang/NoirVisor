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

use bitfield_struct::bitfield;

#[bitfield(u64)] pub struct TlfsHypercallCode
{
	pub call_code:u16,
	pub fast:bool,
	#[bits(9)] pub var_header_size:u64,
	pub is_nested:bool,
	#[bits(5)] rsvd0:u64,
	#[bits(12)] rep_count:u64,
	#[bits(4)] rsvd1:u64,
	#[bits(12)] rep_start_index:u64,
	#[bits(4)] rsvd2:u64
}