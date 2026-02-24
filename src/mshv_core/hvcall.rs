/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines Microsoft TLFS hypercalls for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;

// Currently, we don't support hypercall, so let's avoid hypercall VM-Exit.
pub(super) static MSHV_HYPERCALL_CODE64:[u8;6]=
[
	0xB8,0x02,0x00,0x00,0x00,	// mov eax,HvStatus::INVALID_HYPERCALL_CODE
	0xC3						// ret
];

pub(super) static MSHV_HYPERCALL_CODE32:[u8;8]=
[
	0xB8,0x02,0x00,0x00,0x00,	// mov eax,HvStatus::INVALID_HYPERCALL_CODE
	0x33,0xD2,					// xor edx,edx
	0xC3						// ret
];

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