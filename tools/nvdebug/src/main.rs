#![feature(str_from_utf16_endian)]
use std::{env::*, net::TcpStream, slice};
use pe::{locate_symbol, RemotePEImage};
use windows::Win32::System::{Diagnostics::Debug::*,Threading::*};
use uefi_raw::{protocol::{device_path::*, loaded_image::LoadedImageProtocol}, table::{configuration::ConfigurationTable, system::SystemTable}, Guid, Handle};

use gdb::Target;

mod gdb;
mod pe;

const EFI_DEBUG_IMAGE_INFO_TABLE_GUID:Guid=Guid::new([0x77,0x2e,0x15,0x49],[0xda,0x1a],[0x64,0x47],0xb7,0xa2,[0x7a,0xfe,0xfe,0xd9,0x5e,0x8b]);

#[derive(Debug)]
#[repr(C)] struct DebugImageInfoTableHeader
{
	update_status:u32,
	table_size:u32,
	debug_image_info_table:*mut u64
}

#[derive(Debug)]
#[repr(C)] struct DebugImageInfoNormal
{
	image_info_type:u32,
	loaded_image:*mut LoadedImageProtocol,
	handle:Handle
}

#[derive(Eq, Ord, Debug)]
struct ImageInfo
{
	base:u64,
	length:u64,
	path:String
}

impl PartialEq for ImageInfo
{
	fn eq(&self, other: &Self) -> bool
	{
		self.base==other.base
	}
}

impl PartialOrd for ImageInfo
{
	fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering>
	{
		self.base.partial_cmp(&other.base)
	}
}

unsafe fn read_device_path(target:&mut Target,dev_path:*const DevicePathProtocol)->String
{
	macro_rules! read_mem_unwrapped
	{
		($addr:expr,$size:expr,$name:literal) =>
		{
			match target.read_memory($addr,$size)
			{
				Ok(raw)=>raw,
				Err(e)=>
				{
					println!("Failed to dump {}! Reason: {e}",$name);
					return String::new()
				}
			}
		};
	}
	let raw=read_mem_unwrapped!(dev_path as u64,size_of::<DevicePathProtocol>(),"EFI_DEVICE_PATH_PROTOCOL");
	let dp:*const DevicePathProtocol=raw.as_ptr().cast();
	let length=u16::from_le_bytes((*dp).length);
	match (*dp).major_type
	{
		DeviceType::MEDIA=>
		{
			match (*dp).sub_type
			{
				DeviceSubType::MEDIA_FILE_PATH=>
				{
					let raw=read_mem_unwrapped!(dev_path.add(1) as u64,length as usize - 4,"Media File Path");
					let s=String::from_utf16le_lossy(slice::from_raw_parts(raw.as_ptr(),length as usize - 4));
					return s;
				}
				DeviceSubType::MEDIA_PIWG_FIRMWARE_FILE=>
				{
					let raw=read_mem_unwrapped!(dev_path.add(1) as u64,length as usize - 4,"Media PIWG Firmware File");
					let guid=Guid::from_bytes(raw.as_slice().try_into().unwrap());
					let s=format!("PIWG Firmware: {}",String::from_utf8_unchecked(guid.to_ascii_hex_lower().to_vec()));
					return s;
				}
				DeviceSubType::MEDIA_RELATIVE_OFFSET_RANGE=>return String::new(),
				_=>println!("Unknown Device SubType: {:?}",(*dp).sub_type)
			}
		}
		_=>println!("Unknown Device Type: {:?}!",(*dp).major_type)
	}
	String::new()
}

