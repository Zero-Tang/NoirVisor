/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the AMD-Vi paging driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;
use log::*;

use crate::xpf_core::nvbdk::*;

/// ## AmdIommuPde
/// The concept of PDE in AMD-Vi is drastically different from AMD-V NPT, Intel EPT and Intel VT-d. \
/// In AMD-Vi, PDE simply means a non-terminal level in the map. It can be followed by a PDE or a PTE. \
/// In other words, PDE in AMD-Vi is equivalent to normal PDE, PDPTE, PML4E and PML5E in AMD-V NPT.
#[bitfield(u64)] pub struct AmdIommuPde
{
	pub pr:bool,
	#[bits(4)] rsvd0:u64,
	pub a:bool,
	#[bits(3)] rsvd1:u64,
	#[bits(3)] next_level:u64,
	#[bits(40)] pub page_address:u64,
	#[bits(9)] rsvd2:u64,
	pub ir:bool,
	pub iw:bool,
	rsvd3:bool
}

impl AmdIommuPde
{
	pub fn new_pde(pte:u64,next_level:u64,read:bool,write:bool)->Self
	{
		Self::from_bits(pte).with_pr(true).with_ir(read).with_iw(write).with_next_level(next_level)
	}
}

/// ## AmdIommuPte
/// The concept of PTE in AMD-Vi is drastically different from AMD-V NPT, Intel EPT and Intel VT-d. \
/// In AMD-Vi, PTE simply means a terminal level in the map. It determines the base of the page. \
/// In other words, PTE in AMD-Vi is equivalent to PTE, large PDE and huge PDPTE in AMD-V NPT.
#[bitfield(u64)] pub struct AmdIommuPte
{
	pub pr:bool,
	#[bits(4)] rsvd0:u64,
	pub a:bool,
	pub d:bool,
	#[bits(2)] rsvd1:u64,
	#[bits(3)] next_level:u64,
	#[bits(40)] pub page_address:u64,
	#[bits(7)] rsvd2:u64,
	pub u:bool,
	pub fc:bool,
	pub ir:bool,
	pub iw:bool,
	rsvd3:bool
}

impl AmdIommuPte
{
	pub fn new_pte(page_base:u64,read:bool,write:bool)->Self
	{
		let mut template=Self(page_base);
		template.set_pr(true);
		template.set_ir(read);
		template.set_iw(write);
		template
	}
}

pub union AmdIommuPxe
{
	pub pde:AmdIommuPde,
	pub pte:AmdIommuPte
}

pub struct AmdIommuPmlDescriptor<const N:u8>
{
	pub pxe:MemoryDescriptor<1,AmdIommuPxe>,
	pub gpa_start:u64
}

impl<const N:u8> AmdIommuPmlDescriptor<N>
{
	pub(super) fn compare_gpa(&self,gpa:u64)->core::cmp::Ordering
	{
		use core::cmp::Ordering;
		let size:u64=1<<(PAGE_SHIFT+PAGE_SHIFT_DIFF*(N+1));
		if gpa<self.gpa_start
		{
			Ordering::Greater
		}
		else if gpa>=self.gpa_start+size
		{
			Ordering::Less
		}
		else
		{
			Ordering::Equal
		}
	}

	pub(super) fn new_as_pde(gpa:u64)->Self
	{
		let pxe:MemoryDescriptor<1,AmdIommuPxe>=MemoryDescriptor::alloc().unwrap();
		let size:u64=1<<(PAGE_SHIFT+PAGE_SHIFT_DIFF*N);
		let mask:u64=!(size-1);
		let array:&mut [AmdIommuPde;PAGE_TABLE_ENTRIES]=unsafe{&mut *pxe.virt.cast()};
		trace!("PML{N}E is allocated at 0x{:X}!",pxe.phys);
		for x in array
		{
			*x=AmdIommuPde::new().with_next_level(N as u64);
		}
		Self
		{
			pxe,
			gpa_start:gpa&mask
		}
	}

	pub(super) fn new_as_pte(gpa:u64)->Self
	{
		let pxe:MemoryDescriptor<1,AmdIommuPxe>=MemoryDescriptor::alloc().unwrap();
		let shift=PAGE_SHIFT+PAGE_SHIFT_DIFF*N;
		let size:u64=1<<(shift+PAGE_SHIFT_DIFF);
		let mask=!(size-1);
		let gpa_start=gpa&mask;
		let array:&mut [AmdIommuPte;PAGE_TABLE_ENTRIES]=unsafe{&mut *pxe.virt.cast()};
		trace!("PML{N}E is allocated at 0x{:X}!",pxe.phys);
		for (i,x) in array.iter_mut().enumerate()
		{
			*x=AmdIommuPte::from_bits(gpa_start+(i<<shift) as u64).with_pr(true).with_ir(true).with_iw(true);
		}
		Self
		{
			pxe,
			gpa_start
		}
	}
}