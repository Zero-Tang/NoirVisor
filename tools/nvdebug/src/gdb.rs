// GDB Protocol

use core::str;
use std::{fmt::Display, io::{Error, Read, Write}, net::TcpStream, num::ParseIntError};

#[derive(Debug)]
pub enum GdbError
{
	Io(Error),
	Gnu(u8)
}

impl Display for GdbError
{
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result
	{
		match self
		{
			Self::Io(e)=>write!(f,"I/O Fault: {}",e),
			Self::Gnu(e)=>write!(f," GNU Errno: {}",e)
		}
	}
}

pub struct Target
{
	stream:TcpStream
}

impl Target
{
	pub fn new(stream:TcpStream)->Self
	{
		Self{stream}
	}

	pub fn read_memory(&mut self,address:u64,length:usize)->Result<Vec<u8>,GdbError>
	{
		let raw_packet=create_packet(&format!("m {:016X},{:x}",address,length));
		let _=self.stream.write_all(&raw_packet);
		let mut raw_response:Vec<u8>=vec![0;(length<<1)+4];
		let r=self.stream.read(raw_response.as_mut_slice());
		match r
		{
			Ok(s)=>
			{
				if s==1 && raw_response[0]==b'+'
				{
					// Acknowledgement.
					let _=self.stream.read(&mut raw_response);
				}
			}
			Err(e)=>return Err(GdbError::Io(e))
		}
		let raw_pkt=get_packet_data(&raw_response);
		match raw_pkt
		{
			Some(s)=>
			{
				let raw_str=unsafe{str::from_utf8_unchecked(s)};
				if raw_str.chars().nth(0)==Some('E')
				{
					Err(GdbError::Gnu(u8::from_str_radix(&raw_str[1..3],16).unwrap()))
				}
				else
				{
					let resp:Result<Vec<u8>,ParseIntError>=(0..length<<1).step_by(2).map(|i| u8::from_str_radix(&raw_str[i..i+2],16)).collect();
					Ok(resp.unwrap())
				}

			}
			_=>panic!("Invalid Checksum!")
		}
	}
}

pub fn calculate_checksum(raw:&[u8])->u8
{
	let mut csum:u8=0;
	for b in raw
	{
		csum=csum.wrapping_add(*b);
	}
	csum
}

pub fn create_packet(raw:&String)->Vec<u8>
{
	let mut r=vec![b'$'];
	r.extend_from_slice(raw.as_bytes());
	r.push(b'#');
	let csum=calculate_checksum(raw.as_bytes());
	let csum_str=format!("{:02X}",csum);
	r.extend_from_slice(csum_str.as_bytes());
	r
}

pub fn get_packet_data(raw:&Vec<u8>)->Option<&[u8]>
{
	if raw[0]==b'$'
	{
		match raw.iter().position(|&r| r==b'#')
		{
			Some(index)=>
			{
				let raw_pkt=&raw[1..index];
				let csum_lhs=calculate_checksum(raw_pkt);
				let csum_rhs:u8=u8::from_str_radix(unsafe{str::from_utf8_unchecked(&raw[index+1..index+3])},16).unwrap();
				if csum_lhs==csum_rhs
				{
					Some(raw_pkt)
				}
				else
				{
					println!("Invalid Checksum! Left=0x{:02X}, Right=0x{:02X}",csum_lhs,csum_rhs);
					None
				}
			}
			None=>
			{
				println!("Invalid packet! Raw Data: {raw:?}");
				None
			}
		}
	}
	else
	{
		println!("Invalid packet! Raw Data: {raw:?}");
		None
	}
}