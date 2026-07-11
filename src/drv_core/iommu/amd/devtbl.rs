/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the AMD-Vi MMIO driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;

#[bitfield(u64)] pub struct AmdIommuDteP1
{
	pub v:bool,
	pub tv:bool,
	#[bits(2)] rsvd0:u64,
	#[bits(3)] pub cxl_mem_attr:u64,
	pub ha:bool,
	pub hd:bool,
	#[bits(3)] pub mode:u64,
	#[bits(40)] pub page_table_root_pointer:u64,
	pub ppr:bool,
	pub gprp:bool,
	pub giov:bool,
	pub gv:bool,
	#[bits(2)] pub glx:u64,
	#[bits(3)] pub gcr3_trp_lo:u64,
	pub ir:bool,
	pub iw:bool,
	rsvd1:bool
}

#[bitfield(u64)] pub struct AmdIommuDteP2
{
	pub did:u16,
	pub gcr3_trp_mid:u16,
	pub i:bool,
	pub se:bool,
	pub sa:bool,
	#[bits(2)] pub ioctl:u64,
	pub cache:bool,
	pub sd:bool,
	pub ex:bool,
	#[bits(2)] pub sysmgt:u64,
	pub sats:bool,
	#[bits(21)] pub gcr3_trp_hi:u64
}

#[bitfield(u64)] pub struct AmdIommuDteP3
{
	pub iv:bool,
	#[bits(4)] pub int_tab_len:u64,
	pub ig:bool,
	#[bits(46)] pub interrupt_table_root_pointer:u64,
	#[bits(2)] rsvd:u64,
	#[bits(2)] pub guest_paging_mode:u64,
	pub init_pass:bool,
	pub eint_pass:bool,
	pub nmi_pass:bool,
	pub hpt_mode:bool,
	#[bits(2)] pub int_ctl:u64,
	pub lint0_pass:bool,
	pub lint1_pass:bool
}

#[bitfield(u64)] pub struct AmdIommuDteP4
{
	#[bits(15)] rsvd0:u64,
	pub vimu_en:bool,
	pub gdevice_id:u16,
	pub guest_id:u16,
	#[bits(6)] rsvd1:u64,
	pub attr_v:bool,
	pub mode0_fc:bool,
	pub snoop_attribute:u8
}

#[repr(C)] pub struct AmdIommuDeviceTableEntry
{
	pub p1:AmdIommuDteP1,
	pub p2:AmdIommuDteP2,
	pub p3:AmdIommuDteP3,
	pub p4:AmdIommuDteP4,
}

impl Default for AmdIommuDeviceTableEntry
{
	fn default() -> Self
	{
		Self
		{
			p1:AmdIommuDteP1(0),
			p2:AmdIommuDteP2(0),
			p3:AmdIommuDteP3(0),
			p4:AmdIommuDteP4(0)
		}
	}
}

impl AmdIommuDeviceTableEntry
{
	pub fn get_gcr3_table_trp(&self)->u64
	{
		let lo=self.p1.gcr3_trp_lo();
		let mid=self.p2.gcr3_trp_mid() as u64;
		let hi=self.p2.gcr3_trp_hi();
		lo|(mid<<3)|(hi<<19)
	}

	pub fn set_gcr3_table_trp(&mut self,value:u64)
	{
		let lo=value&0x7;
		let mid=(value>>3) as u16;
		let hi=(value>>19)&0xFFFFF;
		self.p1.set_gcr3_trp_lo(lo);
		self.p2.set_gcr3_trp_mid(mid);
		self.p2.set_gcr3_trp_hi(hi);
	}
}