/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines the ACPI for AMD-Vi of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::*;

use bitfield_struct::bitfield;

#[repr(C,packed)] pub struct Ivhd
{
	pub ivdb_type:u8,
	pub flags:IvhdFlags,
	pub length:u16,
	pub device_id:u16,
	pub capability_offset:u16,
	pub iommu_base_pa:u64,
	pub pci_segment_group:u16,
	pub iommu_info:IvhdIommuInfo,
	pub iommu_feature_reporting:IvhdIommuFeatureReporting,
	pub ivhd_dtes:[u32;0]
}

impl Ivhd
{
	pub const REVISION_FIXED:u8=0x1;
	pub const REVISION_MIXED:u8=0x2;
}

#[bitfield(u8)] pub struct IvhdFlags
{
	pub ht_tun_en:bool,
	pub pass_pw:bool,
	pub res_pass_pw:bool,
	pub isoc:bool,
	pub iotlb_sup:bool,
	pub coherent:bool,
	pub prefetch_sup:bool,
	pub ppr_sup:bool
}

#[bitfield(u16)] pub struct IvhdIommuInfo
{
	#[bits(5)] pub msi_num:u16,
	#[bits(3)] rsvd0:u16,
	#[bits(5)] pub unit_id:u16,
	#[bits(3)] rsvd1:u16
}

#[bitfield(u32)] pub struct IvhdIommuFeatureReporting
{
	pub xt_sup:bool,
	pub nx_sup:bool,
	pub gt_sup:bool,
	#[bits(2)] pub glx_sup:u32,
	pub ia_sup:bool,
	pub ga_sup:bool,
	pub he_sup:bool,
	#[bits(5)] pub pas_max:u32,
	#[bits(4)] pub pn_counters:u32,
	#[bits(6)] pub pn_banks:u32,
	#[bits(5)] pub msi_num_ppr:u32,
	#[bits(2)] pub gats:u32,
	#[bits(2)] pub hats:u32
}

#[repr(C,packed)] pub struct IvhdLarge
{
	pub ivdb_type:u8,
	pub flags:IvhdFlags,
	pub length:u16,
	pub device_id:u16,
	pub capability_offset:u16,
	pub iommu_base_pa:u64,
	pub pci_segment_group:u16,
	pub iommu_info:IvhdIommuInfo,
	pub iommu_attributes:IvhdIommuFeatureReporting,
	pub efr_register_image:u64,
	pub efr_register_image2:u64,
	pub ivhd_dtes:[u32;0]
}

pub enum IvhdDeviceEntry
{
	Invalid,
	All
	{
		settings:IvhdDteSetting
	},
	Select
	{
		device_id:u16,
		settings:IvhdDteSetting
	},
	Range
	{
		start:u16,
		end:u16,
		settings:IvhdDteSetting
	},
	AliasSelect
	{
		actual_id:u16,
		source_id:u16,
		settings:IvhdDteSetting,
	},
	AliasRange
	{
		actual_id:u16,
		start:u16,
		end:u16,
		settings:IvhdDteSetting
	},
	ExtendedSelect
	{
		device_id:u16,
		settings:IvhdDteSetting,
		extended:IvhdExtendedDteSetting
	},
	ExtendedRange
	{
		start:u16,
		end:u16,
		settings:IvhdDteSetting,
		extended:IvhdExtendedDteSetting
	},
	SpecialDevice
	{
		settings:IvhdDteSetting,
		handle:u8,
		device_id:u16,
		variety:IvhdSpecialDeviceVariety
	},
	AcpiHidDevice
	{
		device_id:u16,
		settings:IvhdDteSetting,
		hardware_id:u64,
		compatible_id:u64,
		unique_id:IvhdDeviceUniqueID
	}
}

