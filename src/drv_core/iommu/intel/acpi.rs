/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines ACPI tables for Intel VT-d IOMMU hardware.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;

use crate::drv_core::acpi::tables::SystemDescriptionHeader;

#[bitfield(u8)] pub struct DmarTableFlags
{
	/// If Clear, the platform does not support interrupt
	/// remapping. If Set, the platform supports interrupt remapping.
	pub intr_remap:bool,
	/// X2APIC_OPT_OUT - For firmware compatibility reasons, platform
	/// firmware may Set this field to request system software to opt out of
	/// enabling Extended xAPIC (X2APIC) mode. This field is valid only when
	///	the INTR_REMAP field (bit 0) is Set. Since firmware is permitted to hand
	/// off platform to system software in legacy xAPIC mode, system software
	/// is required to check this field as Clear as part of detecting X2APIC mode
	/// support in the platform.
	pub x2apic_opt_out:bool,
	/// DMA_CTRL_PLATFORM_OPT_IN_FLAG: Platform firmware is
	/// recommended to Set this field to report any platform initiated DMA is
	/// restricted to only reserved memory regions (reported in RMRR
	/// structures) when transferring control to system software such as on
	/// ExitBootServices(). System software may program DMA remapping
	/// hardware to block DMA outside of RMRR, except for memory explicitly
	/// registered by device drivers with system software.
	pub dma_ctrl_platform_opt_in_flag:bool,
	#[bits(5)] rsvd:u8
}

#[repr(C,packed)] pub struct DmarTable
{
	pub header:SystemDescriptionHeader,
	/// This field indicates the maximum DMA physical addressability supported by
	/// this platform. The system address map reported by the BIOS indicates what
	/// portions of this addresses are populated. \
	/// The Host Address Width (HAW) of the platform is computed as (N+1), where
	/// N is the value reported in this field. For example, for a platform supporting
	/// 40 bits of physical addressability, the value of 100111b is reported in this
	/// field.
	pub host_addr_width:u8,
	/// A 8-bit Flags
	pub flags:DmarTableFlags,
	rsvd:[u8;10]
	// Remapping structures goes here.
}

impl DmarTable
{
	/// Returns an iterator that iterates over DMAR remapping tables.
	pub const fn iter_tables(&self)->DmarIterator<'_>
	{
		DmarIterator
		{
			source:self,
			offset:size_of::<Self>()
		}
	}
}

pub struct DmarIterator<'a>
{
	source:&'a DmarTable,
	offset:usize
}

impl<'a> Iterator for DmarIterator<'a>
{
	// ACPI Table items always exist. So it's safe to assume they have static lifetime.
	type Item = &'static DmarRemappingStructure;

	fn next(&mut self)->Option<Self::Item>
	{
		if self.offset<self.source.header.get_length() as usize
		{
			let rs:&DmarRemappingStructure=unsafe{&*(self.source as *const DmarTable).byte_add(self.offset).cast()};
			self.offset+=unsafe{rs.head.len()};
			Some(rs)
		}
		else
		{
			None
		}
	}
}

#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct RemappingTypeHeader
{
	struct_type:u16,
	length:u16,
}

impl RemappingTypeHeader
{
	/// Returns the type of this remapping structure. Can be one of the following
	/// - TYPE_DRHD: DMA Remapping Hardware Unit Definition Structure
	/// - TYPE_RMRR: Reserved Memory Region Reporting Structure
	/// - TYPE_ATSR: Root Port ATS Capability Reporting Structure
	/// - TYPE_RHSA: Remapping Hardware Static Affinity Structure
	/// - TYPE_ANDD: ACPI Namespace Device Declaration Structure
	/// - TYPE_SATC: SoC Integrated Address Translation Cache Reporting Structure
	/// - TYPE_SIDP: SoC Integrated Device Property Reporting Structure
	pub fn r#type(&self)->u16
	{
		self.struct_type
	}

	/// Returns the length of this structure in bytes.
	pub fn len(&self)->usize
	{
		self.length as usize
	}

	pub const TYPE_DRHD:u16=0;
	pub const TYPE_RMRR:u16=1;
	pub const TYPE_ATSR:u16=2;
	pub const TYPE_RHSA:u16=3;
	pub const TYPE_ANDD:u16=4;
	pub const TYPE_SATC:u16=5;
	pub const TYPE_SIDP:u16=6;
}

#[bitfield(u8)] pub struct DeviceScopeFlags
{
	/// For this Source ID, it is recommended that system software not program the PGTT field in
	/// the PASID Table entry, associated with RID_PASID, with a value of 011b (Nested Translation).
	pub req_wo_pasid_nested_not_allowed:bool,
	pub req_wo_pasid_pwsnp_not_allowed:bool,
	pub req_wo_pasid_pgsnp_not_allowed:bool,
	pub atc_hardened:bool,
	pub atc_required:bool,
	#[bits(3)] rsvd:u8
}

