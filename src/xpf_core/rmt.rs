/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines the reverse-mapping tables of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::mem::ManuallyDrop;

use bitfield_struct::bitfield;
use log::{error, trace};
use spin::MutexGuard;

use crate::xpf_core::{allocator::{PAGE_ALLOC_MANAGER, PageAllocationManager}, ci::CI_MANAGER, nvbdk::{MemoryDescriptor, PAGE_SHIFT, SYSTEM_PHYSICAL_MEMORY_RANGES, page_count, page_mult}};

#[bitfield(u64)] pub struct RmtIntermediateEntryHigh
{
	#[bits(11)] rsvd:u64,
	/// Indicates whether this entry is splitted. \
	/// Must be set for Intermediate Entries.
	pub split:bool,
	#[bits(52)] phys:u64
}

#[derive(Clone, Copy, Default)]
#[repr(C,align(16))] pub struct RmtIntermediateEntry
{
	pub hva:*mut RmtEntry,
	pub high:RmtIntermediateEntryHigh
}

#[bitfield(u128)] pub struct RmtTerminalEntry
{
	pub asid:u32,
	rsvd0:u32,
	pub ownership:u8,
	#[bits(2)] rsvd1:u8,
	pub share:bool,
	/// Indicates whether this entry is splitted. \
	/// Must be cleared for Terminal Entries.
	pub split:bool,
	#[bits(52)] pub gpa:u64
}

impl RmtTerminalEntry
{
	/// Indicates this page is assigned to the subverted host. \
	/// Pages assigned to the subverted host can be later reassigned to any ownership.
	pub const OWNERSHIP_SUBVERTED_HOST:u8=0;
	/// Indicates this page is assigned to the NoirVisor. \
	/// Pages assigned to the NoirVisor must not be reassigned in any circumstances.
	pub const OWNERSHIP_NOIRVISOR:u8=1;
	/// Indicates this page is assigned to the CVM Guest. \
	/// Pages assigned to the CVM Guest can be reassigned to the subverted host once
	/// the CVM Guest is terminated. If shared, this page must be unmapped among all
	/// shared CVM guests before reassigning back to the subverted host.
	pub const OWNERSHIP_CVM_GUEST:u8=2;
	/// Indicates this page is assigned to the Parhelion Guest. \
	/// Pages assigned to the Parhelion Guest are inaccessible to the subverted host,
	/// CVM Guests and other Parhelion Guests, and cannot be shared. To reassign back
	/// to the subverted host, it must be unmapped without preserving original data.
	pub const OWNERSHIP_PARHELION_GUEST:u8=3;
}

pub union RmtEntry
{
	intermediate:RmtIntermediateEntry,
	terminal:RmtTerminalEntry
}

impl RmtEntry
{
	pub fn new_terminal(gpa:u64,asid:u32,ownership:u8)->Self
	{
		Self{terminal:RmtTerminalEntry::new().with_asid(asid).with_ownership(ownership).with_gpa(page_count(gpa))}
	}

	pub fn new_intermediate(lk:&mut Option<MutexGuard<PageAllocationManager>>)->Option<Self>
	{
		let md_opt=match lk
		{
			Some(lk)=>MemoryDescriptor::alloc_with_lock(lk),
			None=>MemoryDescriptor::alloc()
		};
		if let Some(md)=md_opt
		{
			let md:ManuallyDrop<MemoryDescriptor<1,RmtEntry>>=ManuallyDrop::new(md);
			Some
			(
				Self
				{
					intermediate:RmtIntermediateEntry
					{
						hva:md.virt,
						high:RmtIntermediateEntryHigh::from_bits(md.phys).with_split(true)
					}
				}
			)
		}
		else
		{
			None
		}
	}
}

pub struct ReverseMappingTableRoot
{
	pub root:MemoryDescriptor<1,RmtEntry>,
	pub levels:u8
}

const fn pfn_shift(level:u8)->u8
{
	(level<<3)+PAGE_SHIFT
}

const fn pfn_size(level:u8)->u64
{
	1<<pfn_shift(level)
}

const fn pfn_mask(level:u8)->u64
{
	pfn_size(level)-1
}

const fn pfn_high_mask(level:u8)->u64
{
	!pfn_mask(level)
}

const fn pfn_index(addr:u64,level:u8)->usize
{
	((addr>>pfn_shift(level))&0xFF) as usize
}

const fn pfn_mult(index:usize,level:u8)->u64
{
	(index as u64)<<pfn_shift(level)
}

const fn pfn_base(addr:u64,level:u8)->u64
{
	addr&pfn_high_mask(level)
}

