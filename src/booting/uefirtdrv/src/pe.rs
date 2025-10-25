/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file defines a few PE structures for NoirVisor on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).
*/

#[repr(C)]
pub struct IMAGE_DOS_HEADER
{
	pub e_magic:u16,
	pub e_cblp:u16,
	pub e_cp:u16,
	pub e_crlc:u16,
	pub e_cparhdr:u16,
	pub e_minalloc:u16,
	pub e_maxalloc:u16,
	pub e_ss:u16,
	pub e_sp:u16,
	pub e_csum:u16,
	pub e_ip:u16,
	pub e_cs:u16,
	pub e_lfarlc:u16,
	pub e_ovno:u16,
	pub e_res: [u16; 4],
	pub e_oemid:u16,
	pub e_oeminfo:u16,
	pub e_res2: [u16; 10],
	pub e_lfanew:u32,
}

#[repr(C)]
pub struct IMAGE_FILE_HEADER
{
	pub Machine:u16,
	pub NumberOfSections:u16,
	pub TimeDateStamp:u32,
	pub PointerToSymbolTable:u32,
	pub NumberOfSymbols:u32,
	pub SizeOfOptionalHeader:u16,
	pub Characteristics:u16,
}

#[repr(C)]
pub struct IMAGE_DATA_DIRECTORY
{
	pub VirtualAddress:u32,
	pub Size:u32,
}


#[repr(C)]
pub struct IMAGE_OPTIONAL_HEADER64
{
	pub Magic:u16,
	pub LinkerMajorVersion:u8,
	pub LinkerMinorVersion:u8,
	pub SizeOfCode:u32,
	pub SizeOfInitializedData:u32,
	pub SizeOfUninitializedData:u32,
	pub AddressOfEntryPoint:u32,
	pub BaseOfCode:u32,
	pub ImageBase:u64,
	pub SectionAlignment:u32,
	pub FileAlignment:u32,
	pub OperatingSystemMajorVersion:u16,
	pub OperatingSystemMinorVersion:u16,
	pub ImageMajorVersion:u16,
	pub ImageMinorVersion:u16,
	pub SubsystemMajorVersion:u16,
	pub SubsystemMinorVersion:u16,
	pub Win32VersionValue:u32,
	pub SizeOfImage:u32,
	pub SizeOfHeaders:u32,
	pub CheckSum:u32,
	pub Subsystem:u16,
	pub DllCharacteristics:u16,
	pub SizeOfStackReserve:u64,
	pub SizeOfStackCommit:u64,
	pub SizeOfHeapReserve:u64,
	pub SizeOfHeapCommit:u64,
	pub LoaderFlags:u32,
	pub NumberOfRvaAndSizes:u32,
	pub DataDirectory:[IMAGE_DATA_DIRECTORY;16],
}

#[repr(C)]
pub struct IMAGE_NT_HEADERS64
{
	pub Signature:u32,
	pub FileHeader:IMAGE_FILE_HEADER,
	pub OptionalHeader:IMAGE_OPTIONAL_HEADER64,
}

#[repr(C)]
pub struct IMAGE_SECTION_HEADER
{
	pub Name:[u8;IMAGE_SIZEOF_SHORT_NAME],
	pub VirtualSize:u32,
	pub VirtualAddress:u32,
	pub SizeOfRawData:u32,
	pub PointerToRawData:u32,
	pub PointerToRelocations:u32,
	pub PointerToLinenumbers:u32,
	pub NumberOfRelocations:u16,
	pub NumberOfLinenumbers:u16,
	pub Characteristics:u32,
}

#[cfg(target_pointer_width="64")]
pub type IMAGE_NT_HEADERS=IMAGE_NT_HEADERS64;
#[cfg(target_pointer_width="32")]
pub type IMAGE_NT_HEADERS=IMAGE_NT_HEADERS32;

pub const IMAGE_SIZEOF_SHORT_NAME:usize=8;
pub const IMAGE_DOS_SIGNATURE:u16=0x5A4D;
pub const IMAGE_NT_SIGNATURE:u32=0x4550;