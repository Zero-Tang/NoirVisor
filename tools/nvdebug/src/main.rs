#![feature(str_from_utf16_endian)]
use std::{env::*, ffi::c_void, net::TcpStream, ptr::null_mut, slice};
use pe::{locate_symbol, RemotePEImage};
use windows::Win32::{Foundation::*, System::{Diagnostics::Debug::*,Threading::*,SystemInformation::*}};
use uefi_raw::{protocol::{device_path::*, loaded_image::LoadedImageProtocol}, table::{configuration::ConfigurationTable, system::SystemTable}, Guid, Handle};
use iced_x86::*;

use gdb::{registers::x64::QemuGdbX64Registers, Target};

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

#[derive(Eq, Debug)]
struct ImageInfo
{
	base:u64,
	length:u64,
	path:String
}

#[repr(C)] struct SymProcessHandle
{
	target:*mut Target,
	images:*const ImageInfo,
	count:usize,
	fpo:IMAGE_RUNTIME_FUNCTION_ENTRY
}

impl PartialEq for ImageInfo
{
	fn eq(&self, other: &Self) -> bool
	{
		self.base==other.base
	}
}

impl Ord for ImageInfo
{
	fn cmp(&self, other: &Self) -> std::cmp::Ordering
	{
		self.base.cmp(&other.base)
	}
}

