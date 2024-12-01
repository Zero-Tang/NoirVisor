// PE Image and Symbols.
use std::{str, slice};

use windows::{core::*, Win32::{Foundation::*, Storage::FileSystem::*, System::{Diagnostics::Debug::*, SystemServices::*, Threading::GetCurrentProcess}}};

use crate::gdb::Target;

#[allow(non_camel_case_types,non_snake_case,dead_code)]
#[repr(C)] pub struct IMAGE_FILE_HEADER
{
	pub Machine:u16,
    pub NumberOfSections: u16,
    pub TimeDateStamp: u32,
    pub PointerToSymbolTable: u32,
    pub NumberOfSymbols: u32,
    pub SizeOfOptionalHeader: u16,
    pub Characteristics: IMAGE_FILE_CHARACTERISTICS,
}

#[allow(non_camel_case_types,non_snake_case,dead_code)]
#[repr(C)] pub struct IMAGE_NT_HEADERS64
{
	pub Signature:u32,
	pub FileHeader:IMAGE_FILE_HEADER,
	pub OptionalHeader:IMAGE_OPTIONAL_HEADER64
}

#[allow(non_camel_case_types,non_snake_case,dead_code)]
#[repr(C)] pub struct IMAGE_CODEVIEW_DEBUG_DIRECTORY
{
	pub Signature:[u8;4],
	pub Guid:GUID,
	pub Age:u32,
	pub Path:[u8;0]
}

#[allow(dead_code)]
pub struct RemotePEImage <'a>
{
	target:&'a mut Target,
	pub base:u64,
	pub size:u64,
	pub path:String,
	pub name:String,
	debug_section:IMAGE_DATA_DIRECTORY
}

impl <'a> RemotePEImage<'a>
{
	pub fn new(target:&'a mut Target,base:u64,size:u64,path:String)->Option<Self>
	{
		match target.read_memory(base,size_of::<IMAGE_DOS_HEADER>())
		{
			Ok(raw)=>
			{
				let dos_head=raw.as_ptr() as *const IMAGE_DOS_HEADER;
				unsafe
				{
					if (*dos_head).e_magic==IMAGE_DOS_SIGNATURE
					{
						match target.read_memory(base+(*dos_head).e_lfanew as u64,size_of::<IMAGE_NT_HEADERS64>())
						{
							Ok(raw)=>
							{
								let nt_head=raw.as_ptr() as *const IMAGE_NT_HEADERS64;
								if (*nt_head).Signature==IMAGE_NT_SIGNATURE
								{
									let name=match path.rfind('\\')
									{
										Some(i)=>String::from(&path.as_str()[i+1..]),
										None=>path.clone()
									};
									Some
									(
										Self
										{
											target,
											base,
											size,
											path,
											name,
											debug_section:(*nt_head).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG.0 as usize]
										}
									)
								}
								else
								{
									None
								}
							}
							Err(e)=>
							{
								println!("Failed to dump IMAGE_NT_HEADERS! Reason: {e}");
								None
							}
						}
					}
					else
					{
						println!("Invalid IMAGE_DOS_HEADER Signature!");
						None
					}
				}
			}
			Err(e)=>
			{
				println!("Failed to dump IMAGE_DOS_HEADER! Reason: {e}");
				None
			}
		}
	}

	pub fn load_symbols(&mut self)
	{
		println!("Debug Directory: 0x{:X}, Size: 0x{:X}",self.debug_section.VirtualAddress,self.debug_section.Size);
		match self.target.read_memory(self.base+self.debug_section.VirtualAddress as u64,self.debug_section.Size as usize)
		{
			Ok(raw)=>
			{
				let count=(self.debug_section.Size as usize)/size_of::<IMAGE_DEBUG_DIRECTORY>();
				let debug_dir:*const IMAGE_DEBUG_DIRECTORY=raw.as_ptr().cast();
				unsafe
				{
					for i in 0..count
					{
						let debug_entry=debug_dir.add(i);
						match (*debug_entry).Type
						{
							IMAGE_DEBUG_TYPE_CODEVIEW=>
							{
								match self.target.read_memory(self.base+(*debug_entry).AddressOfRawData as u64,(*debug_entry).SizeOfData as usize)
								{
									Ok(raw)=>
									{
										let cv_dir:*const IMAGE_CODEVIEW_DEBUG_DIRECTORY=raw.as_ptr().cast();
										let pdb_path_raw=slice::from_raw_parts((*cv_dir).Path.as_ptr(),(*debug_entry).SizeOfData as usize-size_of::<IMAGE_CODEVIEW_DEBUG_DIRECTORY>());
										let pdb_path=str::from_utf8_unchecked(pdb_path_raw);
										println!("CodeView PDB File: {}",pdb_path);
										match CreateFileA(PCSTR::from_raw((*cv_dir).Path.as_ptr()),GENERIC_READ.0,FILE_SHARE_READ,None,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,None)
										{
											Ok(h_file)=>
											{
												let fp=String::from(pdb_path);
												let fsize=GetFileSize(h_file,None);
												let fp_utf16:Vec<u16>=fp.encode_utf16().collect();
												let fn_utf16:Vec<u16>=match fp.rfind('\\')
												{
													Some(i)=>String::from(&fp.as_str()[i+1..]),
													None=>fp.clone()
												}.encode_utf16().collect();
												let mod_ptr=SymLoadModuleExW
												(
													GetCurrentProcess(),
													h_file,
													PCWSTR::from_raw(fp_utf16.as_ptr()),
													PCWSTR::from_raw(fn_utf16.as_ptr()),
													self.base,
													fsize,
													None,
													SYM_LOAD_FLAGS(0)
												);
												if mod_ptr==0
												{
													println!("Failed to load symbol! Reason: {}",GetLastError().to_hresult().message());
												}
												let _=CloseHandle(h_file);
											}
											Err(e)=>println!("Failed to open symbol file! Reason: {e}")
										}
									}
									Err(e)=>panic!("Failed to dump IMAGE_CODEVIEW_DEBUG_DIRECTORY! Reason: {e}")
								}
								break;
							}
							_=>println!("Ignoring Unknown Debug Type: {}!",(*debug_entry).Type.0)
						}
					}
				}
			}
			Err(e)=>
			{
				println!("Failed to dump IMAGE_DEBUG_DIRECTORY! Reason: {e}");
			}
		}
	}
}

pub fn locate_symbol(address:u64)->Option<(String,u64)>
{
	let mut buff:[u8;1024]=[0;1024];
	let mut disp:u64=0;
	unsafe
	{
		let sym_info:*mut SYMBOL_INFOW=buff.as_mut_ptr().cast();
		(*sym_info).SizeOfStruct=size_of::<SYMBOL_INFOW>() as u32;
		(*sym_info).MaxNameLen=((buff.len()-size_of::<SYMBOL_INFOW>())>>1) as u32;
		match SymFromAddrW(GetCurrentProcess(),address,Some(&raw mut disp),sym_info)
		{
			Ok(_)=>
			{
				let sym_name_raw:&[u16]=slice::from_raw_parts((*sym_info).Name.as_ptr(),(*sym_info).NameLen as usize);
				match String::from_utf16(sym_name_raw)
				{
					Ok(sym_name)=>return Some((sym_name,disp)),
					Err(e)=>println!("Failed to convert from UTF-16! Reason: {e}")
				}
			}
			Err(e)=>println!("Failed to locate symbol! Reason: {e}")
		}
	}
	None
}