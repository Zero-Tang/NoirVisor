/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines x86 CVM of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use paste::paste;
use crate::*;
use xpf_core::nvbdk::*;

pub struct CvmVcpuCache(pub u32);

impl CvmVcpuCache
{
	build_bit_mut_method!(gpr,0);
	build_bit_mut_method!(cr,1);
	build_bit_mut_method!(cr2,2);
	build_bit_mut_method!(dr,3);
	build_bit_mut_method!(sr,4);
	build_bit_mut_method!(fg,5);
	build_bit_mut_method!(dt,6);
	build_bit_mut_method!(lt,7);
	build_bit_mut_method!(sc,8);
	build_bit_mut_method!(se,9);
	build_bit_mut_method!(tp,10);
	build_bit_mut_method!(ef,11);
	build_bit_mut_method!(pa,12);
	build_bit_mut_method!(lb,13);
	build_bit_mut_method!(ap,14);
	build_bit_mut_method!(ts,15);
	build_bit_mut_method!(ss,16);
	build_bit_mut_method!(synchronized,31);
}

pub struct CvmVcpu
{
	// Processor State
	pub gpr:GprState,
	pub seg:SegmentState,
	pub crs:CrState,
	pub drs:DrState,
	pub msrs:MsrState,
	pub xcrs:XcrState,
	pub rflags:u64,
	pub rip:u64,
	pub tsc_offset:u64,
	pub state_cache:CvmVcpuCache
}