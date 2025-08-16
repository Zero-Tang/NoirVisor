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

use crate::*;
use xpf_core::nvbdk::*;

pub struct CvmVcpuCache(pub u32);

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