impl PartialOrd for ImageInfo
{
	fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering>
	{
		Some(self.cmp(other))
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

fn locate_module_from_ptr(images:&[ImageInfo],ptr:u64)->Option<&ImageInfo>
{
	let mut lo:isize=0;
	let mut hi=images.len() as isize-1;
	while hi>=lo
	{
		let mid=(lo+hi)>>1;
		if images[mid as usize].base+images[mid as usize].length<ptr
		{
			lo=mid+1;
		}
		else if images[mid as usize].base>ptr
		{
			hi=mid-1;
		}
		else
		{
			return Some(&images[mid as usize]);
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
	/*
	if let Err(e)=unsafe{SymRegisterCallbackW64(GetCurrentProcess(),Some(sym_registered_callback),0)}
	{
		panic!("SymRegisterCallbackW64 failed! Reason: {e}");
	}*/
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
						match argv.get(2)
						{
							// Target address is specified. Halt target and see the symbol.
							Some(s)=>
							{
								let target_address:u64=u64::from_str_radix(s.as_str(),16).unwrap();
								if let Some(r)=locate_symbol(target_address)
								{
									println!("Symbol: {}+{:X} ({}@{})",r.name,r.displacement,r.source_file,r.line_number);
								}
								match locate_module_from_ptr(&r,target_address)
								{
									Some(img_info)=>
									{
										match RemotePEImage::new(&mut target,img_info.base,img_info.length,img_info.path.clone())
										{
											Some(mut pe_img)=>
											{
												println!("Loading symbols for {} (Base: 0x{:016X}, Length: 0x{:X})...",img_info.path,img_info.base,img_info.length);
												pe_img.load_symbols();
												if let Some(r)=locate_symbol(target_address)
												{
													println!("Symbol: {}+{:X} ({}@{})",r.name,r.displacement,r.source_file,r.line_number);
												}
											}
											None=>println!("Failed to initialize PE Image!")
										}
									}
									_=>println!("[Test] Failed to locate!")
								}
								// Also disassemble the instruction.
								match target.read_memory(target_address,15)
								{
									Ok(ins_bytes)=>
									{
										let mut decoder=Decoder::with_ip(64,&ins_bytes,target_address,0);
										if decoder.can_decode()
										{
											let mut formatter=MasmFormatter::new();
											let instruction=decoder.decode();
											let mut output=String::new();
											formatter.format(&instruction,&mut output);
											print!("{:016X}\t",target_address);
											for b in ins_bytes.iter().take(instruction.len())
											{
												print!("{:02X} ",*b);
											}
											println!("\t{output}");
										}
									}
									Err(e)=>println!("Failed to read instruction bytes! Reason: {e}")
								}
							}
							None=>
							{
								// Target address is not specified. Halt target and analyze stack trace.
								// Read registers.
								match target.read_registers::<QemuGdbX64Registers>()
								{
									#[allow(clippy::field_reassign_with_default)]
									Ok(regs)=>
									{
										println!("Registers: {regs:X?}");
										let mut stk_f=STACKFRAME_EX::default();
										stk_f.AddrPC=ADDRESS64{Offset:regs.rip,Segment:0,Mode:AddrModeFlat};
										stk_f.AddrStack=ADDRESS64{Offset:regs.rsp,Segment:0,Mode:AddrModeFlat};
										stk_f.StackFrameSize=size_of::<STACKFRAME_EX>() as u32;
										let mut ctxt=CONTEXT::default();
										ctxt.MxCsr=regs.mxcsr;
										ctxt.SegCs=regs.cs as u16;
										ctxt.SegDs=regs.ds as u16;
										ctxt.SegEs=regs.es as u16;
										ctxt.SegFs=regs.fs as u16;
										ctxt.SegGs=regs.gs as u16;
										ctxt.SegSs=regs.ss as u16;
										ctxt.EFlags=regs.eflags;
										ctxt.Rax=regs.rax;
										ctxt.Rcx=regs.rcx;
										ctxt.Rdx=regs.rdx;
										ctxt.Rbx=regs.rbx;
										ctxt.Rsp=regs.rsp;
										ctxt.Rbp=regs.rbp;
										ctxt.Rsi=regs.rsi;
										ctxt.Rdi=regs.rdi;
										ctxt.R8=regs.r8;
										ctxt.R9=regs.r9;
										ctxt.R10=regs.r10;
										ctxt.R11=regs.r11;
										ctxt.R12=regs.r12;
										ctxt.R13=regs.r13;
										ctxt.R14=regs.r14;
										ctxt.R15=regs.r15;
										let mut handle=SymProcessHandle
										{
											target:&raw mut target,
											images:r.as_ptr(),
											count:r.len(),
											fpo:IMAGE_RUNTIME_FUNCTION_ENTRY::default()
										};
										while unsafe{StackWalkEx(IMAGE_FILE_MACHINE_AMD64.0.into(),HANDLE(&raw mut handle as *mut c_void),None,&raw mut stk_f,&raw mut ctxt as *mut c_void,Some(sym_read_memory_rt),Some(sym_func_table_access_rt),Some(sym_get_module_base_rt),None,0)}.as_bool()
										{
											// println!("Stack Info: {stk_f:X?}");
											let target_address=stk_f.AddrPC.Offset;
											match locate_module_from_ptr(&r,target_address)
											{
												Some(img_info)=>
												{
													match RemotePEImage::new(&mut target,img_info.base,img_info.length,img_info.path.clone())
													{
														Some(mut pe_img)=>
														{
															// println!("Loading symbols for {} (Base: 0x{:016X}, Length: 0x{:X})...",img_info.path,img_info.base,img_info.length);
															pe_img.load_symbols();
															if let Some(r)=locate_symbol(target_address)
															{
																println!("Symbol: {}+{:X} ({}@{})",r.name,r.displacement,r.source_file,r.line_number);
															}
														}
														None=>println!("Failed to initialize PE Image!")
													}
												}
												_=>println!("[Test] Failed to locate!")
											}
											// Move to next stack frame.
											let pc=stk_f.AddrReturn;
											let sp=stk_f.AddrFrame;
											stk_f=STACKFRAME_EX::default();
											stk_f.AddrPC=pc;
											stk_f.AddrStack=sp;
											stk_f.AddrStack.Offset=sp.Offset.wrapping_add(0x10);
											stk_f.StackFrameSize=size_of::<STACKFRAME_EX>() as u32;
											// println!("New Stack Info: {stk_f:X?}");
										}
									}
									Err(e)=>println!("Failed to read registers! Reason: {e}")
								}
							}
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
	let _=unsafe{SymCleanup(GetCurrentProcess())};
}

unsafe extern "system" fn sym_read_memory_rt(process:HANDLE,base_address:u64,buffer:*mut c_void,size:u32,number_of_bytes_read:*mut u32)->BOOL
{
	let handle=unsafe{&mut *(process.0 as *mut SymProcessHandle)};
	let target=&mut *handle.target;
	match target.read_memory(base_address,size as usize)
	{
		Ok(r)=>
		{
			unsafe
			{
				let p:*mut u8=buffer.cast();
				for (i,v) in r.iter().enumerate().take(size as usize)
				{
					p.add(i).write(*v);
				}
				number_of_bytes_read.write(size);
			}
		}
		Err(e)=>panic!("Failed to read memory! Reason: {e}")
	}
	TRUE
}

unsafe extern "system" fn sym_func_table_access_rt(process:HANDLE,addr_base:u64)->*mut c_void
{
	// This routine must be manually implemented, in that this incurs remote memory accesses!
	// println!("Function-Table Access is queried! Address: 0x{addr_base:016X}");
	let handle=unsafe{&mut *(process.0 as *mut SymProcessHandle)};
	let images=slice::from_raw_parts(handle.images,handle.count);
	match locate_module_from_ptr(images,addr_base)
	{
		Some(img)=>
		{
			match RemotePEImage::new(&mut *handle.target,img.base,img.length,img.path.clone())
			{
				Some(mut pe_img)=>
				{
					match pe_img.load_fpo_data(addr_base)
					{
						Some(fpo)=>
						{
							handle.fpo=fpo;
							&raw mut handle.fpo as *mut c_void
						}
						None=>
						{
							println!("Failed to locate FPO for 0x{addr_base:016X}!");
							null_mut()
						}
					}
				}
				None=>null_mut()
			}
		}
		None=>
		{
			panic!("Failed to locate image info!");
			// null_mut()
		}
	}
}

unsafe extern "system" fn sym_get_module_base_rt(process:HANDLE,address:u64)->u64
{
	let handle=unsafe{&mut *(process.0 as *mut SymProcessHandle)};
	let images=slice::from_raw_parts(handle.images,handle.count);
	match locate_module_from_ptr(images,address)
	{
		Some(img)=>
		{
			let p=img.base;
			p

		}
		None=>
		{
			println!("Failed to locate image info!");
			0
		}
	}
}