#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct DeviceScope
{
	/// Type of this device scope. Can be one of the following:
	/// - TYPE_PCI_ENDPOINT (0x01): PCI Endpoint Device
	/// - TYPE_PCI_SUB_HIERARCHY (0x02): PCI Sub-hierarchy
	/// - TYPE_IOAPIC (0x03): IOAPIC Device
	/// - TYPE_MSI_CAPABLE_HPET (0x04): MSI-capable HPET Device
	/// - TYPE_ACPI_NAMESPACE_DEVICE (0x05): ACPI Namespace Device
	/// 
	/// Other types are reserved for future use.
	pub r#type:u8,
	/// Length, in bytes, of this Device Scope entry, including
	/// the header and the Path field.
	pub length:u8,
	/// Flags associated with this Device Scope entry. Note that this field is reserved when:
	/// - This device scope entry appears outside of an SIDP structure.
	/// - The type field is 0x01 (PCI Endpoint Device) or 0x05 (ACPI Namespace Device).
	pub flags:DeviceScopeFlags,
	rsvd:u8,
	/// When the Type field indicates IOAPIC Device (0x03), this field provides the I/O APIC-ID
	/// as provided in the I/O APIC structure of the ACPI Multiple APIC Description Table (MADT).
	/// 
	/// When the Type field indicates MSI-capable HPET Device (0x04), this field provides the
	/// "HPET Number" as provided in the HPET ACPI table for the corresponding Timer Block.
	/// 
	/// When the Type field indicates ACPI Namespace Device (0x05), this field provides the
	/// "ACPI Device Number" as provided in the ACPI Namespace Device Declaration Table (ANDD)
	/// structure for the corresponding ACPI device.
	/// 
	/// This field is reserved for all other Type values.
	pub enum_id:u8,
	/// This field indicates the bus number (bus number of the first PCI Bus produced by the
	/// PCI Host Bridge) under which the device identified by this Device Scope entry resides.
	/// 
	/// For Device Scope Entries with Type value of 0x4 (HPET), this field describes the upper 8 bits {Bus,
	/// Device} of the unique 16-bit source-id allocated by the platform for the MSI-capable HPET Timer Block.
	///
	/// For Device Scope Entries with Type value of 0x5 (ACPI_NAMESPACE_DEVICE), this field describes the upper 8 bits {Bus,
	/// Device} of the unique 16-bit source-id allocated by the platform for the ACPI name-space device.
	pub start_bus_num:u8,
	// Path starts here.
	path:[u16;0]
}

impl DeviceScope
{
	/// For Device Scope Entries with Type value of 0x1, 0x2 or 0x3 this field
	/// describes the hierarchical path from the Host Bridge to the device
	/// specified by the Device Scope Entry.
	/// 
	/// For example, a device in a N-deep hierarchy is identified by N {PCI Device
	/// Number, PCI Function Number} pairs, where N is a positive integer. Even
	/// offsets contain the Device numbers, and odd offsets contain the Function
	/// numbers.
	/// 
	/// The first {Device, Function} pair resides on the bus identified by the ‘Start
	/// Bus Number’ field. Each subsequent pair resides on the bus directly
	/// behind the bus of the device identified by the previous pair. The identity
	/// (Bus, Device, Function) of the target device is obtained by recursively
	/// walking down these N {Device, Function} pairs.
	/// 
	/// If the ‘Path’ field length is 2 bytes (N=1), the Device Scope Entry identifies
	///  ‘Root-Complex Integrated Device’. The requester-id of ‘Root-Complex
	/// Integrated Devices’ are static and not impacted by system software bus
	/// rebalancing actions.
	/// 
	/// If the ‘Path’ field length is more than 2 bytes (N > 1), the Device Scope
	/// Entry identifies a device behind one or more system software visible PCIPCI
	/// . Bus rebalancing actions by system software modifying bus
	/// assignments of the device’s parent bridge impacts the bus number portion
	/// of device’s requester-id.
	/// 
	/// For Device Scope Entries with Type value of 0x4 (HPET) this field describes
	/// the lower 8 bits {Device, Function} of the unique 16-bit source-id
	/// allocated by the platform for the MSI-capable HPET Timer Block.
	/// For Device Scope Entries with Type value of 0x5
	/// (`ACPI_NAMESPACE_DEVICE`) this field describes the lower 8 bits {Device,
	/// Function} of the unique 16-bit source-id allocated by the platform for the
	/// ACPI name-space device.
	/// 
	/// Note that `path` is an unaligned slice. Since.
	pub const fn iter_path(&self)->DeviceScopePathIter<'_>
	{
		DeviceScopePathIter
		{
			source:self,
			index:0
		}
	}

