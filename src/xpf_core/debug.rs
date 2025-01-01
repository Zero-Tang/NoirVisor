/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the Debugger of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use iced_x86::*;
use core::{fmt, str};

use qemu_debugcon::*;

use crate::{Status, NOIR_NOT_IMPLEMENTED, NOIR_SUCCESS};

mod qemu_debugcon;
mod unknown;

// We need to implement a formatter without alloc!
pub struct FormatBuffer
{
	buffer:[u8;512],
	used:usize
}

impl FormatBuffer
{
	pub fn as_str(&self)->&str
	{
		unsafe
		{
			str::from_utf8_unchecked(&self.buffer[..self.used])
		}
	}
}

impl Default for FormatBuffer
{
	fn default() -> Self
	{
		Self
		{
			buffer:[0;512],
			used:0
		}
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

impl FormatterOutput for FormatBuffer
{
	fn write(&mut self, text: &str, _kind: FormatterTextKind)
	{
		let b=text.as_bytes();
		self.buffer[self.used..self.used+b.len()].copy_from_slice(b);
		self.used+=b.len();
	}
}

pub trait DebuggerBackend
{
	/// # Safety
	/// The `buffer` argument is a raw pointer.
	unsafe fn read(&self,buffer:*mut u8,length:usize)->bool;
	/// # Safety
	/// The `buffer` argument is a raw pointer.
	unsafe fn write(&self,buffer:*const u8,length:usize)->bool;
	fn acquire(&mut self);
	fn release(&mut self);
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
unsafe fn debug_read(debugger:&mut impl DebuggerBackend,buffer:*mut u8,length:usize)->bool
{
	debugger.acquire();
	let b=debugger.read(buffer,length);
	debugger.release();
	b
}

unsafe fn debug_write(debugger:&mut impl DebuggerBackend,buffer:*const u8,length:usize)->bool
{
	debugger.acquire();
	let b=debugger.write(buffer,length);
	debugger.release();
	b
}

pub fn dbg_print(args: core::fmt::Arguments)
{
	let mut w=FormatBuffer::default();
	let r=fmt::write(&mut w, args);
	if r.is_ok()
	{
		let dbg=&raw mut DEBUGGER;
		unsafe
		{
			match &mut *dbg
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

/// # Safety
/// This function is intended to be called from C codes of NoirVisor.
#[no_mangle] pub unsafe extern "C" fn noir_debug_output(buffer:*const u8,length:usize)
{
	let dbg=&raw mut DEBUGGER;
	match &mut *dbg
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