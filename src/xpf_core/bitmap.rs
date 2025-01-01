/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file operates bitmaps in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

/// # `set_bitmap` function
/// This function sets a bit to 1 in a wide range of bitmap.
/// # Safety
/// Panics if the bit position exceeds the limit.
pub unsafe fn set_bitmap(bitmap:*mut c_void,limit:usize,bit_position:usize)
{
	assert!((bit_position>>3)<limit,"The set-bitmap operation exceeded the limit! Bit Position: {}, Limit: {} bytes",bit_position,limit);
	let bmp:*mut u32=bitmap.cast();
	let i=bit_position>>5;
	let j=bit_position&0x1F;
	*bmp.add(i)|=1<<j;
}

/// # `reset_bitmap` function
/// This function resets a bit to 0 in a wide range of bitmap.
/// # Safety
/// Panics if the bit position exceeds the limit.
pub unsafe fn reset_bitmap(bitmap:*mut c_void,limit:usize,bit_position:usize)
{
	assert!((bit_position>>3)<limit,"The reset-bitmap operation exceeded the limit! Bit Position: {}, Limit: {} bytes",bit_position,limit);
	let bmp:*mut u32=bitmap.cast();
	let i=bit_position>>5;
	let j=bit_position&0x1F;
	*bmp.add(i)&=!(1<<j);
}

/// # `test_bitmap` function
/// This function tests a bit in a wide range of bitmap.
/// # Safety
/// Panics if the bit position exceeds the limit.
pub unsafe fn test_bitmap(bitmap:*const c_void,limit:usize,bit_position:usize)->bool
{
	assert!((bit_position>>3)<limit,"The test-bitmap operation exceeded the limit! Bit Position: {}, Limit: {} bytes",bit_position,limit);
	let bmp:*const u32=bitmap.cast();
	let i=bit_position>>5;
	let j=bit_position&0x1F;
	bmp.add(i).read()&(1<<j)!=0
}