impl IvhdDeviceEntry
{
	fn from_raw_parts(ptr:&'static [u8])->Self
	{
		match ptr[0]
		{
			1=>Self::All{settings:IvhdDteSetting(ptr[3])},
			2=>Self::Select{device_id:unsafe{ptr.as_ptr().add(1).cast::<u16>().read_unaligned()},settings:IvhdDteSetting(ptr[3])},
			3=>
			{
				// Next entry must be End.
				assert_eq!(ptr[4],4);
				let s:[u8;2]=[ptr[1],ptr[2]];
				let e:[u8;2]=[ptr[5],ptr[6]];
				Self::Range
				{
					start:u16::from_le_bytes(s),
					end:u16::from_le_bytes(e),
					settings:IvhdDteSetting(ptr[3])
				}
			}
			66=>
			{
				let a:[u8;2]=[ptr[1],ptr[2]];
				let s:[u8;2]=[ptr[5],ptr[6]];
				Self::AliasSelect
				{
					actual_id:u16::from_le_bytes(a),
					source_id:u16::from_le_bytes(s),
					settings:IvhdDteSetting(ptr[3])
				}
			}
			67=>
			{
				let a:[u8;2]=[ptr[1],ptr[2]];
				let s:[u8;2]=[ptr[5],ptr[6]];
				assert_eq!(ptr[8],4);
				let e:[u8;2]=[ptr[9],ptr[10]];
				Self::AliasRange
				{
					actual_id:u16::from_le_bytes(a),
					start:u16::from_le_bytes(s),
					end:u16::from_le_bytes(e),
					settings:IvhdDteSetting(ptr[3])
				}
			}
			70=>
			{
				let d:[u8;2]=[ptr[1],ptr[2]];
				let e:[u8;4]=[ptr[4],ptr[5],ptr[6],ptr[7]];
				Self::ExtendedSelect
				{
					device_id:u16::from_le_bytes(d),
					settings:IvhdDteSetting(ptr[3]),
					extended:IvhdExtendedDteSetting(u32::from_le_bytes(e))
				}
			}
			71=>
			{
				// Next entry must be End.
				assert_eq!(ptr[8],4);
				let s:[u8;2]=[ptr[1],ptr[2]];
				let e:[u8;2]=[ptr[9],ptr[10]];
				let es:[u8;4]=[ptr[4],ptr[5],ptr[6],ptr[7]];
				Self::ExtendedRange
				{
					start:u16::from_le_bytes(s),
					end:u16::from_le_bytes(e),
					settings:IvhdDteSetting(ptr[3]),
					extended:IvhdExtendedDteSetting(u32::from_le_bytes(es))
				}
			}
			72=>
			{
				let d:[u8;2]=[ptr[5],ptr[6]];
				Self::SpecialDevice
				{
					settings:IvhdDteSetting(ptr[3]),
					handle:ptr[4],
					device_id:u16::from_le_bytes(d),
					variety:IvhdSpecialDeviceVariety(ptr[7])
				}
			}
			0xF0=>
			{
				let d:[u8;2]=ptr[1..3].try_into().unwrap();
				let h:[u8;8]=ptr[4..12].try_into().unwrap();
				let c:[u8;8]=ptr[12..20].try_into().unwrap();
				Self::AcpiHidDevice
				{
					device_id:u16::from_le_bytes(d),
					settings:IvhdDteSetting(ptr[3]),
					hardware_id:u64::from_le_bytes(h),
					compatible_id:u64::from_le_bytes(c),
					unique_id:match ptr[20]
					{
						0=>IvhdDeviceUniqueID::NotPresent,
						1=>IvhdDeviceUniqueID::Integer(u32::from_le_bytes(ptr[22..26].try_into().unwrap())),
						2=>IvhdDeviceUniqueID::String(unsafe{str::from_utf8_unchecked(&ptr[22..22+(ptr[21] as usize)])}),
						_=>IvhdDeviceUniqueID::NotPresent
					}
				}
			}
			_=>Self::Invalid
		}
	}
}

#[bitfield(u8)] pub struct IvhdDteSetting
{
	pub init_pass:bool,
	pub exint_pass:bool,
	pub nmi_pass:bool,
	rsvd:bool,
	#[bits(2)] pub sys_mgt:u8,
	pub lint0_pass:bool,
	pub lint1_pass:bool
}

#[bitfield(u32)] pub struct IvhdExtendedDteSetting
{
	#[bits(31)] rsvd:u32,
	pub ats_disabled:bool
}

#[repr(C)] pub struct IvhdSpecialDeviceVariety(pub u8);

impl IvhdSpecialDeviceVariety
{
	pub const IOAPIC:Self=Self(0x1);
	pub const HPET:Self=Self(0x2);
}

pub enum IvhdDeviceUniqueID
{
	NotPresent,
	Integer(u32),
	String(&'static str)
}

#[repr(C)] pub struct Ivmd
{
	pub ivmd_type:u8,
	pub flags:u8,
	pub length:u16,
	pub device_id:u16,
	pub aux_data:u16,
	pub pci_segment_group:u16,
	pub start_address:u64,
	pub block_length:u64
}

#[bitfield(u8)] pub struct IvmdFlags
{
	pub unity:bool,
	pub ir:bool,
	pub iw:bool,
	pub exclusion_range:bool,
	#[bits(4)] rsvd:u8
}