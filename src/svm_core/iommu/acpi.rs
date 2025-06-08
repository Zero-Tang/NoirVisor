/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines the ACPI for AMD-Vi of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::*;
use paste::paste;

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

#[repr(C)] pub struct IvhdFlags(pub u8);

impl IvhdFlags
{
	build_bit_mut_method!(ht_tun_en,0);
	build_bit_mut_method!(pass_pw,1);
	build_bit_mut_method!(res_pass_pw,2);
	build_bit_mut_method!(isoc,3);
	build_bit_mut_method!(iotlb_sup,4);
	build_bit_mut_method!(coherent,5);
	build_bit_mut_method!(prefetch_sup,6);
	build_bit_mut_method!(ppr_sup,7);
}

#[repr(C)] pub struct IvhdIommuInfo(pub u16);

impl IvhdIommuInfo
{
	build_int_mut_method!(msi_num,0,5,u16);
	build_int_mut_method!(unit_id,12,5,u16);
}

#[repr(C)] pub struct IvhdIommuFeatureReporting(pub u32);

impl IvhdIommuFeatureReporting
{
	build_bit_mut_method!(xt_sup,0);
	build_bit_mut_method!(nx_sup,1);
	build_bit_mut_method!(gt_sup,2);
	build_int_mut_method!(glx_sup,3,2,u32);
	build_bit_mut_method!(ia_sup,5);
	build_bit_mut_method!(ga_sup,6);
	build_bit_mut_method!(he_sup,7);
	build_int_mut_method!(pas_max,8,5,u32);
	build_int_mut_method!(pn_counters,13,4,u32);
	build_int_mut_method!(pn_banks,17,6,u32);
	build_int_mut_method!(msi_num_ppr,23,5,u32);
	build_int_mut_method!(gats,28,2,u32);
	build_int_mut_method!(hats,30,2,u32);
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

#[repr(C)] pub struct IvhdDteSetting(pub u8);

impl IvhdDteSetting
{
	build_bit_mut_method!(init_pass,0);
	build_bit_mut_method!(exint_pass,1);
	build_bit_mut_method!(nmi_pass,2);
	build_int_mut_method!(sys_mgt,4,2,u8);
	build_bit_mut_method!(lint0_pass,6);
	build_bit_mut_method!(lint1_pass,7);
}

#[repr(C)] pub struct IvhdExtendedDteSetting(pub u32);

impl IvhdExtendedDteSetting
{
	build_bit_mut_method!(ats_disabled,31);
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

#[repr(C)] pub struct IvmdFlags(pub u8);

impl IvmdFlags
{
	build_bit_mut_method!(unity,0);
	build_bit_mut_method!(ir,1);
	build_bit_mut_method!(iw,2);
	build_bit_mut_method!(exclusion_range,3);
}