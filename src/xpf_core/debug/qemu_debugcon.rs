/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the QEMU ISA-DebugCon Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{sync::atomic::{AtomicBool,Ordering},arch::asm};

use super::DebuggerBackend;
use crate::xpf_core::asm::io::*;

pub struct QemuDebugConDebugger
{
	port_base:u16,
	lock:AtomicBool
}

impl DebuggerBackend for QemuDebugConDebugger
{
	unsafe fn read(&self,buffer:*mut u8,length:usize)->bool
	{
		// QEMU ISA-DebugCon has a fixed readback state.
		// It is actually meaningless to implement read.
		for i in 0..length
		{
			*(buffer.add(i))=in_byte(self.port_base);
		}
		true
	}

	unsafe fn write(&self,buffer:*const u8,length:usize)->bool
	{
		for i in 0..length
		{
			out_byte(self.port_base,*(buffer.add(i)));
		}
		true
	}

	fn acquire(&mut self)
	{
		while self.lock.compare_exchange(false,true,Ordering::Acquire,Ordering::Relaxed).is_err()
		{
			while self.lock.load(Ordering::Relaxed)
			{
				unsafe
				{
					asm!("pause");
				}
			}
		}
	}

	fn release(&mut self)
	{
		self.lock.store(false,Ordering::Release);
	}
}

impl QemuDebugConDebugger
{
	pub fn new(port_base:u16)->QemuDebugConDebugger
	{
		Self
		{
			port_base,
			lock:false.into()
		}
	}
}