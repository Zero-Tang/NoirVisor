/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the Serial Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::arch::asm;

use crate::xpf_core::asm::io::{in_byte,out_byte};
use super::DebuggerBackend;

const COM_PORT_OFFSET_DATA:u16=0;
const COM_PORT_OFFSET_INT_EN:u16=1;
const COM_PORT_OFFSET_BAUDRATE_LSB:u16=0;
const COM_PORT_OFFSET_BAUDRATE_MSB:u16=1;
const COM_PORT_OFFSET_INT_ID:u16=2;
const COM_PORT_OFFSET_FIFO_CTRL:u16=2;
const COM_PORT_OFFSET_LINE_CTRL:u16=3;
const COM_PORT_OFFSET_MODEM_CTRL:u16=4;
const COM_PORT_OFFSET_LINE_ST:u16=5;
const COM_PORT_OFFSET_MODEM_ST:u16=6;
const COM_PORT_OFFSET_SCRATCH:u16=7;

pub struct SerialPort
{
	// Basic Configuration
	port_base:u16
}

impl DebuggerBackend for SerialPort
{
	unsafe fn read(&self,buffer:*mut u8,length:usize)->bool
	{
		for i in 0..length
		{
			unsafe
			{
				buffer.add(i).write(self.read_byte());
			}
		}
		true
	}

	unsafe fn write(&self,buffer:*const u8,length:usize)->bool
	{
		for i in 0..length
		{
			unsafe
			{
				self.write_byte(buffer.add(i).read());
			}
		}
		true
	}
}

impl SerialPort
{
	pub fn new(port_base:u16,baud_rate:u32)->Option<Self>
	{
		let baud_remainder=115200%baud_rate;
		if baud_remainder!=0
		{
			return None;
		}
		let baud_quotient=115200/baud_rate;
		unsafe
		{
			// Disable all interrupts.
			out_byte(port_base+COM_PORT_OFFSET_INT_EN,0);
			// Enable DLAB to set up baud rate.
			out_byte(port_base+COM_PORT_OFFSET_LINE_CTRL,0x80);
			// Set baud rate. We need to use quotient.
			out_byte(port_base+COM_PORT_OFFSET_BAUDRATE_LSB,baud_quotient as u8);
			out_byte(port_base+COM_PORT_OFFSET_BAUDRATE_MSB,(baud_quotient>>8) as u8);
			// Disable DLAB. Use 8-bit data, no parity, one stop bit.
			out_byte(port_base+COM_PORT_OFFSET_LINE_CTRL,3);
			// Enable FIFO, clear them, and use 1-byte interrupt threshold.
			out_byte(port_base+COM_PORT_OFFSET_FIFO_CTRL,0x47);
			// Enable loopback mode.
			out_byte(port_base+COM_PORT_OFFSET_MODEM_CTRL,0x1E);
			// Test serial port.
			out_byte(port_base+COM_PORT_OFFSET_DATA,0xA5);
			if in_byte(port_base+COM_PORT_OFFSET_DATA)==0xA5
			{
				// Serial port is functional. Disable loopback.
				out_byte(port_base+COM_PORT_OFFSET_MODEM_CTRL,0x3);
				Some
				(
					Self
					{
						port_base
					}
				)
			}
			else
			{
				// Serial port is faulty.
				None
			}
		}
	}

	fn read_byte(&self)->u8
	{
		unsafe 
		{
			while (in_byte(self.port_base+COM_PORT_OFFSET_LINE_ST)&1)==1
			{
				asm!("pause");
			}
			in_byte(self.port_base+COM_PORT_OFFSET_DATA)
		}
	}

	fn write_byte(&self,val:u8)
	{
		unsafe
		{
			while (in_byte(self.port_base+COM_PORT_OFFSET_LINE_ST)&1)==1
			{
				asm!("pause");
			}
			out_byte(self.port_base+COM_PORT_OFFSET_DATA,val);
		}
	}
}