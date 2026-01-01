/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines paging structures for Intel VT-d IOMMU hardware.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;

use crate::xpf_core::{allocator::{ContiguousAllocator, InternalPageAllocator}, nvbdk::MemoryDescriptor};

/// ## Root Entry
/// The Root Table Address Register points to a table of root-entries,
/// when the Translation Table Mode (TTM) field in the register is 00b.
#[bitfield(u128)] pub struct RootEntry
{
	/// ## Present
	/// This field indicates whether the root-entry is present.
	/// - 0: Indicates the root-entry is not present. All other fields are ignored by hardware.
	/// - 1: Indicates the root-entry is present.
	pub p:bool,
	#[bits(11)] rsvd0:u128,
	/// ## Context-Table Pointer
	/// Pointer to Context-table for this bus. The Context-table is 4KB in size and sizealigned.
	/// Hardware treats bits 63:HAW as reserved (0), where HAW is the host address width
	/// of the platform.
	#[bits(52)] pub ctp:u128,
	rsvd1:u64
}

#[bitfield(u128)] pub struct ContextEntry
{
	/// ## Present
	/// - 0: Indicates the context-entry is not present. All other fields except
	/// Fault Processing Disable (FPD) field are ignored by hardware.
	/// - 1: Indicates the context-entry is present.
	pub p:bool,
	/// ## Fault Processing Disable
	/// Enables or disables recording/reporting of qualified non-recoverable faults.
	/// - 0: Qualified non-recoverable faults are recorded/reported for requests processed through this context-entry.
	/// - 1: Qualified non-recoverable faults are not recorded/reported for requests processed through this context-entry.
	/// 
	/// This field is evaluated by hardware irrespective of the setting of the present (P) field.
	pub fpd:bool,
	/// ## Translation Type
	/// This field is applicable only for requests-without-PASID, as hardware blocks all requests-with-
	/// PASID in legacy mode before they can use context table.
	/// - 00b: Untranslated requests are translated using second-stage paging structures referenced
	/// through the SSPTPTR field. Translated requests and Translation Requests are blocked.
	/// - 01b: Untranslated, Translated and Translation Requests are supported. This encoding is
	/// treated as reserved by hardware implementations not supporting Device-TLBs (DT=0 in
	/// Extended Capability Register).
	/// - 10b: Untranslated requests are processed as pass-through. The SSPTPTR field is ignored
	/// by hardware. Translated and Translation Requests are blocked. This encoding is treated
	/// by hardware as reserved for hardware implementations not supporting Pass Through
	/// (PT=0 in Extended Capability Register).
	/// - 11b: Reserved.
	#[bits(2)] pub tt:u8,
	rsvd0:u8,
	/// ## Second-Stage Page Translation Pointer
	/// When the Translation-Type (TT) field is 00b or 01b, this field points to the base of
	/// second stage paging entries (described in Section 9.8).
	/// 
	/// Hardware treats bits 63:HAW as reserved (0), where HAW is the host address width of the platform.
	/// 
	/// This field is ignored by hardware when Translation-Type (TT) field is 10b (pass-through).
	#[bits(52)] pub ssptptr:u64,
	/// ## Address-Width
	/// When the Translation-type (TT) field is 00b or 01b, this field indicates the adjusted
	/// guest address- width (AGAW) to be used by hardware for the second-stage page-table walk.
	/// The following encodings are defined for this field:
	/// - 000b: Reserved
	/// - 001b: 39-bit AGAW (3-level page table)
	/// - 010b: 48-bit AGAW (4-level page table)
	/// - 011b: 57-bit AGAW (5-level page table)
	/// - 100b-111b: Reserved
	/// 
	/// The value specified in this field must match an AGAW value supported by hardware
	/// (as reported in the SAGAW field in the Capability Register).
	/// 
	/// When the Translation-type (TT) field indicates pass-through processing (10b), this field must
	/// be programmed to indicate the largest AGAW value supported by hardware.
	/// 
	/// Untranslated requests-without-PASID processed through this context-entry and accessing
	/// addresses above 2^X-1 (where X is the AGAW value indicated by this field) are blocked and
	/// treated as translation faults.
	#[bits(3)] pub aw:u8,
	/// ## Ignored
	/// Hardware ignores the programming of this field.
	#[bits(4)] pub ign:u8,
	rsvd1:bool,
	/// ## Domain Identifier
	/// Identifier for the domain to which this context-entry maps. Hardware may use the domain
	/// identifier to tag its internal caches.
	/// 
	/// The Capability Register reports the domain-id width supported by hardware. For
	/// implementations supporting less than 16-bit domain-ids, unused bits of this field are treated
	/// as reserved by hardware. For example, for implementation supporting 8-bit domain-ids, bits
	/// 87:80 of this field are treated as reserved.
	/// 
	/// Context-entries programmed with the same domain identifier must always reference same
	/// address translation (SSPTPTR field). Context-entries referencing same address translation are
	/// recommended to be programmed with same domain id for hardware efficiency.
	/// 
	/// When Caching Mode (CM) field in Capability Register is reported as Set, the domain-id value of
	/// zero is architecturally reserved. Software must not use domain-id value of zero when CM is Set.
	pub did:u16,
	#[bits(40)] rsvd2:u64
}

