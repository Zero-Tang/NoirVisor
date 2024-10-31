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

use crate::{Status, NOIR_NOT_IMPLEMENTED, NOIR_SUCCESS};

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
		Ok(())
	}
}

pub trait DebuggerBackend
{
	/// # Safety
	/// The `buffer` argument is a raw pointer.
	unsafe fn read(self,buffer:*mut u8,length:usize)->bool;
	/// # Safety
	/// The `buffer` argument is a raw pointer.
	unsafe fn write(self,buffer:*const u8,length:usize)->bool;
}

pub enum Debugger
{
	QemuDebugCon(QemuDebugConDebugger),
	Unknown
}

pub static mut DEBUGGER:Debugger=Debugger::Unknown;

// Currently, interactive debugger is in draft-stage, so `debug_read` will never be called.
// Mark it as a piece of dead code.
#[allow(dead_code)]
unsafe fn debug_read(debugger:impl DebuggerBackend,buffer:*mut u8,length:usize)->bool
{
	debugger.read(buffer,length)
}

unsafe fn debug_write(debugger:impl DebuggerBackend,buffer:*const u8,length:usize)->bool
{
	debugger.write(buffer,length)
}

pub fn dbg_print(args: core::fmt::Arguments)
{
	let mut w=FormatBuffer::new();
	let r=fmt::write(&mut w, args);
	if r.is_ok()
	{
		unsafe
		{
			match DEBUGGER
			{
				Debugger::QemuDebugCon(item)=>
				{
					debug_write(item,w.buffer.as_ptr(),w.used);
				}
				Debugger::Unknown=>
				{
					// Do nothing.
				}
			}
		}
	}
}

#[no_mangle] pub unsafe extern "C" fn noir_debug_output(buffer:*const u8,length:usize)
{
	match DEBUGGER
	{
		Debugger::QemuDebugCon(item)=>
		{
			debug_write(item,buffer,length);
		}
		Debugger::Unknown=>
		{

		}
	};
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

#[no_mangle] pub extern "C" fn noir_configure_serial_port_debugger(_port_number:u8,_port_base:u16,_baud_rate:u32)->Status
{
	NOIR_NOT_IMPLEMENTED
}

#[no_mangle] pub extern "C" fn noir_configure_qemu_debug_console(port:u16)->Status
{
	unsafe
	{
		DEBUGGER=Debugger::QemuDebugCon(QemuDebugConDebugger::new(port));
	}
	println!("Internal Debugger is configured to QEMU ISA-DebugCon! Port=0x{:04X}",port);
	NOIR_SUCCESS
}