	pub const TYPE_PCI_ENDPOINT:u8=0x01;
	pub const TYPE_PCI_SUB_HIERARCHY:u8=0x02;
	pub const TYPE_IOAPIC:u8=0x03;
	pub const TYPE_MSI_CAPABLE_HPET:u8=0x04;
	pub const TYPE_ACPI_NAMESPACE_DEVICE:u8=0x05;
}

/// This iterator would iterate all path items in `DeviceScope`.
pub struct DeviceScopePathIter<'a>
{
	source:&'a DeviceScope,
	index:usize
}

impl Iterator for DeviceScopePathIter<'_>
{
	type Item = u16;

	fn next(&mut self)->Option<Self::Item>
	{
		let limit=(self.source.length as usize-size_of::<Self>())>>1;
		if self.index<limit
		{
			let base:*const u16=(&raw const self.source.path).cast();
			let r=unsafe{base.add(self.index).read_unaligned()};
			self.index+=1;
			Some(r)
		}
		else
		{
			None
		}
	}

	fn nth(&mut self,n:usize)->Option<Self::Item>
	{
		self.index+=n;
		self.next()
	}

	fn count(self)->usize where Self:Sized
	{
		(self.source.length as usize-size_of::<Self>())>>1
	}

	fn last(self)->Option<Self::Item> where Self:Sized
	{
		let limit=(self.source.length as usize-size_of::<Self>())>>1;
		if limit>0
		{
			let base:*const u16=(&raw const self.source.path).cast();
			Some(unsafe{base.add(limit-1).read_unaligned()})
		}
		else
		{
			None
		}
	}
}

#[bitfield(u8)] pub struct DrhdFlags
{
	/// If clear, this remapping hardware unit has under its scope only devices
	/// in the specified PCI Segment that are explicitly identified through the
	/// "Device Scope" entries of this structure. The device can be of any type
	/// as described by the "Type" field of the Device Scope entry including (
	/// but not limited to) I/O APIC and HPET.
	/// 
	/// If set, this remapping hardware unit has under its scope all PCI-compatible
	/// devices in the specified PCI Segment, except devices explicitly reported
	/// under its scope of other remapping hardware units for the same Segment. If
	/// a DRHD structure with this flag set is reported for a Segment, it must be
	/// enumerated by BIOS after all other DRHD structures for the same Segment.
	/// A DRHD structure with this flag set may use the Device Scope fields to
	/// enumerate I/O APIC and HPET devices under its scope.
	pub include_pci_all:bool,
	#[bits(7)] rsvd:u8
}

#[bitfield(u8)] pub struct DrhdSize
{
	/// Indicates the size of this remapping hardware register set for this remapping
	/// hardware unit. The size is reported in units of 4-KiB pages.
	#[bits(4)] pub size:u8,
	#[bits(4)] rsvd:u8
}

/// A DMA-remapping hardware unit definition structure (DRHD) structure uniquely
/// represents a remapping hardware unit present in the platform. There must be
/// at least one DRHD structure for each PCI Segment present in the platform.
#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct DrhdTable
{
	header:RemappingTypeHeader,
	/// 8-bit flags for DRHD structure.
	pub flags:DrhdFlags,
	/// Indicates the size of BAR pages.
	pub size:DrhdSize,
	/// The PCI Segment associated with this remapping hardware unit.
	pub segment:u16,
	/// Base Address Register (BAR) for this remapping hardware unit.
	/// 
	/// This address must be aligned according to the size of the register set
	/// size reported in the `size` field of this structure.
	pub bar:u64,
	// Device scope goes here.
	device_scope:[DeviceScope;0]
}

/// BIOS allocated reserved memory ranges may be DMA targets. BIOS may report each such reserved memory
/// region through the RMRR structures, along with the devices that requires access to the specified
/// reserved memory region. Reserved memory ranges that are either not DMA targets, or memory ranges
/// that may be target of BIOS initiated DMA only during pre-boot phase (such as from a boot disk drive)
/// must not be included in the reserved memory region reporting. The base address of each RMRR region
/// must be 4KB aligned and the size must be an integer multiple of 4KB.
/// 
/// BIOS must report the RMRR reported memory addresses as reserved (or as EFI runtime) in the
/// system memory map returned through methods such as INT15, EFI GetMemoryMap etc. The reserved
/// memory region reporting structures are optional. If there are no RMRR structures, the system
/// software concludes that the platform does not have any reserved memory ranges that are DMA
/// targets.
/// 
/// The RMRR regions are expected to be used for legacy usages (such as USB, UMA Graphics, etc.)
/// requiring reserved memory. Platform designers should avoid or limit use of reserved memory regions
/// since these require system software to create holes in the DMA virtual address range available to
/// system software and its drivers.
#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct RmrrTable
{
	header:RemappingTypeHeader,
	rsvd:u16,
	/// PCI Segment Number associated with devices identified through the Device
	/// Scope entries of this structure.
	pub segment:u16,
	/// Base Address of 4KiB-aligned reserved memory region.
	pub region_base:u64,
	/// Last Address of reserved memory region. Value in this field must be larger
	/// than to the value in the `region_base` field.
	pub region_limit:u64,
	device_scope:[DeviceScope;0]
}

