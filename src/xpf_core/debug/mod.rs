/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the Debugger of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;
use log::*;
use spin::{Mutex, MutexGuard};

use nvcvm::status::Status;
use static_collections::{string::StaticString,format_static};

use core::{cell::LazyCell, fmt, ptr::null_mut, sync::atomic::{AtomicPtr, AtomicUsize, Ordering}};
use alloc::boxed::Box;

use qemu_debugcon::*;
use serial::*;
use unknown::*;

use crate::{println,sysdprintln};

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

/// Asynchronous Log Header
#[bitfield(u16)] struct LogHead
{
	#[bits(9)] length:usize,
	#[bits(4)] rsvd:u16,
	#[bits(3)] level:u8
}

#[allow(dead_code)]
impl LogHead
{
	fn as_str_ptr(&self)->*const u8
	{
		unsafe
		{
			(&raw const *self).add(1).cast()
		}
	}

	fn as_str_mut_ptr(&mut self)->*mut u8
	{
		unsafe
		{
			(&raw mut *self).add(1).cast()
		}
	}
}

struct InternalLogger
{
	pool:AtomicPtr<u8>,
	limit:AtomicUsize
}

impl InternalLogger
{
	const LEVEL_CHAR:[char;6]=[' ','E','W','I','D','T'];
	const LEVEL_COLOR:[u8;6]=[39,31,33,32,94,36];

	const fn new()->Self
	{
		Self
		{
			pool:AtomicPtr::new(null_mut()),
			limit:AtomicUsize::new(0)
		}
	}
}

impl Log for InternalLogger
{
	fn enabled(&self, _metadata: &Metadata) -> bool
	{
		true
	}

	fn flush(&self)
	{
		// We currently do not implement asynchronous logging.
		// Therefore, leave the implementation empty.
	}

	fn log(&self, record: &Record)
	{
		let level=InternalLogger::LEVEL_CHAR[record.level() as usize];
		let color=InternalLogger::LEVEL_COLOR[record.level() as usize];
		let mut x=format_static!(40,"{}:{}",record.file().unwrap_or("unknown-file"),record.line().unwrap_or(0)).unwrap();
		while x.len()<x.capacity()
		{
			let _=x.push(' ');
		}
		println!("\x1b[{color}m{x} |{level}|\x1b[39m {}",record.args());
	}
}

static INTERNAL_LOGGER:InternalLogger=InternalLogger::new();

#[unsafe(no_mangle)] extern "C" fn nvc_logger_initialize(level:u32)->bool
{
	let lv:Option<LevelFilter>=match level
	{
		0=>Some(LevelFilter::Off),
		1=>Some(LevelFilter::Error),
		2=>Some(LevelFilter::Warn),
		3=>Some(LevelFilter::Info),
		4=>Some(LevelFilter::Debug),
		5=>Some(LevelFilter::Trace),
		_=>None
	};
	let Some(level)=lv else
	{
		sysdprintln!("Logger level {level} is unknown! It must be between 0-5 (0 and 5 included)!");
		return false;
	};
	let r=set_logger(&INTERNAL_LOGGER);
	match &r
	{
		Ok(_)=>set_max_level(level),
		Err(e)=>sysdprintln!("Failed to set logger! Reason: {e}")
	};
	r.is_ok()
}

#[unsafe(no_mangle)] extern "C" fn nvc_logger_set_pool(pool_base:*mut u8,pool_limit:usize)
{
	INTERNAL_LOGGER.pool.store(pool_base,Ordering::Relaxed);
	INTERNAL_LOGGER.limit.store(pool_limit,Ordering::Relaxed);
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
			DebuggerConfig::Serial(port,baud_rate)=>match SerialPort::new(port,baud_rate)
			{
				Some(serial)=>Box::new(serial),
				None=>
				{
					sysdprintln!("Failed to initialize serial port console!");
					Box::new(UnknownDebugger)
				}
			}
			DebuggerConfig::Unknown=>Box::new(UnknownDebugger)
		}
	)
);

// Avoid copying the formatted string. Directly pass-thru to the debugger output.
struct FormattedDebugOutput(MutexGuard<'static,LazyCell<Box<dyn DebuggerBackend>>>);

impl fmt::Write for FormattedDebugOutput
{
	fn write_str(&mut self, s: &str) -> fmt::Result
	{
		let r=unsafe{self.0.write(s.as_ptr(),s.len())};
		if r
		{
			Ok(())
		}
		else
		{
			Err(fmt::Error)
		}
	}
}

pub fn dbg_print(args: fmt::Arguments)
{
	let mut w=FormattedDebugOutput(DEBUGGER.lock());
	unsafe
	{
		// Interrupts may cause mutex recursion, so disable interrupts.
		// However, NMIs might still cause recursion and cause deadlocks.
		use core::arch::asm;
		let old_rflags:usize;
		asm!
		(
			"pushfq",
			"pop {flags}",
			"cli",
			flags=out(reg) old_rflags
		);
		let _=fmt::write(&mut w,args);
		asm!
		(
			"push {flags}",
			"popfq",
			flags=in(reg) old_rflags
		);
	}
}

pub fn system_print(args: fmt::Arguments)
{
	let mut w:StaticString<512>=StaticString::new();
	let r=fmt::write(&mut w,args);
	if r.is_ok()
	{
		unsafe 
		{
			noir_system_debugger_write(w.as_bytes().as_ptr(),w.as_bytes().len());
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
		$crate::xpf_core::debug::dbg_print(format_args!($($arg)*))
	};
}

#[macro_export] macro_rules! println
{
	() =>
	{
		$crate::print!("\n")
	};
	($($arg:tt)*) =>
	{
		$crate::print!("{}\n",format_args!($($arg)*))
	};
}

#[macro_export] macro_rules! sysdprint
{
	($($arg:tt)*) =>
	{
		$crate::xpf_core::debug::system_print(format_args!($($arg)*))
	};
}

#[macro_export] macro_rules! sysdprintln
{
	() =>
	{
		$crate::sysdprint("\n");
	};
	($($arg:tt)*) =>
	{
		$crate::sysdprint!("{}\n",format_args!($($arg)*))
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
	Status::SUCCESS
}

#[unsafe(no_mangle)] extern "C" fn noir_configure_qemu_debug_console(port:u16)->Status
{
	unsafe
	{
		DEBUGGER_CONFIG=DebuggerConfig::QemuDebugCon(port);
	}
	println!("Internal Debugger is configured to QEMU ISA-DebugCon! Port=0x{:04X}",port);
	Status::SUCCESS
}