/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the Default Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use super::DebuggerBackend;

#[derive(Copy,Clone)] pub struct UnknownDebugger;

impl DebuggerBackend for UnknownDebugger
{
	unsafe fn read(self,_buffer:*mut u8,_length:usize)->bool
	{
		false
	}

	unsafe fn write(self,_buffer:*const u8,_length:usize)->bool
	{
		false
	}
}