#[bitfield(u8)] pub struct AtsrFlags
{
	pub all_ports:bool,
	#[bits(7)] rsvd:u8
}

/// This structure is applicable only for platforms supporting Device-TLBs as reported through the
/// Extended Capability Register. For each PCI Segment in the platform that supports Device-TLBs, BIOS
/// provides an ATSR structure. The ATSR structures identifies PCI Express Root-Ports supporting Address
/// Translation Services (ATS) transactions. Software must enable ATS on endpoint devices behind a Root
/// Port only if the Root Port is reported as supporting ATS transactions.
#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct AtsrTable
{
	header:RemappingTypeHeader,
	flags:AtsrFlags,
	rsvd:u8,
	segment:u16,
	device_scope:[DeviceScope;0]
}

/// Remapping Hardware Status Affinity (RHSA) structure is applicable for platforms supporting nonuniform
/// memory (NUMA), where Remapping hardware units spans across nodes. This optional structure provides
/// the association between each Remapping hardware unit (identified by its respective Base Address) and
/// the proximity domain to which that hardware unit belongs. Such platforms, report the proximity of
/// processor and memory resources using ACPI Static Resource Affinity (SRAT) structure. To optimize
/// remapping hardware performance, software may allocate translation structures referenced by a remapping
/// hardware unit from memory in the same proximity domain. Similar to SRAT, the information in the RHSA
/// structure is expected to be used by system software during early initialization, when evaluation of
/// objects in the ACPI name-space is not yet possible.
#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct RhsaTable
{
	header:RemappingTypeHeader,
	rsvd:u32,
	pub bar:u64,
	pub proximity_domain:u32
}

/// An ACPI Name-space Device Declaration (ANDD) structure uniquely represents an ACPI name-space
/// enumerated device capable of issuing DMA requests in the platform. ANDD structures are used in
/// conjunction with Device-Scope entries of type ACPI_NAMESPACE_DEVICE.
#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct AnddTable
{
	header:RemappingTypeHeader,
	rsvd:u8,
	pub device_number:u8,
	object_name:[u8;0]
}

#[bitfield(u8)] pub struct SatcFlags
{
	pub atc_required:bool,
	#[bits(7)] rsvd:u8
}

/// The SoC Integrated Address Translation Cache (SATC) reporting structure identifies devices that have
/// address translation cache (ATC), as defined by the PCI Express Base Specification, and that is
/// validated per requirements described in Device TLB in System-on-Chip (SoC) Integrated Devices.
#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct SatcTable
{
	header:RemappingTypeHeader,
	pub flags:SatcFlags,
	rsvd:u8,
	pub segment:u16,
	device_scope:[DeviceScope;0]
}

/// The SoC Integrated Device Property (SIDP) reporting structure identifies devices that have special
/// properties and that may put restrictions on how system software must configure remapping structures
/// that govern such devices in a platform where remapping hardware is enabled. SIDP uses the Flags
/// field in the Device Scope Entry to indicate the special properties of each device.
/// 
/// System software that uses the SIDP structure must ignore SATC structures that may also be present.
#[derive(Clone, Copy)]
#[repr(C,packed)] pub struct SidpTable
{
	header:RemappingTypeHeader,
	rsvd:u16,
	pub segment:u16,
	device_scope:[DeviceScope;0]
}

/// This union represents all possible DMAR remapping structures. Use `head` field to
/// determine the actual type and length of this structure.
#[repr(C,packed)] pub union DmarRemappingStructure
{
	/// Use this field when you don't know what this structure is.
	pub head:RemappingTypeHeader,
	/// Use this field if the type is DMA-Remapping Hardware Unit Definition (DRHD) structure.
	pub drhd:DrhdTable,
	/// Use this field if the type is Reserved Memory Region Reporting (RMRR) structure.
	pub rmrr:RmrrTable,
	/// Use this field if the type is Root Port ATS Capability Reporting (ATSR) structure.
	pub atsr:AtsrTable,
	/// Use this field if the type is Remapping Hardware Static Affinity (RHSA) structure.
	pub rhsa:RhsaTable,
	/// Use this field if the type is ACPI Namespace Device Declaration (ANDD) structure.
	pub andd:AnddTable,
	/// Use this field if the type is SoC Integrated Address Translation Cache (SATC) structure.
	pub satc:SatcTable,
	/// Use this field if the type is SoC Integrated Device Property Reporting (SIDP) structure.
	pub sidp:SidpTable
}
