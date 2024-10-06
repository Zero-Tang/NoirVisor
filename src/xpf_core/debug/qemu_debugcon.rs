/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the QEMU ISA-DebugCon Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use super::DebuggerBackend;
use crate::xpf_core::asm::io::*;

#[derive(Clone, Copy)] pub struct QemuDebugConDebugger
{
	port_base:u16
}

impl DebuggerBackend for QemuDebugConDebugger
{
	unsafe fn read(self,buffer:*mut u8,length:usize)->bool
	{
		// QEMU ISA-DebugCon has a fixed readback state.
		// It is actually meaningless to implement read.
		for i in 0..length
		{
			*(buffer.add(i))=in_byte(self.port_base);
		}
		true
	}

	unsafe fn write(self,buffer:*const u8,length:usize)->bool
	{
		for i in 0..length
		{
			out_byte(self.port_base,*(buffer.add(i)));
		}
		true
	}
}

impl QemuDebugConDebugger
{
	pub fn new(port_base:u16)->QemuDebugConDebugger
	{
		QemuDebugConDebugger{port_base:port_base}
	}
}