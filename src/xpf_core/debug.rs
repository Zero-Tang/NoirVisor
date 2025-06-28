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
use spin::Mutex;
use core::{cell::LazyCell, fmt, str};
use alloc::boxed::Box;

use qemu_debugcon::*;
use serial::*;
use unknown::*;

use crate::xpf_core::nvstatus::{NOIR_SUCCESS,Status};

mod qemu_debugcon;
#[allow(dead_code)] mod serial;
mod unknown;

unsafe extern "C"
{
	/// ## `noir_system_debugger_write` Function
	/// This function will use the system's debugger to log outputs. \
	/// However, not all circumstances will allow usage of this debug output.
	/// 
	/// ## Safety
	/// The `maximum_length` specifies the maximum size of the `string`. \
	/// If the `string` contains a null-terminator `\0`, the output will stop there.
	fn noir_system_debugger_write(string:*const u8,maximum_length:usize);
}

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

pub trait DebuggerBackend:Send
{
	/// # Safety
	/// The `buffer` argument is a raw pointer.
	unsafe fn read(&self,buffer:*mut u8,length:usize)->bool;
	/// # Safety
	/// The `buffer` argument is a raw pointer.
	unsafe fn write(&self,buffer:*const u8,length:usize)->bool;
}

static DEBUGGER:Mutex<LazyCell<Box<dyn DebuggerBackend>>>=Mutex::new(
	LazyCell::new
	(
		|| match unsafe{DEBUGGER_CONFIG}
		{
			DebuggerConfig::QemuDebugCon(port)=>Box::new(QemuDebugConDebugger::new(port)),
			DebuggerConfig::Serial(port,baud_rate)=>Box::new(SerialPort::new(port,baud_rate).unwrap()),
			DebuggerConfig::Unknown=>Box::new(UnknownDebugger)
		}
	)
);

pub fn dbg_print(args: fmt::Arguments)
{
	let mut w=FormatBuffer::default();
	let r=fmt::write(&mut w, args);
	if r.is_ok()
	{
		unsafe
		{
			noir_debug_output(w.buffer.as_ptr(),w.used);
		}
	}
}

pub fn system_print(args: fmt::Arguments)
{
	let mut w=FormatBuffer::default();
	let r=fmt::write(&mut w,args);
	if r.is_ok()
	{
		unsafe 
		{
			noir_system_debugger_write(w.buffer.as_ptr(),w.used);
		}
	}
}

/// # Safety
/// Make sure `buffer` has the size of `length`.
#[unsafe(no_mangle)] unsafe extern "C" fn noir_debug_output(buffer:*const u8,length:usize)
{
	let d=DEBUGGER.lock();
	unsafe
	{
		d.write(buffer,length);
	}
}

/// # Safety
/// Make sure `buffer` has the size of `length` and is mutable.
#[unsafe(no_mangle)] unsafe extern "C" fn noir_debug_input(buffer:*mut u8,length:usize)
{
	let d=DEBUGGER.lock();
	unsafe
	{
		d.read(buffer,length);
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

#[macro_export] macro_rules! sysdprint
{
	($($arg:tt)*) =>
	{
		(system_print(format_args!($($arg)*)))
	};
}

#[macro_export] macro_rules! sysdprintln
{
	() =>
	{
		sysdprint("\n");
	};
	($($arg:tt)*) =>
	{
		sysdprint!("{}\n",format_args!($($arg)*))
	}
}

#[derive(Clone, Copy)]
enum DebuggerConfig
{
	QemuDebugCon(u16),
	Serial(u16,u32),
	Unknown
}

static mut DEBUGGER_CONFIG:DebuggerConfig=DebuggerConfig::Unknown;

#[unsafe(no_mangle)] extern "C" fn noir_configure_serial_port_debugger(_port_number:u8,port_base:u16,baud_rate:u32)->Status
{
	unsafe
	{
		DEBUGGER_CONFIG=DebuggerConfig::Serial(port_base,baud_rate);
	}
	println!("Internal Debugger is configured to Serial Port! Port=0x{:04X}",port_base);
	NOIR_SUCCESS
}

#[unsafe(no_mangle)] extern "C" fn noir_configure_qemu_debug_console(port:u16)->Status
{
	unsafe
	{
		DEBUGGER_CONFIG=DebuggerConfig::QemuDebugCon(port);
	}
	println!("Internal Debugger is configured to QEMU ISA-DebugCon! Port=0x{:04X}",port);
	NOIR_SUCCESS
}