#[bitfield(u64)] pub struct SsPml5e
{
	pub r:bool,
	pub w:bool,
	pub x:bool,
	#[bits(4)] pub ign0:u8,
	rsvd0:bool,
	pub a:bool,
	#[bits(2)] pub ign1:u8,
	rsvd1:bool,
	#[bits(40)] pub pml4_base:u64,
	#[bits(10)] pub ign2:u64,
	rsvd2:bool,
	pub ign3:bool
}

#[bitfield(u64)] pub struct SsPml4e
{
	pub r:bool,
	pub w:bool,
	pub x:bool,
	#[bits(4)] pub ign0:u8,
	rsvd0:bool,
	pub a:bool,
	#[bits(2)] pub ign1:u8,
	rsvd1:bool,
	#[bits(40)] pub pdpt_base:u64,
	#[bits(10)] pub ign2:u64,
	rsvd2:bool,
	pub ign3:bool
}

#[bitfield(u64)] pub struct SsHugePdpe
{
	pub r:bool,
	pub w:bool,
	pub x:bool,
	#[bits(3)] pub emt:u8,
	pub ipat:bool,
	pub ps:bool,
	pub a:bool,
	pub d:bool,
	pub ign1:bool,
	pub snp:bool,
	#[bits(18)] rsvd1:u64,
	#[bits(22)] pub page_base:u64,
	#[bits(10)] pub ign2:u64,
	rsvd2:bool,
	pub ign3:bool
}

#[bitfield(u64)] pub struct SsPdpe
{
	pub r:bool,
	pub w:bool,
	pub x:bool,
	#[bits(4)] pub ign0:u8,
	pub ps:bool,
	pub a:bool,
	#[bits(2)] pub ign1:u8,
	rsvd1:bool,
	#[bits(40)] pub pd_base:u64,
	#[bits(10)] pub ign2:u64,
	rsvd2:bool,
	pub ign3:bool
}

#[bitfield(u64)] pub struct SsLargePde
{
	pub r:bool,
	pub w:bool,
	pub x:bool,
	#[bits(3)] pub emt:u8,
	pub ipat:bool,
	pub ps:bool,
	pub a:bool,
	pub d:bool,
	pub ign1:bool,
	pub snp:bool,
	#[bits(9)] rsvd1:u64,
	#[bits(31)] pub page_base:u64,
	#[bits(10)] pub ign2:u64,
	rsvd2:bool,
	pub ign3:bool
}

#[bitfield(u64)] pub struct SsPde
{
	pub r:bool,
	pub w:bool,
	pub x:bool,
	#[bits(4)] pub ign0:u8,
	pub ps:bool,
	pub a:bool,
	#[bits(2)] pub ign1:u8,
	rsvd1:bool,
	#[bits(40)] pub pt_base:u64,
	#[bits(10)] pub ign2:u64,
	rsvd2:bool,
	pub ign3:bool
}

#[bitfield(u64)] pub struct SsPte
{
	pub r:bool,
	pub w:bool,
	pub x:bool,
	#[bits(3)] pub emt:u8,
	pub ipat:bool,
	pub ign0:bool,
	pub a:bool,
	pub d:bool,
	pub ign1:bool,
	pub snp:bool,
	#[bits(40)] pub page_base:u64,
	#[bits(10)] pub ign2:u64,
	rsvd2:bool,
	pub ign3:bool
}

// TODO: Add Scalable-Mode-related structures when NoirVisor really uses them.
// - Scalable-Mode Root Entry
// - Scalable-Mode PASID Directory Entry
// - Scalable-Mode PASID Table Entry
// - First-Stage Paging Entry

// The following structures are NoirVisor's paging manager.

pub struct VtdPageTableDescriptor<T:Sized,A:ContiguousAllocator=InternalPageAllocator>
{
	pub table:MemoryDescriptor<1,T,A>,
	pub gpa_start:u64,
}

impl<T> Default for VtdPageTableDescriptor<T>
{
	fn default()->Self
	{
		Self
		{
			table:MemoryDescriptor::null(),
			gpa_start:0
		}
	}
}