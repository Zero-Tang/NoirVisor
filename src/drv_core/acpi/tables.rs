/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines ACPI tables for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use paste::paste;
use core::{fmt::{self,Display}, mem::offset_of, str};

use crate::*;

#[repr(C)] pub struct AcpiAddressSpaceId(pub u8);

impl AcpiAddressSpaceId
{
	pub const SYSTEM_MEMORY:u8=0;
	pub const SYSTEM_IO:u8=1;
	pub const PCI_CONFIG_SPACE:u8=2;
	pub const PCI_BAR_TARGET:u8=6;
	pub const FUNCTIONAL_FIXED_HARDWARE:u8=0x7F;
}

#[repr(C,packed)] pub struct GenericAddress
{
	pub asid:AcpiAddressSpaceId,
	pub register_width:u8,
	pub register_offset:u8,
	pub access_size:u8,
	pub address:u64
}

#[repr(C)] pub struct RootSystemDescriptionPointer
{
	pub signature:u64,
	pub checksum:u8,
	pub oemid:[u8;6],
	pub revision:u8,
	pub rsdt_address:u32,
	pub length:u32,
	pub xsdt_address:u64,
	pub ext_checksum:u8,
	pub reserved:[u8;3]
}

#[derive(Clone, Copy, PartialEq)]
#[repr(C)] pub struct AcpiSystemDescriptorSignature(pub [u8;4]);

macro_rules! make_acpi_signature
{
	($name:tt,$value:literal) =>
	{
		paste!
		{
			pub const [<$name:upper>]:Self=Self(*$value);
		}
	};
}

impl AcpiSystemDescriptorSignature
{
	// Currently we'd only define the signatures which might be used by NoirVisor.
	make_acpi_signature!(MULTIPLE_APIC_DESCRIPTION_TABLE,b"APIC");
	make_acpi_signature!(DMA_REMAPPING_TABLE,b"DMAR");
	make_acpi_signature!(DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE,b"DSDT");
	make_acpi_signature!(FIXED_ACPI_DESCRIPTION_TABLE,b"FADP");
	make_acpi_signature!(HIGH_PRECISION_EVENT_TIMER_TABLE,b"HPET");
	make_acpi_signature!(IO_VIRTUALIZATION_REPORTING_STRUCTURE,b"IVRS");
	make_acpi_signature!(ROOT_SYSTEM_DESCRIPTION_TABLE,b"RSDT");
	make_acpi_signature!(EXTENDED_SYSTEM_DESCRIPTION_TABLE,b"XSDT");
}

impl Display for AcpiSystemDescriptorSignature
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		let buff=self.0;
		write!(f,"{}",unsafe{str::from_utf8_unchecked(&buff)})
	}
}

#[repr(C,packed)] pub struct SystemDescriptionHeader
{
	pub signature:AcpiSystemDescriptorSignature,
	length:u32,
	pub revision:u8,
	pub checksum:u8,
	pub oemid:[u8;6],
	pub oem_table_id:[u8;8],
	pub oem_revision:u32,
	pub creator_id:u32,
	pub creator_revision:u32
}

impl SystemDescriptionHeader
{
	pub fn get_length(&self)->u32
	{
		// ACPI-table does not guarantee alignment.
		let s=self as *const Self;
		let p:*const u32=unsafe{s.byte_add(offset_of!(Self,length)).cast()};
		unsafe{p.read_unaligned()}
	}
}

#[repr(C)] pub struct RootSystemDescriptionTable
{
	pub header:SystemDescriptionHeader,
	pub entries:[u32;0]
}

#[repr(C,packed)] pub struct ExtendedSystemDescriptorTable
{
	pub header:SystemDescriptionHeader,
	pub entries:[u64;0]
}

#[repr(C,packed)] pub struct HighPrecisionEventTimerTable
{
	pub header:SystemDescriptionHeader,
	pub hardware_id:u32,
	pub block:GenericAddress,
	pub hpet_number:u8,
	pub minimum_tick:u16,
	pub page_protection:u8
}

#[repr(C)] pub struct DmaRemappingReportingStructure
{
	pub header:SystemDescriptionHeader,
	pub host_address_width:u8,
	pub flags:u8,
	pub reserved:[u8;10],
	pub remap_structs:[u8;0]
}

#[repr(C)] pub struct IoVirtualizationReportingStructure
{
	pub header:SystemDescriptionHeader,
	pub iv_info:IvInfo,
	pub reserved:u64,
	pub ivdb:[u8;0]
}

#[repr(C)] pub struct IvInfo(pub u32);

impl IvInfo
{
	build_bit_mut_method!(efr_sup,0);
	build_bit_mut_method!(dma_remap_sup,1);
	build_int_mut_method!(gva_size,5,3,u32);
	build_int_mut_method!(pa_size,8,7,u32);
	build_int_mut_method!(va_size,15,7,u32);
	build_bit_mut_method!(ht_ats_reserved,22);
}