impl ReverseMappingTableRoot
{
	#[allow(clippy::new_without_default)]
	pub fn new()->Self
	{
		// Find the address of Top-of-Memory (TOM).
		let mut tom:u64=0;
		for x in SYSTEM_PHYSICAL_MEMORY_RANGES.iter()
		{
			let top=x.start+x.length;
			if top>tom
			{
				tom=top;
			}
		}
		trace!("TOM: 0x{tom:X}");
		// Now that we know the address of TOM, infer the maximum level we need.
		let mut levels:u8=0;
		tom>>=20;
		while tom>0
		{
			levels+=1;
			tom>>=8;
		}
		trace!("RMT is {levels}-level table!");
		let md:MemoryDescriptor<1,RmtEntry>=MemoryDescriptor::alloc().unwrap();
		trace!("RMT is allocated at {:p}",md.virt);
		let s:&mut [RmtEntry;256]=unsafe{&mut *md.virt.cast()};
		// As initialization, assign all pages to subverted host.
		for (i,x) in s.iter_mut().enumerate()
		{
			*x=RmtEntry::new_terminal(pfn_mult(i,levels),1,RmtTerminalEntry::OWNERSHIP_SUBVERTED_HOST);
		}
		Self
		{
			root:md,
			levels
		}
	}

	pub fn assign_for_noirvisor(&mut self)
	{
		// Assign the ownership of NoirVisor image and allocated pages to NoirVisor.
		for p in CI_MANAGER.read().into_iter()
		{
			let phys=page_mult(p.pfn());
			self.assign_4kb_page(phys,0,RmtTerminalEntry::OWNERSHIP_NOIRVISOR);
		}
		// Assign the ownership of allocated pages to NoirVisor.
		for p in PAGE_ALLOC_MANAGER.lock().iter()
		{
			let mut lk=None;
			self.assign_2mb_page(&mut lk,p,0,RmtTerminalEntry::OWNERSHIP_NOIRVISOR);
		}
	}

	pub fn assign_2mb_page(&mut self,lk:&mut Option<MutexGuard<'_,PageAllocationManager>>,gpa:u64,asid:u32,ownership:u8)->bool
	{
		let max_shift:u8=(self.levels<<3)+PAGE_SHIFT+8;
		let max_addr:u64=1<<max_shift;
		if gpa>=max_addr
		{
			error!("GPA (0x{gpa:X} is too big! (ToM: 0x{max_addr:X}) Cannot reassign ownership!");
			return false;
		}
		let mut rmt:&mut [RmtEntry;256]=unsafe{&mut *self.root.virt.cast()};
		for cur_level in (0..=self.levels).rev()
		{
			let i=pfn_index(gpa,cur_level);
			if cur_level==1
			{
				// Last level.
				rmt[i]=RmtEntry::new_terminal(gpa,asid,ownership);
				return true;
			}
			else
			{
				// Not the last level, split the entry.
				match RmtEntry::new_intermediate(lk)
				{
					Some(e)=>
					{
						let (old_asid,old_ownership)=unsafe{(rmt[i].terminal.asid(),rmt[i].terminal.ownership())};
						rmt[i]=e;
						let base=pfn_base(gpa,cur_level);
						rmt=unsafe{&mut *rmt[i].intermediate.hva.cast()};
						for (i,x) in rmt.iter_mut().enumerate()
						{
							*x=RmtEntry::new_terminal(base+pfn_mult(i,cur_level-1),old_asid,old_ownership);
						}
					}
					None=>return false
				}
			}
		}
		panic!("Shouldn't reach here!");
	}

	pub fn assign_4kb_page(&mut self,gpa:u64,asid:u32,ownership:u8)->bool
	{
		let max_shift:u8=(self.levels<<3)+PAGE_SHIFT+8;
		let max_addr:u64=1<<max_shift;
		if gpa>=max_addr
		{
			error!("GPA (0x{gpa:X} is too big! (ToM: 0x{max_addr:X}) Cannot reassign ownership!");
			return false;
		}
		let mut rmt:&mut [RmtEntry;256]=unsafe{&mut *self.root.virt.cast()};
		for cur_level in (0..=self.levels).rev()
		{
			let i=pfn_index(gpa,cur_level);
			if cur_level==0
			{
				// Last level.
				rmt[i]=RmtEntry::new_terminal(gpa,asid,ownership);
				return true;
			}
			else
			{
				// Not the last level, split the entry.
				let mut lk=None;
				match RmtEntry::new_intermediate(&mut lk)
				{
					Some(e)=>
					{
						let (old_asid,old_ownership)=unsafe{(rmt[i].terminal.asid(),rmt[i].terminal.ownership())};
						rmt[i]=e;
						let base=pfn_base(gpa,cur_level);
						rmt=unsafe{&mut *rmt[i].intermediate.hva.cast()};
						for (i,x) in rmt.iter_mut().enumerate()
						{
							*x=RmtEntry::new_terminal(base+pfn_mult(i,cur_level-1),old_asid,old_ownership);
						}
					}
					None=>return false
				}
			}
		}
		panic!("Shouldn't reach here!");
	}
}

// TODO: implement drop trait for ReverseMappingTableRoot
