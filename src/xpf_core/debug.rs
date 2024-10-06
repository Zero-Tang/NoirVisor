/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the Debugger of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */


use core::fmt;

use qemu_debugcon::*;

mod qemu_debugcon;
mod unknown;

// We need to implement a formatter without alloc!
struct FormatBuffer
{
	buffer:[u8;512],
	used:usize
}

impl FormatBuffer
{
	fn new()->Self
	{
		FormatBuffer{buffer:[0;512],used:0}
	}
}

impl fmt::Write for FormatBuffer
{
	fn write_str(&mut self, s: &str) -> fmt::Result
	{
		let remainder=&mut self.buffer[self.used..];
		let current=s.as_bytes();
		if remainder.len()<current.len()
		{
			return Err(fmt::Error);
		}
		remainder[..current.len()].copy_from_slice(current);
		self.used+=current.len();
		return Ok(());
	}
}
pub trait DebuggerBackend
{
	unsafe fn read(self,buffer:*mut u8,length:usize)->bool;
	unsafe fn write(self,buffer:*const u8,length:usize)->bool;
}

pub unsafe fn debug_read(debugger:impl DebuggerBackend,buffer:*mut u8,length:usize)->bool
{
	debugger.read(buffer,length)
}

pub unsafe fn debug_write(debugger:impl DebuggerBackend,buffer:*const u8,length:usize)->bool
{
	debugger.write(buffer,length)
}

pub fn dbg_print(args: core::fmt::Arguments)
{
	let mut w=FormatBuffer::new();
	let r=fmt::write(&mut w, args);
	if let Ok(_)=r
	{
		unsafe
		{
			debug_write(QemuDebugConDebugger::new(0x402),w.buffer.as_ptr(), w.used);
		}
	}
}

#[macro_export] macro_rules! print
{
	($($arg:tt)*) =>
	{
		(dbg_print(format_args!($($arg)*)))		
	};
}

#[macro_export] macro_rules! println
{
	() =>
	{
		print!("\n")
	};
	($($arg:tt)*) =>
	{
		print!("{}\n",format_args!($($arg)*))
	};
}