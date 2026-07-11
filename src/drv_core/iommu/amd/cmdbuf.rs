/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines the AMD-Vi Command Buffer of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;

pub(super) trait CommandBufferItem:Into<u128>
{
	const OPCODE:u8;
}

/// Derives the implementation of `CommandBufferItem` trait. \
/// To derive this trait, the type must be `#[bitfield(u128,new=false,default=false)]`
macro_rules! derive_cmdbuf_item
{
	($type:ty,$opcode:literal)=>
	{
		impl CommandBufferItem for $type
		{
			const OPCODE:u8=$opcode;
		}

		impl Default for $type
		{
			fn default()->Self
			{
				Self::new()
			}
		}

		impl $type
		{
			pub const fn new()->Self
			{
				Self::from_bits((Self::OPCODE as u128)<<60)
			}
		}
	};
}

#[bitfield(u128,new=false,default=false)] pub struct CompletionWaitCommand
{
	pub s:bool,
	pub i:bool,
	pub f:bool,
	#[bits(49)] pub store_address:u64,
	rsvd:u8,
	#[bits(4)] pub opcode:u8,
	pub store_data:u64
}

derive_cmdbuf_item!(CompletionWaitCommand,1);

#[bitfield(u128,new=false,default=false)] pub struct InvalidateDevtabEntryCommand
{
	pub device_id:u16,
	#[bits(44)] rsvd0:u64,
	#[bits(4)] pub opcode:u8,
	rsvd1:u64
}

derive_cmdbuf_item!(InvalidateDevtabEntryCommand,2);

#[bitfield(u128,new=false,default=false)] pub struct InvalidateIommuPagesCommand
{
	#[bits(20)] pub pasid:u32,
	#[bits(12)] rsvd0:u32,
	pub domain_id:u16,
	#[bits(12)] rsvd1:u32,
	#[bits(4)] pub opcode:u8,
	pub s:bool,
	pub pde:bool,
	pub gn:bool,
	#[bits(9)] rsvd2:u32,
	#[bits(52)] pub address:u64
}

derive_cmdbuf_item!(InvalidateIommuPagesCommand,3);

#[bitfield(u128,new=false,default=false)] pub struct InvalidateIotlbPagesCommand
{
	pub device_id:u16,
	pub pasid_mid:u8,
	pub maxpend:u8,
	pub queue_id:u16,
	pub pasid_lo:u8,
	#[bits(4)] pub pasid_hi:u8,
	#[bits(4)] pub opcode:u8,
	pub s:bool,
	rsvd0:bool,
	pub gn:bool,
	rsvd1:bool,
	#[bits(2)] pub r#type:u8,
	#[bits(6)] rsvd2:u8,
	#[bits(52)] pub address:u64
}

derive_cmdbuf_item!(InvalidateIotlbPagesCommand,4);

#[bitfield(u128,new=false,default=false)] pub struct InvalidateInterruptTableCommand
{
	pub device_id:u16,
	#[bits(44)] rsvd0:u64,
	#[bits(4)] pub opcode:u8,
	rsvd1:u64
}

derive_cmdbuf_item!(InvalidateInterruptTableCommand,5);

#[bitfield(u128,new=false,default=false)] pub struct PrefetchIommuPagesCommand
{
	pub device_id:u16,
	rsvd0:u8,
	pub pf_count:u8,
	#[bits(20)] pub pasid:u32,
	rsvd1:u8,
	#[bits(4)] pub opcode:u8,
	pub s:bool,
	rsvd2:bool,
	pub gn:bool,
	rsvd3:bool,
	pub inval:bool,
	#[bits(7)] rsvd4:u8,
	#[bits(52)] pub address:u64
}

derive_cmdbuf_item!(PrefetchIommuPagesCommand,6);

#[bitfield(u128,new=false,default=false)] pub struct CompletePprRequestCommand
{
	pub device_id:u16,
	rsvd0:u16,
	#[bits(20)] pub pasid:u32,
	rsvd1:u8,
	#[bits(4)] pub opcode:u8,
	#[bits(2)] rsvd2:u8,
	pub gn:bool,
	#[bits(29)] rsvd3:u32,
	pub completion_tag:u16,
	rsvd4:u16
}

derive_cmdbuf_item!(CompletePprRequestCommand,7);

#[bitfield(u128,new=false,default=false)] pub struct InvalidateIommuAllCommand
{
	#[bits(60)] rsvd0:u64,
	#[bits(4)] pub opcode:u8,
	rsvd1:u64
}

derive_cmdbuf_item!(InvalidateIommuAllCommand,8);

#[bitfield(u128,new=false,default=false)] pub struct InsertGuestEventCommand
{
	rsvd0:u32,
	pub guest_id:u16,
	#[bits(12)] rsvd1:u16,
	#[bits(4)] pub opcode:u8,
	rsvd2:u64
}

derive_cmdbuf_item!(InsertGuestEventCommand,9);
