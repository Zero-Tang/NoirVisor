/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2026, Zero Tang. All rights reserved.

  This file is the configuration manager on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).
*/

use core::{mem::MaybeUninit, ptr::null_mut, slice, sync::atomic::Ordering};

use r_efi::protocols::{file, simple_file_system};
use utf16_lit::utf16_null;

use crate::{host::{handle_protocol, IMAGE_INFO}, println};

#[repr(C)] struct RelativeAddress(u32);

impl RelativeAddress
{
	fn reference(&self)->&RawConfigurationRecord
	{
		let p:*const RawConfigurationRecord=(&raw const CONFIGURATION_BUFFER).cast();
		unsafe
		{
			&*p.byte_add(self.0 as usize)
		}
	}
}

impl PartialEq for RelativeAddress
{
	fn eq(&self, other: &Self) -> bool
	{
		self.reference().eq(other.reference())
	}
}

impl PartialOrd for RelativeAddress
{
	fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering>
	{
		self.reference().partial_cmp(other.reference())
	}
}

#[repr(C)] pub struct ConfigurationList
{
	record_count:u32,
	record_offsets:[RelativeAddress;0]
}

#[allow(static_mut_refs)]
impl ConfigurationList
{
	pub fn init()
	{
		let buffer=unsafe{CONFIGURATION_BUFFER.assume_init_mut()};
		buffer[..4].copy_from_slice(&[0;4]);
		match handle_protocol::<simple_file_system::Protocol>(unsafe{(*IMAGE_INFO.load(Ordering::Relaxed)).device_handle},simple_file_system::PROTOCOL_GUID)
		{
			Ok(file_sys_ptr)=>
			{
				let file_sys=unsafe{&*file_sys_ptr};
				let mut root_file_ptr:*mut file::Protocol=null_mut();
				let st=(file_sys.open_volume)(file_sys_ptr,&raw mut root_file_ptr);
				if st.is_error()
				{
					println!("OpenVolume failed! Status=0x{:X}",st.as_usize());
				}
				else
				{
					let root_file=unsafe{&*root_file_ptr};
					let mut cfg_file_ptr:*mut file::Protocol=null_mut();
					let mut cfg_file_path=utf16_null!("NoirVisorConfig.bin");
					let st=(root_file.open)(root_file_ptr,&raw mut cfg_file_ptr,cfg_file_path.as_mut_ptr(),file::MODE_READ,0);
					if st.is_error()
					{
						println!("OpenFile failed! Status=0x{:X}",st.as_usize());
					}
					else
					{
						let mut fsize:usize=buffer.len();
						let cfg_file=unsafe{&*cfg_file_ptr};
						let st=(cfg_file.read)(cfg_file_ptr,&raw mut fsize,buffer.as_mut_ptr().cast());
						if st.is_error()
						{
							println!("Failed to read config file! Status=0x{:X}, Needed size: {fsize}",st.as_usize());
						}
						else
						{
							println!("Successfully loaded configuration file! Size={fsize}");
							let configs=Self::ref_global();
							println!("Exists {} configuration records!",configs.record_count);
							//let records=unsafe{slice::from_raw_parts(configs.record_offsets.as_ptr(),configs.record_count as usize)};
							//for rec in records {println!("Record: {}",rec.reference().get_name());}
						}
					}
				}
			}
			Err(st)=>println!("HandleProtocol failed to resolve root device! Status=0x{:X}",st.as_usize())
		}
	}

	pub fn ref_global()->&'static ConfigurationList
	{
		unsafe
		{
			let buffer=CONFIGURATION_BUFFER.assume_init_mut();
			&*buffer.as_mut_ptr().cast()
		}
	}

	pub fn query<'a>(&self,name:&str)->Option<ConfigRecord<'a>>
	{
		let addresses=unsafe{slice::from_raw_parts(self.record_offsets.as_ptr(),self.record_count as usize)};
		match addresses.binary_search_by(|x| x.reference().get_name().cmp(name))
		{
			Ok(i)=>Some(addresses[i].reference().decode()),
			Err(_)=>None
		}
	}
}

static mut CONFIGURATION_BUFFER:MaybeUninit<[u8;512]>=MaybeUninit::uninit();

#[repr(C)] struct RawConfigurationRecord
{
	record_type:u32,
	name_len:u32,
	data_len:u32,
	name:[u8;0]
}

impl<'a> RawConfigurationRecord
{
	const STRING_TYPE:u32=0;
	const INTEGER_TYPE:u32=1;
	const BOOLEAN_TYPE:u32=2;

	fn get_name(&self)->&str
	{
		unsafe
		{
			str::from_utf8_unchecked(slice::from_raw_parts(self.name.as_ptr(),self.name_len as usize-1))
		}
	}

	fn decode(&self)->ConfigRecord<'a>
	{
		let data_offset=size_of::<Self>()+self.name_len as usize+if (self.name_len&3)!=0 {4-(self.name_len as usize&3)} else {0};
		let data:*const u8=unsafe{(self as *const Self).byte_add(data_offset).cast()};
		match self.record_type
		{
			Self::STRING_TYPE=>ConfigRecord::String(unsafe{str::from_utf8_unchecked(slice::from_raw_parts(data,self.data_len as usize-1))}),
			Self::INTEGER_TYPE=>ConfigRecord::Integer(unsafe{*data.cast()}),
			Self::BOOLEAN_TYPE=>ConfigRecord::Boolean(unsafe{*data.cast()}),
			_=>ConfigRecord::Unknown
		}
	}
}

impl PartialEq for RawConfigurationRecord
{
	fn eq(&self, other: &Self) -> bool
	{
		self.get_name()==other.get_name()
	}
}

impl PartialOrd for RawConfigurationRecord
{
	fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering>
	{
		self.get_name().partial_cmp(other.get_name())
	}
}

#[derive(Debug)]
pub enum ConfigRecord<'a>
{
	String(&'a str),
	Integer(u32),
	Boolean(bool),
	Unknown
}

impl<'a> ConfigRecord<'a>
{
	#[allow(unused)]
	pub fn unwrap_string(&'a self,default:&'a str)->&'a str
	{
		if let Self::String(s)=self
		{
			s
		}
		else
		{
			default
		}
	}

	pub fn unwrap_integer(&self,default:u32)->u32
	{
		if let Self::Integer(v)=self
		{
			*v
		}
		else
		{
			default
		}
	}

	pub fn unwrap_bool(&self,default:bool)->bool
	{
		if let Self::Boolean(b)=self
		{
			*b
		}
		else
		{
			default
		}
	}
}