fn enum_efi_images(target:&mut Target)->Vec<ImageInfo>
{
	let mut addr:u64=0;
	loop
	{
		let ret=target.read_memory(addr,8);
		if ret.is_err() {return Vec::new()}
		let sig=u64::from_le_bytes(ret.unwrap().as_slice().try_into().unwrap());
		if sig==0x5453595320494249
		{
			println!("Located EFI_SYSTEM_TABLE_POINTER at 0x{addr:016X}!");
			match target.read_memory(addr+8,8)
			{
				Ok(raw)=>
				{
					let base=u64::from_le_bytes(raw.as_slice().try_into().unwrap());
					println!("EFI_SYSTEM_TABLE is at 0x{base:016X}!");
					match target.read_memory(base,size_of::<SystemTable>())
					{
						Ok(raw)=>
						{
							
							let sys_table=raw.as_ptr() as *const SystemTable;
							unsafe
							{
								for i in 0..(*sys_table).number_of_configuration_table_entries
								{
									let p=(*sys_table).configuration_table.add(i);
									match target.read_memory(p as u64,size_of::<ConfigurationTable>())
									{
										Ok(raw)=>
										{
											let config_table=raw.as_ptr() as *const ConfigurationTable;
											if (*config_table).vendor_guid==EFI_DEBUG_IMAGE_INFO_TABLE_GUID
											{
												println!("Located EFI_DEBUG_IMAGE_INFO_TABLE_HEADER at {:p}!",(*config_table).vendor_table);
												match target.read_memory((*config_table).vendor_table as u64,size_of::<DebugImageInfoTableHeader>())
												{
													Ok(raw)=>
													{
														let header=raw.as_ptr() as *const DebugImageInfoTableHeader;
														let mut ptrs:Vec<u64>=Vec::with_capacity((*header).table_size as usize);
														let mut ret_vec:Vec<ImageInfo>=Vec::with_capacity((*header).table_size as usize);
														match target.read_memory((*header).debug_image_info_table as u64,((*header).table_size<<3) as usize)
														{
															Ok(raw)=>
															{
																for j in 0..(*header).table_size as usize
																{
																	ptrs.push(u64::from_le_bytes(raw[j<<3..(j<<3)+8].try_into().unwrap()));
																}
															}
															Err(e)=>println!("Failed to dump image table! Reason: {e}")
														}
														for p in ptrs
														{
															match target.read_memory(p,size_of::<DebugImageInfoNormal>())
															{
																Ok(raw)=>
																{
																	let info=raw.as_ptr() as *const DebugImageInfoNormal;
																	match target.read_memory((*info).loaded_image as u64,size_of::<LoadedImageProtocol>())
																	{
																		Ok(raw)=>
																		{
																			let image:*const LoadedImageProtocol=raw.as_ptr().cast();
																			let path=read_device_path(target,(*image).file_path);
																			ret_vec.push(ImageInfo{base:(*image).image_base as u64,length:(*image).image_size,path});
																		}
																		Err(e)=>println!("Failed to dump EFI_LOADED_IMAGE_PROTOCOL! Reason: {e}")
																	}
																}
																Err(e)=>println!("Failed to dump EFI_DEBUG_IMAGE_INFO_NORMAL! Reason: {e}")
															}
														}
														return ret_vec;
													}
													Err(e)=>println!("Failed to dump EFI_DEBUG_IMAGE_INFO_TABLE_HEADER! Reason: {e}")
												}
											}
										}
										Err(e)=>println!("Failed to dump EFI_CONFIGURATION_TABLE at index {i}! Reason: {e}")
									}
								}
							}
						}
						Err(e)=>
						{
							println!("Failed to dump EFI_SYSTEM_TABLE! Reason: {e}");
						}
					}
				}
				Err(e)=>panic!("Failed to read EfiSystemTableBase! Reason: {e}")
			}
		}
		addr+=4<<20;
	}
}

fn locate_module_from_ptr(images:&Vec<ImageInfo>,ptr:u64)->Option<&ImageInfo>
{
	let mut lo:isize=0;
	let mut hi=images.len() as isize;
	while hi>=lo
	{
		let mid=((lo+hi)>>1) as usize;
		if images[mid].base+images[mid].length<ptr
		{
			lo=(mid+1) as isize;
		}
		else if images[mid].base>=ptr
		{
			hi=(mid-1) as isize;
		}
		else
		{
			return Some(&images[mid]);
		}
	}
	None
}

fn main()
{
	if let Err(e)=unsafe{SymInitializeW(GetCurrentProcess(),None,false)}
	{
		panic!("SymInitializeW failed! Reason: {}",e);
	}
	let argv:Vec<String>=args().collect();
	let connection=argv.get(1);
	match argv.get(2)
	{
		Some(s)=>
		{
			let target_address=u64::from_str_radix(s.as_str(),16).unwrap();
			match connection
			{
				Some(conn)=>
				{
					if let Some(conn_str)=conn.strip_prefix("qemu://")
					{
						match TcpStream::connect(conn_str)
						{
							Ok(stream)=>
							{
								println!("Connected to QEMU GDB Session at {}!",conn_str);
								let mut target=Target::new(stream);
								let mut r=enum_efi_images(&mut target);
								r.sort();
								match locate_module_from_ptr(&r,target_address)
								{
									Some(img_info)=>
									{
										match RemotePEImage::new(&mut target,img_info.base,img_info.length,img_info.path.clone())
										{
											Some(mut pe_img)=>
											{
												pe_img.load_symbols();
												if let Some((sn,disp))=locate_symbol(target_address)
												{
													println!("Symbol: {}+{:X}",sn,disp);
												}
											}
											None=>println!("Failed to initialize PE Image!")
										}
									}
									_=>println!("[Test] Failed to locate!")
								}
							}
							Err(e)=>
							{
								println!("Failed to connect to QEMU GDB Session at {}! {}",conn_str,e);
							}
						}
					}
				}
				_=>println!("No connection method is specified!")
			}
		}
		None=>println!("Target address is not specified!")
	}
	let _=unsafe{SymCleanup(GetCurrentProcess())};
}
