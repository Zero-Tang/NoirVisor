/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines registers for Intel VT-d IOMMU hardware.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;

use crate::xpf_core::asm::io::{mmio_read,mmio_write};

pub(super) trait IommuRegister
{
	/// Offset of this register to the IOMMU BAR.
	const MMIO_OFFSET:usize;

	/// Performs an MMIO-read to obtain the register value.
	/// 
	/// Unfortunately, this method cannot be provided by trait because
	/// the size of `Self` is unknown at compilation time. \
	/// Use `derive_mmio_rw` macro to automatically derive the implementation.
	unsafe fn read(base:u64)->Self;
	/// Performs an MMIO-write to change the register value.
	/// 
	/// Unfortunately, this method cannot be provided by trait because
	/// the size of `Self` is unknown at compilation time. \
	/// Use `derive_mmio_rw` macro to automatically derive the implementation.
	unsafe fn write(base:u64,value:Self);
}

/// Derives the implementation of `IommuRegister` trait.
macro_rules! derive_mmio_rw
{
	() =>
	{
		unsafe fn read(base:u64)->Self
		{
			unsafe
			{
				mmio_read((base+Self::MMIO_OFFSET as u64) as *const Self)
			}
		}

		unsafe fn write(base:u64,value:Self)
		{
			unsafe
			{
				mmio_write((base+Self::MMIO_OFFSET as u64) as *mut Self,value)
			}
		}
	};
}

#[bitfield(u32)] pub(super) struct VersionRegister
{
	#[bits(4)] pub minor:u32,
	#[bits(4)] pub major:u32,
	#[bits(24)] rsvd:u32
}

impl IommuRegister for VersionRegister
{
	const MMIO_OFFSET:usize = 0x000;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub(super) struct CapabilityRegister
{
	/// # Number of domains supported
	/// - `000b`: Hardware supports 4-bit domain-ids with support for up to 16 domains.
	/// - `001b`: Hardware supports 6-bit domain-ids with support for up to 64 domains.
	/// - `010b`: Hardware supports 8-bit domain-ids with support for up to 256 domains.
	/// - `011b`: Hardware supports 10-bit domain-ids with support for up to 1024 domains.
	/// - `100b`: Hardware supports 12-bit domain-ids with support for up to 4K domains.
	/// - `101b`: Hardware supports 14-bit domain-ids with support for up to 16K domains.
	/// - `110b`: Hardware supports 16-bit domain-ids with support for up to 64K domains.
	/// - `111b`: Reserved.
	#[bits(3)] pub nd:usize,
	rsvd0:bool,
	/// # Required Write-Buffer Flushing
	/// - `false`: Indicates no write-buffer flushing is needed to ensure chagnes to
	/// memory-resident structures are visible to hardware.
	/// - `true`: Indicates software must explicitly flush the write buffers to ensure
	/// updates made to memory-resident remapping structures are visible to hardware.
	pub rwbf:bool,
	/// # Protected Low-Memory Region
	/// - `false`: Indicates protected low-memory region is not supported.
	/// - `true`: Indicates protected low-memory region is supported.
	pub plmr:bool,
	/// # Protected High-Memory Region
	/// - `false`: Indicates protected high-memory region is not supported.
	/// - `true`: Indicates protected high-memory region is supported.
	pub phmr:bool,
	/// # Caching Mode
	/// This field applies to all DMA and Interrupt remap tables except FS-tables. Hardware
	/// will not cache faulting FS-only translations in IOTLB or FS-paging-structure caches.
	/// - `false`: Not-present and erroneous entries are not cached in any of the remapping
	/// caches. Invalidations are not required for modifications to individual not present
	/// or invalid entries. \
	/// However, any modifications that result in decreasing the effective permissions or
	/// partial permission increases require invalidations for them to be effective.
	/// - `true`: Not-present and erroneous mappings may be cached in the remapping caches.
	/// Any software updates to the remapping structures (including updates to "not-present"
	/// or erroneous entries) require explicit invalidation.
	/// 
	/// Hardware implementations of this architecture must support a value of 0 in this field.
	pub cm:bool,
	/// # Supported Adjusted Guest Address Widths
	/// This 5-bit field indicates the supported adjusted guest address widths (which in turn
	/// represents the levels of page-table walks for the 4KB base page size) supported by
	/// the hardware implementation. \
	/// A value of 1 in any of these bits indicates the corresponding adjusted guest address
	/// width is supported. The adjusted guest address widths corresponding to various bit
	/// positions within this field are:
	/// - 0: Reserved
	/// - 1: 39-bit AGAW (3-level page table)
	/// - 2: 48-bit AGAW (4-level page table)
	/// - 3: 57-bit AGAW (5-level page table)
	/// - 4: Reserved
	/// 
	/// Software must ensure that the adjusted guest address width used to set up the page
	/// tables is one of the supported guest address widths reported in this field. \
	/// Hardware implementations reporting second-stage translation support (SSTS) field
	/// as Clear also report this field as 0.
	#[bits(5)] pub sagaw:u64,
	#[bits(3)] rsvd1:u64,
	/// # Maximum Guest Address Width
	/// This field indicates the maximum guest physical address width supported by second-stage
	/// translation in remapping hardware. The Maximum Guest Address Width (MGAW) is computed
	/// as (N+1), where N is the valued reported in this field. For example, a hardware
	/// implementation supporting 48-bit MGAW reports a value of 47 (101111b) in this field.
	/// 
	/// If the value in this field is X, untranslated DMA requests with addresses above
	/// 2(X+1)-1 that are subjected to second-stage translation are blocked by hardware.
	/// Device-TLB translation requests to addresses above 2(X+1)-1 that are subjected to
	/// second-stage translation from allowed devices return a null Translation-Completion
	/// Data with R=W=0.
	/// 
	/// Guest addressability for a given DMA request is limited to the minimum of the value
	/// reported through this field and the adjusted guest address width of the corresponding
	/// page-table structure. (Adjusted guest address widths supported by hardware are
	/// reported through the SAGAW field).
	/// 
	/// Implementations must support MGAW at least equal to the physical addressability
	/// (host address width) of the platform. \
	/// All Root-Complex integrated devices and fabrics must implement at least MGAW address bits.
	#[bits(6)] pub mgaw:u64,
	/// # Zero-Length Read
	/// - `false`: Indicates the remapping hardware unit blocks (and treats as fault)
	/// zero length DMA read requests to write-only pages.
	/// - `true`: Indicates the remapping hardware unit supports
	/// zero length DMA read requests to write-only pages.
	/// 
	/// DMA remapping hardware implementations are recommended to report ZLR field as Set.
	pub zlr:bool,
	/// # Deprecated Bit
	/// This field must be reported as 0 to ensure backward compatibility wit older hardware.
	pub dep:bool,
	/// # Fault-Recording Register Offset
	/// This field specifies the offset of the first fault recording register relative to the
	/// register base address of this remapping hardware unit.
	/// 
	/// If the register base address is X, and the value reported in this field is Y,
	/// the address for the first fault recording register is calculated as X+(16*Y).
	#[bits(10)] pub fro:usize,
	/// # Second-Stage Large Page Support
	/// This field indicates the large page sizes supported by hardware.
	/// 
	/// A value of 1 in any of these bits indicates the corresponding large page size is supported.
	/// The large-page sizes corresponding to various bit positions within this field are:
	/// - 0: 21-bit offset to page frame (2MB)
	/// - 1: 30-bit offset to page frame (1GB)
	/// - 2: Reserved
	/// - 3: Reserved
	/// 
	/// Hardware implementations supporting a specific large-page size must support all smaller
	/// large-page sizes. i.e., only valid values for this field are 0000b, 0001b, 0011b.
	#[bits(4)] pub sslps:u64,
	rsvd2:bool,
	/// # Page Selective Invalidation
	/// - `false`: Hardware supports only global and domain-selective invalidates for IOTLB.
	/// - `true`: Hardware supports page-selective, domain-selective, and global invalidates for IOTLB.
	/// 
	/// Hardware implementations reporting this field as Set are recommended to support
	/// a Maximum Address Mask Value (MAMV) of at least 9 (or 18 if supporting 1GB pages
	/// with second level translation).
	/// 
	/// This field is applicable only for IOTLB invalidation descriptor. Irrespective of
	/// value reported in this field, implementations supporting SMTS must support
	/// page/address selective PASID-based IOTLB invalidation descriptor.
	pub psi:bool,
	/// # Number of Fault-Recording Registers
	/// Number of fault recording registers is computed as N+1, where N is the value reported
	/// in this field. Implementations must support at least one fault recording register
	/// (NFR = 0) for each remapping hardware unit in the platform. The maximum number of
	/// fault recording registers per remapping hardware unit is 256.
	pub nfr:u8,
	/// # Maximum Address Mask Value
	/// The value in this field indicates the maximum supported value for the Address Mask
	/// (AM) field in the Invalidation Address register (IVA_REG), and IOTLB Invalidation
	/// Descriptor (iotlb_inv_dsc) used for invalidations of second-stage translation.
	/// 
	/// This field is valid when the PSI field in Capability register is reported as Set.
	/// 
	/// Independent of value reported in this field, implementations supporting SMTS must
	/// support address-selective PASID-based IOTLB invalidations (p_iotlb_inv_dsc) with
	/// any defined address mask.
	#[bits(6)] pub mamv:u64,
	/// # Write-Draining
	/// - `false`: Hardware does not support draining of write requests on IOTLB invalidation.
	/// - `true`: Hardware supports draining of write requests on IOTLB invalidation.
	/// 
	/// Hardware implementation with Major Version 2 or higher (VER_REG), always performs
	/// required drain without software explicitly requesting a drain in IOTLB invalidation.
	/// This field is deprecated and hardware will always report it as 1 to maintain
	/// backward compatibility with software.
	pub dwd:bool,
	/// # Read-Draining
	/// - `false`: Hardware does not support draining of read requests on IOTLB invalidation.
	/// - `true`: Hardware supports draining of read requests on IOTLB invalidation.
	/// 
	/// Hardware implementation with Major Version 2 or higher (VER_REG), always performs
	/// required drain without software explicitly requesting a drain in IOTLB invalidation.
	/// This field is deprecated and hardware will always report it as 1 to maintain
	/// backward compatibility with software.
	pub drd:bool,
	/// # First-Stage 1-GByte Page Support
	/// A value of `true` in this field indicates 1-GByte page size is supported for first-
	/// stage translation. \
	/// Hardware implementations reporting First-stage Translation Support (FSTS) as Clear
	/// also report this field as Clear.
	pub fs1gp:bool,
	#[bits(2)] rsvd3:u64,
	/// # Posted Interrupts Support
	/// - `false`: Hardware does not support Posting of Interrupts.
	/// - `true`: Hardware supports Posting of Interrupts.
	/// 
	/// Hardware implementations reporting Interrupt Remapping support (IR) field in
	/// Extended Capability Register as Clear also report this field as Clear.
	pub pi:bool,
	/// # First-Stage 5-level Paging Support
	/// - `false`: Hardware does not support 5-level paging for first-stage translation.
	/// - `true`: Hardware supports 5-level paging for first-stage translation.
	/// 
	/// Hardware implementations reporting First-stage Translation Support (FSTS) as Clear
	/// also report this field as Clear.
	pub fs5lp:bool,
	/// # Enhanced Command Support
	/// - `false`: Hardware does not support enhanced command interface.
	/// - `true`: Hardware supports enhanced command interface.
	pub ecmds:bool,
	/// # Enhanced Set Interrupt Remap Table Pointer Support
	/// - `false`: Hardware does not invalidate all Interrupt remapping hardware
	/// translation caches as part of SIRTP flow.
	/// - `true`: Hardware invalidates all Interrupt remapping hardware translation
	/// caches as part of SIRTP flow.
	pub esirtps:bool,
	/// # Enhanced Set Root Table Pointer Support
	/// - `false`: Hardware does not invalidate all DMA remapping hardware
	/// translation caches as part of SRTP flow.
	/// - `true`: Hardware invalidates all DMA remapping hardware translation
	/// caches as part of SRTP flow.
	pub esrtps:bool
}

impl IommuRegister for CapabilityRegister
{
	const MMIO_OFFSET:usize = 0x008;
	derive_mmio_rw!();
}

/// Register to report remapping hardware extended capabilities.
#[bitfield(u64)] pub struct ExtendedCapabilityRegister
{
	/// ## Page-Walk Coherency Support
	/// This field indicates if hardware access to the root, scalable-mode root,
	/// context, scalable-mode-context, scalable-mode PASID directory,
	/// scalable-mode PASID-table, and interrupt-remap tables, and legacy-mode
	/// second-stage paging structures are coherent (snooped) or not.
	/// 
	/// - 0:Indicates hardware accesses to remapping structures are non-coherent.
	/// - 1:Indicates hardware accesses to remapping structures are coherent.
	/// 
	/// Hardware access to invalidation queue, invalidation wait descriptor
	/// completion status address, and page-request queue are always snooped.
	/// 
	/// See Scalable-Mode Page-walk Coherency (SMPWC) field for hardware behavior
	/// on paging structures accessed through scalable-mode PASID-table entry.
	pub c:bool,
	/// ## Queued Invalidation Support
	/// - 0: Hardware does not support queued invalidations.
	/// - 1: Hardware supports queued invalidations.
	pub qi:bool,
	/// ## Device-TLB Support
	/// - 0: Hardware does not support Device-TLBs.
	/// - 1: Hardware supports Device-TLBs.
	/// 
	/// Hardware implementation reporting Queued Invalidation support
	/// (QI) field as Clear also report this field as Clear.
	pub dt:bool,
	/// ## Interrupt Remapping Support
	/// - 0: Hardware does not support interrupt remapping.
	/// - 1: Hardware supports interrupt remapping.
	/// 
	/// Hardware implementation reporting Queued Invalidation support
	/// (QI) field as Clear also report this field as Clear.
	pub ir:bool,
	/// ## Extended Interrupt Mode Support
	/// - 0: On Intel® 64 platforms, hardware supports only 8-bit APIC-IDs (xAPIC Mode).
	/// - 1: On Intel® 64 platforms, hardware supports 32-bit APIC-IDs (x2APIC mode).
	/// 
	/// Hardware implementation reporting Interrupt Remapping support
	/// (IR) field as Clear also report this field as Clear.
	pub eim:bool,
	/// Deprecated, must be 0 to maintain backward compatibility with older software.
	pub dep0:bool,
	/// ## Pass-through Translation Support
	/// - 0: Hardware does not support pass-through translation type
	/// in context-entries and scalable-mode-pasid-table-entries.
	/// - 1: Hardware supports pass-through translation type in
	/// context and scalable-mode-pasid-table-entries.
	pub pt:bool,
	/// ## Snoop Control Support
	/// - 0: Hardware does not support 1-setting of the SNP field in
	/// the second-stage page-table entries and the PGSNP field in
	/// the scalable-mode PASID-table entries.
	/// - 1: Hardware supports the 1-setting of the SNP field in the
	/// second-stage page-table entries and the PGSNP field in the
	/// scalable-mode PASID-table entries.
	/// 
	/// Implementations are recommended to support Snoop Control to
	/// support software usages that require Snoop Control for
	/// assignment of devices behind a remapping hardware unit.
	pub sc:bool,
	/// ## IOTLB Register Offset
	/// This field specifies the offset to the IOTLB registers relative to the
	/// register base address of this remapping hardware unit.
	/// 
	/// If the register base address is X, and the value reported in this
	/// field is Y, the address for the IOTLB registers is calculated as
	/// X+(16*Y).
	#[bits(10)] pub iro:usize,
	#[bits(2)] rsvd0:u64,
	/// ## Maximum Handle Mask Value
	/// The value in this field indicates the maximum supported value for
	/// the Interrupt Mask (IM) field in the Interrupt Entry Cache
	/// Invalidation Descriptor (iec_inv_dsc).
	/// 
	/// This field is unused and is reported as 0 if Interrupt Remapping
	/// support (IR) field is Clear.
	#[bits(4)] pub mhmv:u64,
	/// Deprecated, must be 0 to maintain backward compatibility with older software.
	pub dep1:bool,
	/// ## Memory Type Support
	/// - 0: Hardware does not support Memory Type in first-stage
	/// translation and Extended Memory type in second-stage
	/// translation.
	/// - 1: Hardware supports Memory Type in first-stage translation
	/// and Extended Memory type in second-stage translation.
	/// 
	/// Hardware implementations reporting Scalable Mode Translation
	/// Support (SMTS) field as Clear also report this field as Clear.
	/// 
	/// Remapping hardware units with, one or more devices that
	/// operate in processor coherency domain, under its scope must
	/// report this field as Set.
	pub mts:bool,
	/// ## Nested Translation Support
	/// - 0: Hardware does not support nested translations.
	/// - 1: Hardware supports nested translations.
	/// 
	/// Hardware implementations reporting Scalable Mode Translation
	/// Support (SMTS) field as Clear or First-stage Translation Support
	/// (FSTS) field as Clear or Second-stage Translation Support (SSTS)
	/// field as Clear also report this field as Clear.
	pub nest:bool,
	rsvd1:bool,
	/// Deprecated, must be 0 to maintain backward compatibility with older software.
	pub dep2:bool,
	/// ## Page Request Support
	/// - 0: Hardware does not support page requests.
	/// - 1: Hardware supports page requests.
	/// 
	/// Hardware implementation reporting Device-TLB support (DT) field
	/// as Clear or Scalable Mode Translation Support (SMTS) field as
	/// Clear also report this field as Clear.
	pub prs:bool,
	/// ## Execute Request Support
	/// - 0: Hardware does not support requests-with-PASID seeking execute permission.
	/// - 1: Hardware supports requests-with-PASID seeking execute permission.
	/// 
	/// Hardware implementations reporting Process Address Space ID
	/// support (PASID) field as Clear must report this field as Clear.
	pub ers:bool,
	/// ## Supervisor Request Support
	/// - 0: Hardware does not support requests (with or without
	/// PASID) seeking supervisor privilege.
	/// - 1: Hardware supports requests (with or without PASID)
	/// seeking supervisor privilege.
	/// 
	/// Hardware implementations reporting Scalable Mode Translation
	/// Support (SMTS) field as Clear also report this field as Clear.
	pub srs:bool,
	rsvd2:bool,
	/// ## No-Write Flag Support in Device-TLB Translation Requests
	/// - 0: Hardware ignores the ‘No Write’ (NW) flag in Device-TLB
	/// translation-requests, and behaves as if NW is always 0.
	/// - 1: Hardware supports the ‘No Write’ (NW) flag in Device-TLB
	/// translation-requests.
	/// 
	/// Hardware implementations reporting Device-TLB support (DT)
	/// field as Clear also report this field as Clear.
	pub nwfs:bool,
	/// ## Extended Accessed Bit Support in First-Stage Translation
	/// - 0: Hardware does not support the extended-accessed (EA)
	/// bit in first-stage paging-structure entries.
	/// - 1: Hardware supports the extended-accessed (EA) bit in
	/// first-stage paging-structure entries.
	/// 
	/// Hardware implementations reporting First Stage Translation
	/// Support (FSTS) or Scalable-Mode Page-walk Coherency Support
	/// (SWPWCS) as Clear also report this field as Clear.
	pub eafs:bool,
	/// ## PASID Size Support
	/// This field reports the PASID size supported by the remapping
	/// hardware for requests-with-PASID. A value of N in this field
	/// indicates hardware supports PASID field of N+1 bits (For
	/// example, value of 7 in this field, indicates 8-bit PASIDs are
	/// supported).
	/// Requests-with-PASID with PASID value beyond the limit specified
	/// by this field are treated as error by the remapping hardware.
	/// This field is unused and reported as 0 if Scalable Mode Translation
	/// Support (SMTS) field is Clear.
	#[bits(5)] pub pss:u64,
	/// ## Process Address Space ID Support
	/// - 0: Hardware does not support requests tagged with Process Address Space IDs.
	/// - 1: Hardware supports requests tagged with Process Address Space IDs.
	/// 
	/// Hardware implementations reporting Scalable Mode Translation
	/// Support (SMTS) field as Clear also report this field as Clear.
	pub pasid:bool,
	/// ## Device-TLB Invalidation Throttling Support
	/// - 0: Hardware does not support Device-TLB Invalidation Throttling.
	/// - 1: Hardware supports Device-TLB Invalidation Throttling.
	/// 
	/// Hardware implementations reporting Device-TLB support (DT) as
	/// Clear also report this field as Clear.
	pub dit:bool,
	/// ## Page-Request Drain Support
	/// - 0: Hardware does not support Page-request Drain (PD) flag in inv_wait_dsc.
	/// - 1: Hardware supports Page-request Drain (PD) flag in inv_wait_dsc.
	/// 
	/// Hardware implementations reporting Device-TLB support (DT) as
	/// Clear also report this field as Clear.
	pub pds:bool,
	/// ## Scalable Mode Translation Support
	/// - 0: Hardware does not support Scalable Mode DMA Remapping.
	/// - 1: Hardware supports Scalable Mode DMA Remapping through
	/// scalable-mode context-table and PASID-table structures.
	/// 
	/// Hardware implementation reporting Queued Invalidation (QI)
	/// field as Clear also report this field as Clear.
	/// 
	/// Hardware implementations reporting First-Stage Translation
	/// Support (FSTS), Second-stage Translation Support (SSTS) and
	/// Pass-through Support (PT) as Clear also report this field as Clear.
	pub smts:bool,
	/// ## Virtual Command Support
	/// - 0: Hardware does not support command submission to virtual-DMA Remapping hardware.
	/// - 1: Hardware does support command submission to virtual-DMA Remapping hardware.
	/// 
	/// Hardware implementations of this architecture report a value of 0
	/// in this field. Software implementations (emulation) of this
	/// architecture may report VCS=1.
	/// 
	/// Software managing remapping hardware should be written to
	/// handle both values of VCS.
	pub vcs:bool,
	/// ## Second-Stage Accessed/Dirty Bit Support
	/// - 0: Hardware does not support Accessed/Dirty bits in Second-
	/// Stage translation.
	/// - 1: Hardware supports Accessed/Dirty bits in Second-Stage
	/// translation.
	///
	/// Hardware implementations reporting Scalable-Mode Page-walk
	/// Coherency Support (SMPWCS) as Clear also report this field as
	/// Clear.
	pub ssads:bool,
	/// ## Second-Stage Translation Support
	/// - 0: Hardware does not support PASID Granular Translation
	/// Type of second-stage (PGTT=010b) in scalable-mode PASIDTable
	/// entry.
	/// - 1: Hardware supports PASID Granular Translation Type of
	/// second-stage (PGTT=010b) in scalable-mode PASID-Table
	/// entry.
	pub ssts:bool,
	/// ## First-Stage Translation Support
	/// - 0: Hardware does not support PASID Granular Translation
	/// Type of first-stage (PGTT=001b) in scalable-mode PASIDTable
	/// entry.
	/// - 1: Hardware supports PASID Granular Translation Type of
	/// first-stage (PGTT=001b) in scalable-mode PASID-Table entry.
	/// 
	/// Hardware implementations reporting Scalable Mode Translation
	/// Support (SMTS) as Clear also report this field as Clear.
	pub fsts:bool,
	/// ## Scalable Mode Page-Table Walk Coherency Support
	/// - 0: Hardware access to paging structures accessed through
	/// PASID-table entry are not snooped.
	/// - 1: Hardware access to paging structures accessed through
	/// PASID-table entry are snooped if PWSNP field in PASID-table
	/// entry is Set. Paging-structures accessed through PASID-table
	/// entry are not snooped if PWSNP field in PASID-table entry is
	/// Clear.
	/// 
	/// Hardware implementations reporting Scalable Mode Translation
	/// Support (SMTS) as Clear also report this field as Clear.
	pub smpwcs:bool,
	/// ## RID-PASID Support
	/// - 0: Hardware does not support the RID_PASID field in the
	/// scalable-mode context-entry. It uses the value of 0 for RID_PASID.
	/// - 1: Hardware supports the RID_PASID field in the scalable-mode context-entry.
	/// 
	/// Hardware implementations reporting Scalable Mode Translation Support
	/// (SMTS) as Clear also report this field as Clear.
	pub rps:bool,
	rsvd3:bool,
	/// ## Performance Monitoring Support
	/// - 0: Hardware does not support Performance Monitoring.
	/// - 1: Hardware supports Performance Monitoring.
	/// 
	/// Hardware implementations reporting Enhanced Command Support (ECMDS) as
	/// Clear also report this field as Clear.
	pub pms:bool,
	/// ## Abort DMA Mode Support
	/// - 0: Hardware does not support the ADMS Mode.
	/// - 1: Hardware supports the ADMS Mode.
	pub adms:bool,
	/// ## RID Privilege Support
	/// - 0: Hardware does not support the RID_PRIV field in the
	/// scalable-mode context-entry. It uses the value of 0 for RID_PRIV.
	/// - 1: Hardware supports the RID_PRIV field in the scalable-mode
	/// context-entry.
	/// 
	/// Hardware implementations reporting Supervisor Request Support
	/// (SRS) as Clear also report this field as Clear.
	pub rprivs:bool,
	#[bits(4)] rsvd4:u64,
	/// ## Stop Marker Support
	/// Stop Marker Message is defined by PCI Express specification and
	/// is used to indicate that function has transmitted all the pending
	/// Page Request Messages for a specific PASID.
	/// 
	/// - 0: Remapping hardware does not support Stop Marker
	/// Message. If remapping hardware is unable to write a Stop
	/// Marker Message into the Page Request Queue (due to the
	/// queue being full or a non-recoverable fault), behavior is
	/// undefined; otherwise it will write the Stop Marker Message
	/// into the Page Request Queue.
	/// - 1: Remapping hardware supports Stop Marker Message. If
	/// remapping hardware is unable to write a Stop Marker
	/// Message into the Page Request Queue (due to the queue
	/// being full or a non-recoverable fault), it will silently drop the
	/// Stop Marker Message; otherwise it will write the Stop Marker
	/// Message into the Page Request Queue.
	/// 
	/// For implementations reporting Stop Marker Support (SMS) as
	/// Clear, software is recommended to not enable Page Request
	/// Interface on devices that may generate a Stop Marker Message.
	/// 
	/// Hardware implementations reporting Page Request Support
	/// (PRS) as Clear also report this field as Clear.
	pub sms:bool,
	#[bits(5)] rsvd5:u64
}

impl IommuRegister for ExtendedCapabilityRegister
{
	const MMIO_OFFSET:usize = 0x010;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct GlobalCommandRegister
{
	#[bits(23)] rsvd0:u32,
	/// ## Compatibility Format Interrupts
	/// This field is valid only for Intel® 64 implementations supporting interrupt-remapping.
	/// Software writes to this field to enable or disable Compatibility Format interrupts on
	/// Intel® 64 platforms. The value in this field is effective only when interrupt-remapping
	/// is enabled and Extended Interrupt Mode (x2APIC mode) is not enabled.
	/// - 0: Block Compatibility format interrupts.
	/// - 1: Process Compatibility format interrupts as pass-through (bypass
	/// interrupt remapping).
	/// 
	/// Hardware reports the status of updating this field through the CFIS field
	/// in the Global Status register.
	/// 
	/// Refer to Section 5.1.2.1 for details on Compatibility Format interrupt
	/// requests.
	/// 
	/// The value returned on a read of this field is undefined.
	pub cfi:bool,
	/// ## Set Interrupt Remap Table Pointer
	/// This field is valid only for implementations supporting interrupt remapping.
	/// Software sets this field to set/update the interrupt remapping table pointer
	/// used by hardware. The interrupt remapping table pointer is specified through
	/// the Interrupt Remapping Table Address (IRTA_REG) register.
	/// 
	/// Hardware reports the status of the "Set Interrupt Remap Table Pointer"
	/// operation through the IRTPS field in the Global Status register.
	/// 
	/// The "Set Interrupt Remap Table Pointer" operation must be performed
	/// before enabling or re-enabling (after disabling) interrupt-remapping
	/// hardware through the IRE field.
	/// 
	/// For details on invalidation that software may have to perform after the
	/// 'Set Interrupt Remap Table Pointer' operation, refer to Section 6.7.
	/// 
	/// Clearing this bit has no effect. The value returned on a read of this field is undefined.
	pub sirtp:bool,
	/// ## Interrupt-Remapping Enable
	/// This field is valid only for implementations supporting interrupt remapping.
	/// - 0: Disable interrupt-remapping hardware
	/// - 1: Enable interrupt-remapping hardware
	/// 
	/// Hardware reports the status of the interrupt remapping enable operation
	/// through the IRES field in the Global Status register.
	/// 
	/// There may be active interrupt requests in the platform when software
	/// updates this field. Hardware must enable or disable interrupt-remapping
	/// logic only at deterministic transaction boundaries, so that any in-flight
	/// interrupts are either subject to remapping or not at all.
	/// 
	/// For implementations reporting the Enhanced Set Interrupt Remap Table Pointer
	/// Support (ESIRTPS) field as Set, hardware performs global invalidation on all
	/// Interrupt remapping caches as part of Interrupt Remapping Disable operation.
	/// 
	/// Hardware implementations must drain any in-flight interrupts requests
	/// queued in the Root-Complex before completing the interrupt-remapping
	/// enable command and reflecting the status of the command through the
	/// IRES field in the Global Status register.
	/// 
	/// The value returned on a read of this field is undefined.
	pub ire:bool,
	/// ## Queued Invalidation Enable
	/// This field is valid only for implementations supporting queued invalidations.
	/// 
	/// Software writes to this field to enable or disable queued invalidations.
	/// - 0: Disable queued invalidations.
	/// - 1: Enable use of queued invalidations.
	/// 
	/// Hardware reports the status of queued invalidation enable operation
	/// through QIES field in the Global Status register.
	/// 
	/// Refer to Section 6.5.2 for software requirements for enabling/disabling
	/// queued invalidations.
	/// 
	/// The value returned on a read of this field is undefined.
	pub qie:bool,
	/// ## Write Buffer Flush
	/// This bit is valid only for implementations requiring write buffer flushing.
	/// Software sets this field to request that hardware flush the Root-Complex
	/// internal write buffers. This is done to ensure any updates to the memoryresident
	/// remapping structures are not held in any internal write posting
	/// buffers.
	/// 
	/// Refer to Section 6.8 for details on write-buffer flushing requirements.
	/// 
	/// Hardware reports the status of the write buffer flushing operation
	/// through the WBFS field in the Global Status register.
	/// 
	/// Clearing this bit has no effect. The value returned on a read of this field is
	/// undefined.
	pub wbf:bool,
	#[bits(2)] rsvd1:u32,
	/// ## Set Root Table Pointer
	/// Software sets this field to set/update the root-table pointer (and
	/// translation table mode) used by hardware. The root-table pointer (and
	/// translation table mode) is specified through the Root Table Address
	/// (RTADDR_REG) register.
	/// 
	/// Hardware reports the status of the ‘Set Root Table Pointer’ operation
	/// through the RTPS field in the Global Status register.
	/// 
	/// The ‘Set Root Table Pointer’ operation must be performed before enabling
	/// or re-enabling (after disabling) DMA remapping through the TE field.
	/// 
	/// For details on invalidation that software may have to perform after the
	/// 'Set Root Table Pointer' operation refer to Section 6.6.
	/// 
	/// Clearing this bit has no effect. The value returned on a read of this field is
	/// undefined.
	pub srtp:bool,
	/// ## Translation Enable
	/// Software writes to this field to request hardware to enable/disable DMA remapping:
	/// - 0: Disable DMA remapping
	/// - 1: Enable DMA remapping
	/// 
	/// Hardware reports the status of the translation enable operation through
	/// the TES field in the Global Status register.
	/// 
	/// There may be active DMA requests in the platform when software updates this field.
	/// Hardware must enable or disable remapping logic only at deterministic transaction
	/// boundaries, so that any in-flight transaction is either subject to remapping or not at all.
	/// 
	/// Hardware implementations supporting DMA draining must drain any inflight DMA read/write
	/// requests queued within the Root-Complex before completing the translation enable command
	/// and reflecting the status of the command through the TES field in the Global Status register.
	/// 
	/// For implementations reporting Scalable Mode Translation Support (SMTS) field as Set, hardware
	/// performs global invalidation on all DMA remapping translation caches as part of Translation Disable operation.
	/// 
	/// The value returned on a read of this field is undefined.
	pub te:bool
}

impl IommuRegister for GlobalCommandRegister
{
	const MMIO_OFFSET:usize = 0x018;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct GlobalStatusRegister
{
	#[bits(23)] rsvd0:u32,
	/// ## Compatibility Format Interrupt Status
	/// This field is set by hardware to indicate the completion status of the
	/// 'Compatibility Format Interrupts' (CFI) command in the Global Command register.
	/// - 0: 'Compatibility Format Interrupts' command is completed.
	/// - 1: 'Compatibility Format Interrupts' command is still in progress.
	pub cfis:bool,
	/// ## Interrupt Remap Table Pointer Status
	/// This field is set by hardware to indicate the completion status of the
	/// 'Set Interrupt Remap Table Pointer' (SIRTP) command in the Global Command register.
	/// - 0: 'Set Interrupt Remap Table Pointer' command is completed.
	/// - 1: 'Set Interrupt Remap Table Pointer' command is still in progress.
	pub irtps:bool,
	/// ## Interrupt Remapping Enable Status
	/// This field is set by hardware to indicate the status of interrupt remapping enable/disable
	/// operation requested through the IRE field in the Global Command register.
	/// - 0: Interrupt remapping is disabled.
	/// - 1: Interrupt remapping is enabled.
	pub ires:bool,
	/// ## Queued Invalidation Enable Status
	/// This field is set by hardware to indicate the status of queued invalidation
	/// enable/disable operation requested through the QIE field in the Global Command register.
	/// - 0: Queued invalidations are disabled.
	/// - 1: Queued invalidations are enabled.
	pub qies:bool,
	/// ## Write Buffer Flush Status
	/// This field is set by hardware to indicate the completion status of the
	/// 'Write Buffer Flush' (WBF) command in the Global Command register.
	/// - 0: 'Write Buffer Flush' command is completed.
	/// - 1: 'Write Buffer Flush' command is still in progress.
	pub wbfs:bool,
	#[bits(2)] rsvd1:u32,
	/// ## Root Table Pointer Status
	/// This field is set by hardware to indicate the completion status of the
	/// 'Set Root Table Pointer' (SRTPS) command in the Global Command register.
	/// - 0: 'Set Root Table Pointer' command is completed.
	/// - 1: 'Set Root Table Pointer' command is still in progress.
	pub rtps:bool,
	/// ## Translation Enable Status
	/// This field is set by hardware to indicate the status of DMA remapping
	/// enable/disable operation requested through the TE field in the Global Command register.
	/// /// - 0: DMA remapping is disabled.
	/// - 1: DMA remapping is enabled.
	pub tes:bool
}

impl IommuRegister for GlobalStatusRegister
{
	const MMIO_OFFSET:usize = 0x01C;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub struct RootTableAddressRegister
{
	#[bits(10)] rsvd0:u64,
	/// ## Translation Type
	/// 
	/// This field specifies the translation mode used for DMA remapping.
	/// 
	/// - 00: legacy mode - uses root tables and context tables.
	/// - 01: scalable mode - uses scalable-mode root tables and scalable-mode context tables.
	/// - 10: reserved - in prior version of this specification, this encoding was
	/// used to enable extended mode which is no longer supported.
	/// - 11: abort-dma mode.
	/// 
	/// For implementations reporting Enhanced SRTP Support (ESRTPS) field as
	/// Clear in the Capability register, software must not modify this field while
	/// DMA remapping is active (TES=1 in Global Status register).
	/// 
	/// The value of this field takes effect only after software executes Set Root
	/// Table Pointer command. 
	#[bits(2)] pub tt:u64,
	/// ## Root Table Address
	/// 
	/// This field points to the base of the page-aligned, 4KB-sized root-table in
	/// system memory. Hardware may ignore and not implement bits 63:HAW,
	/// where HAW is the host address width.
	/// 
	/// The value of this field takes effect only after software executes Set Root
	/// Table Pointer command.
	#[bits(52)] pub rta:u64
}

impl IommuRegister for RootTableAddressRegister
{
	const MMIO_OFFSET:usize = 0x020;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub struct ContextCommandRegister
{
	/// ## Domain ID
	/// Indicates the id of the domain whose context-entries need to be selectively
	/// invalidated. This field must be programmed by software for both domainselective
	/// and device-selective invalidation requests.
	/// 
	/// The Capability register reports the domain-id width supported by hardware.
	/// Software must ensure that the value written to this field is within this limit.
	/// Hardware ignores (and may not implement) bits 15:N, where N is the
	/// supported domain-id width reported in the Capability register.
	pub did:u16,
	/// ## Source ID
	/// Indicates the source-id of the device whose corresponding context-entry
	/// needs to be selectively invalidated.This field along with the FM field must
	/// be programmed by software for device-selective invalidation requests.
	/// 
	/// The value returned on a read of this field is undefined.
	pub sid:u16,
	/// ## Function Mask
	/// Software may use the Function Mask to perform device-selective invalidations
	/// on behalf of devices supporting PCI Express Phantom Functions.
	/// 
	/// This field specifies which bits of the function number portion (least
	/// significant three bits) of the SID field to mask when performing deviceselective
	/// invalidations.The following encodings are defined for this field:
	/// 
	/// - 00: No bits in the SID field masked
	/// - 01: Mask bit 2 in the SID field
	/// - 10: Mask bits 2:1 in the SID field
	/// - 11: Mask bits 2:0 in the SID field
	/// 
	/// The context-entries corresponding to the source-ids specified through the
	/// SID and FM fields must have the domain-id specified in the DID field.
	/// 
	/// The value returned on a read of this field is undefined.
	#[bits(2)] pub fm:u64,
	#[bits(25)] rsvd:u64,
	/// ## Context Actual Invalidation Granularity
	/// Hardware reports the granularity at which an invalidation request was
	/// processed through the CAIG field at the time of reporting invalidation
	/// completion (by clearing the ICC field).
	/// 
	/// The following are the encodings for this field:
	/// 
	/// - 00: Error. This indicates hardware detected an incorrect invalidation
	/// request and ignored the request, e.g., register based invalidation when
	/// Translation Table Mode (TTM) in Root Table Address Register is not
	/// programmed to legacy mode (RTADDR_REG.TTM!=00b). \
	/// On hardware implementations with Major Version 6 or higher
	/// (VER_REG), all invalidation requests through this register are treated
	/// as incorrect invalidation requests. Software should use the Queued
	/// Invalidation interface to perform context-cache invalidations for such
	/// hardware implementations. Refer to Section 6.5 for more details.
	/// - 01: Global Invalidation performed. This could be in response to a
	/// global, domain-selective, or device-selective invalidation request.
	/// - 10: Domain-selective invalidation performed using the domain-id
	/// specified by software in the DID field. This could be in response to a
	/// domain-selective or device-selective invalidation request.
	/// - 11: Device-selective invalidation performed using the source-id and
	/// domain-id specified by software in the SID and FM fields. This can only
	/// be in response to a device-selective invalidation request.
	#[bits(2)] pub caig:u64,
	/// ## Context Invalidation Request Granularity
	/// Software provides the requested invalidation granularity through this field
	/// when setting the ICC field:
	/// 
	/// - 00: Reserved.
	/// - 01: Global Invalidation request.
	/// - 10: Domain-selective invalidation request. The target domain-id must
	/// be specified in the DID field.
	/// - 11: Device-selective invalidation request. The target source-id(s) must
	/// be specified through the SID and FM fields, and the domain-id [that
	/// was programmed in the context-entry for these device(s)] must be
	/// provided in the DID field.
	/// 
	/// Hardware implementations may process an invalidation request by
	/// performing invalidation at a coarser granularity than requested. Hardware
	/// indicates completion of the invalidation request by clearing the ICC field. At
	/// this time, hardware also indicates the granularity at which the actual
	/// invalidation was performed through the CAIG field.
	#[bits(2)] pub cirg:u64,
	/// ## Invalidate Context Cache
	/// Software requests invalidation of context-cache by setting this field.
	/// Software must also set the requested invalidation granularity by
	/// programming the CIRG field. Software must read back and check the ICC
	/// field is Clear to confirm the invalidation is complete. Software must not
	/// update this register when this field is Set.
	/// 
	/// Hardware clears the ICC field to indicate the invalidation request is
	/// complete.Hardware also indicates the granularity at which the invalidation
	/// operation was performed through the CAIG field.
	/// 
	/// Software must submit a context-cache invalidation request through this
	/// field only when there are no invalidation requests pending at this
	/// remapping hardware unit.
	/// 
	/// Since information from the context-cache may be used by hardware to tag
	/// IOTLB entries, software must perform domain-selective (or global)
	/// invalidation of IOTLB after the context-cache invalidation has completed.
	/// 
	/// Hardware implementations reporting a write-buffer flushing requirement
	/// (RWBF=1 in the Capability register) must implicitly perform a write buffer
	/// flush before invalidating the context-cache. Refer to Section 6.8 for write
	/// buffer flushing requirements.
	/// 
	/// When Translation Table Mode field in Root Table Address register is not
	/// setup as legacy mode (RTADDR_REG.TTM!=00b), hardware will ignore the
	/// value provided by software in this register, treat it as an incorrect
	/// invalidation request, and report a value of 00b in CAIG field.
	pub icc:bool
}

impl IommuRegister for ContextCommandRegister
{
	const MMIO_OFFSET:usize = 0x028;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub struct IotlbInvalidateRegister
{
	rsvd0:u32,
	/// ## Domain ID
	/// Indicates the ID of the domain whose IOTLB entries need to be selectively
	/// invalidated. This field must be programmed by software for domainselective
	/// and page-selective invalidation requests.
	/// The Capability register reports the domain-id width supported by hardware.
	/// Software must ensure that the value written to this field is within this limit.
	/// Hardware may ignore and not implement bits 47:(32+N), where N is the
	/// supported domain-id width reported in the Capability register.
	pub did:u16,
	/// ## Drain Writes
	/// This field is ignored by hardware if the DWD field is reported as Clear in the
	/// Capability register. When the DWD field is reported as Set in the Capability
	/// register, the following encodings are supported for this field:
	/// - 0: Hardware may complete the IOTLB invalidation without draining DMA write requests.
	/// - 1: Hardware must drain relevant translated DMA write requests.
	pub dw:bool,
	/// ## Drain Reads
	/// This field is ignored by hardware if the DRD field is reported as Clear in the
	/// Capability register. When the DRD field is reported as Set in the Capability
	/// register, the following encodings are supported for this field:
	/// - 0: Hardware may complete the IOTLB invalidation without draining DMA read requests.
	/// - 1: Hardware must drain DMA read requests.
	pub dr:bool,
	#[bits(7)] rsvd1:u64,
	/// ## IOTLB Actual Invalidation Granularity
	/// Hardware reports the granularity at which an invalidation request was
	/// processed through this field when reporting invalidation completion (by
	/// clearing the IVT field).
	/// 
	/// The following are the encodings for this field.
	/// - 00: Error. This indicates hardware detected an incorrect invalidation
	/// request and ignored the request, e.g., register based invalidation when
	/// Translation Table Mode (TTM) in Root Table Address Register is not
	/// programmed to legacy mode (RTADDR_REG.TTM!=00b), detected an
	/// unsupported address mask value in Invalidate Address register for
	/// page-selective invalidation requests. \
	/// On hardware implementations with Major Version 6 or higher (VER_REG),
	/// all invalidation requests through this register are treated as incorrect
	/// invalidation requests. Software should use the Queued Invalidation interface
	/// to perform IOTLB invalidations for such hardware implementations.
	/// - 01: Global Invalidation performed. This could be in response to a
	/// global, domain-selective, or page-selective invalidation request.
	/// - 10: Domain-selective invalidation performed using the domain-id
	/// specified by software in the DID field. This could be in response to a
	/// domain-selective or a page-selective invalidation request.
	/// - 11: Page-selective-within-domain invalidation performed using the
	/// address, mask and hint specified by software in the Invalidate Address
	/// register and domain-id specified in DID field. This can be in response to
	/// a page-selective-within-domain invalidation request.
	#[bits(2)] pub iaig:u64,
	rsvd2:bool,
	/// ## IOTLB Invalidation Request Granularity
	/// When requesting hardware to invalidate the IOTLB (by setting the IVT field), software writes the
	/// requested invalidation granularity through this field. The following are the encodings for the field.
	/// - 00: Reserved.
	/// - 01: Global invalidation request.
	/// - 10: Domain-selective invalidation request. The target domain-id must be specified in the DID field.
	/// - 11: Page-selective-within-domain invalidation request. The target address, mask, and invalidation hint
	/// must be specified in the Invalidate Address register, and the domain-id must be provided in the DID field.
	/// 
	/// Hardware implementations may process an invalidation request by performing invalidation at a coarser
	/// granularity than requested. Hardware indicates completion of the invalidation request by clearing the IVT
	/// field. At that time, the granularity at which actual invalidation was performed is reported through the IAIG field.
	#[bits(2)] pub iirg:u64,
	rsvd3:bool,
	/// ## Invalidate IOTLB
	/// Software requests IOTLB invalidation by setting this field. Software must
	/// also set the requested invalidation granularity by programming the IIRG field.
	/// 
	/// Hardware clears the IVT field to indicate the invalidation request is
	/// complete. Hardware also indicates the granularity at which the invalidation
	/// operation was performed through the IAIG field. Software must not submit
	/// another invalidation request through this register while the IVT field is Set,
	/// nor update the associated Invalidate Address register.
	/// 
	/// Software must not submit IOTLB invalidation requests when there is a
	/// context-cache invalidation request pending at this remapping hardware unit.
	/// 
	/// Hardware implementations reporting a write-buffer flushing requirement (RWBF=1
	/// in Capability register) must implicitly perform a write buffer flushing before
	/// invalidating the IOTLB. Refer to Section 6.8 for write buffer flushing requirements.
	/// 
	/// When Translation Table Mode field in Root Table Address registers is not
	/// setup as legacy mode (RTADDR_REG.TTM!=00b), hardware will ignore the
	/// value provided by software in this register, treat it as an incorrect
	/// invalidation request, and report a value of 00b in IAIG field.
	pub ivt:bool
}

#[bitfield(u64)] pub struct InvalidateAddressRegister
{
	/// ## Address Mask
	/// The value in this field specifies the number of low order bits of the ADDR field
	/// that must be masked for the invalidation operation. This field enables software
	/// to request invalidation of contiguous mappings for size-aligned regions.
	/// 
	/// When invalidating mappings for large-pages, software must specify the appropriate
	/// mask value. For example, when invalidating mapping for a 2MB page, software must
	/// specify an address mask value of at least 9. Hardware implementations report
	/// the maximum supported address mask value through the Capability register.
	/// 
	/// A value returned on a read of this field is undefined.
	#[bits(6)] pub am:u64,
	/// ## Invalidation Hint
	/// The field provides hints to hardware about preserving or flushing the nonleaf
	/// (context-entry) entries that may be cached in hardware:
	/// - 0: Software may have modified both leaf and non-leaf second-stage
	/// paging-structure entries corresponding to mappings specified in the
	/// ADDR and AM fields. On a page-selective-within-domain invalidation
	/// request, hardware must invalidate the cached entries associated with
	/// the mappings specified by DID, ADDR and AM fields, in both IOTLB
	/// and paging-structure caches. Refer to Section 6.5.1.2 for exact
	/// invalidation requirements when IH=0.
	/// - 1: Software has not modified any second-stage non-leaf paging
	/// entries associated with the mappings specified by the ADDR and AM
	/// fields. On a page-selective-within-domain invalidation request,
	/// hardware may preserve the cached second-stage mappings in pagingstructure-
	/// caches. Refer to Section 6.5.1.2 for exact invalidation
	/// requirements when IH=1.
	/// 
	/// A value returned on a read of this field is undefined.
	pub ih:bool,
	#[bits(5)] rsvd0:u64,
	/// ## Invalidate Address
	/// Software provides the second-stage-input-address that needs to be pageselectively invalidated.
	/// To make a page-selective-within-domain invalidation request to hardware, software must first
	/// write the appropriate fields in this register, and then issue the page-selective-within-domain
	/// invalidate command through the IOTLB_REG. Hardware ignores bits 63:N, where N is the maximum
	/// guest address width (MGAW) supported.
	/// 
	/// A value returned on a read of this field is undefined.
	#[bits(52)] pub addr:u64
}

#[bitfield(u32)] pub struct FaultStatusRegister
{
	/// ## Primary Fault Overflow
	/// Hardware sets this field to indicate overflow of the fault recording
	/// registers. When this field is Set, hardware does not record any new faults
	/// until software clears this field.
	pub pfo:bool,
	/// ## Primary Pending Fault
	/// This field indicates if there are one or more pending faults logged in the fault
	/// recording registers. Hardware computes this field as the logical OR of Fault (F)
	/// fields across all the fault recording registers of this remapping hardware unit.
	/// - 0: No pending faults in any of the fault recording registers
	/// - 1: One or more fault recording registers has pending faults. The FRI
	/// field is updated by hardware whenever the PPF field is Set by
	/// hardware. Also, depending on the programming of Fault Event Control
	/// register, a fault event is generated when hardware sets this field.
	pub ppf:bool,
	#[bits(2)] rsvd0:u32,
	/// ## Invalidation Queue Error
	/// Hardware detected an error associated with the invalidation queue. Refer to the
	/// description of the IQEI field in Section 11.4.9.9 for all possible conditions
	/// resulting in an Invalidation Queue Error. At this time, a fault event may be
	/// generated based on the programming of the Fault Event Control register.
	/// 
	/// Hardware implementations not supporting queued invalidations implement
	/// this bit as RsvdZ.
	pub iqe:bool,
	/// ## Invalidation Completion Error
	/// Hardware received an unexpected or invalid Device-TLB invalidation
	/// completion. This could be due to either an invalid ITag or invalid source-id
	/// in an invalidation completion response. At this time, a fault event may be
	/// generated based on the programming of the Fault Event Control register.
	/// 
	/// Hardware implementations not supporting Device-TLBs implement this bit as RsvdZ.
	pub ice:bool,
	/// ## Invalidation Time-out Error
	/// Hardware detected a Device-TLB invalidation completion time-out. At this
	/// time, a fault event may be generated based on the programming of the
	/// Fault Event Control register.
	/// 
	/// Hardware implementations not supporting Device-TLBs implement this bit
	/// as RsvdZ.
	pub ite:bool,
	dep:bool,
	/// ## Fault Recording Index
	/// This field is valid only when the PPF field is Set.
	/// 
	/// The FRI field indicates the index (from base) of the fault recording register
	/// to which the first pending fault was recorded when the PPF field was Set
	/// by hardware.
	/// 
	/// The value read from this field is undefined when the PPF field is Clear.
	pub fri:u8,
	rsvd1:u16,
}

impl IommuRegister for FaultStatusRegister
{
	const MMIO_OFFSET:usize = 0x034;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct FaultEventControlRegister
{
	#[bits(30)] rsvd:u32,
	/// ## Interrupt Pending
	/// Hardware sets the IP field whenever it detects an interrupt condition,
	/// which is defined as:
	/// - When primary fault logging is active, an interrupt condition occurs
	/// when hardware records a fault through one of the Fault Recording
	/// registers and sets the PPF field in the Fault Status register.
	/// - Hardware detected error associated with the Invalidation Queue,
	/// setting the IQE field in the Fault Status register.
	/// - Hardware detected invalid Device-TLB invalidation completion, setting
	/// the ICE field in the Fault Status register.
	/// - Hardware detected Device-TLB invalidation completion time-out,
	/// setting the ITE field in the Fault Status register.
	/// If any of the status fields in the Fault Status register was already Set at
	/// the time of setting any of these fields, it is not treated as a new interrupt
	/// condition.
	/// The IP field is kept Set by hardware while the interrupt message is held
	/// pending. The interrupt message could be held pending due to the interrupt
	/// mask (IM field) being Set or other transient hardware conditions.
	/// The IP field is cleared by hardware as soon as the interrupt message
	/// pending condition is serviced. This could be due to either:
	/// - Hardware issuing the interrupt message due to either a change in the
	/// transient hardware condition that caused the interrupt message to be
	/// held pending, or due to software clearing the IM field.
	/// - Software servicing all the pending interrupt status fields in the Fault
	/// Status register as follows.
	/// - When primary fault logging is active, software clearing the Fault
	/// (F) field in all the Fault Recording registers with faults, causing
	/// the PPF field in the Fault Status register to be evaluated as Clear.
	/// - Software clearing other status fields in the Fault Status register
	/// by writing back the value read from the respective fields.
	pub ip:bool,
	/// ## Interrupt Mask
	/// - 0: No masking of interrupts. When a interrupt condition is detected,
	/// hardware issues an interrupt message (using the Fault Event Data and
	/// Fault Event Address register values).
	/// - 1: This is the value on reset. Software may mask interrupt message
	/// generation by setting this field.Hardware is prohibited from sending
	/// the interrupt message when this field is Set.
	pub im:bool
}

impl IommuRegister for FaultEventControlRegister
{
	const MMIO_OFFSET:usize = 0x038;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct FaultEventDataRegister
{
	/// ## Interrupt Message Data
	/// Data value in the interrupt request. See Section
	/// "Remapping Hardware Event Interrupt Programming" in Intel VT-d manual for details.
	pub imd:u16,
	rsvd:u16
}

impl IommuRegister for FaultEventDataRegister
{
	const MMIO_OFFSET:usize = 0x03C;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct FaultEventAddressRegister
{
	#[bits(2)] rsvd:u32,
	/// ## Message Address
	/// When fault events are enabled, the contents of this register specify
	/// the DWORD-aligned address (bits 31:2) for the interrupt request.
	/// 
	/// See Section "Remapping Hardware Event Interrupt Programming"
	/// in Intel VT-d manual for details.
	#[bits(30)] pub ma:u32
}

impl IommuRegister for FaultEventAddressRegister
{
	const MMIO_OFFSET:usize = 0x040;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct FaultEventUpperAddressRegister
{
	/// ## Message Upper Address
	/// Hardware implementations supporting Extended Interrupt Mode are
	/// required to implement this register.
	/// 
	/// See Section "Remapping Hardware Event Interrupt Programming"
	/// in Intel VT-d manual for details.
	/// 
	/// Hardware implementations not supporting Extended Interrupt Mode
	/// may treat this field as RsvdZ.
	pub mua:u32
}

impl IommuRegister for FaultEventUpperAddressRegister
{
	const MMIO_OFFSET:usize = 0x044;
	derive_mmio_rw!();
}

#[bitfield(u128)] pub struct FaultRecordingRegister
{
	#[bits(12)] rsvd0:u128,
	#[bits(52)] pub fi:u64,
	pub sid:u16,
	#[bits(12)] rsvd1:u128,
	pub t2:bool,
	pub r#priv:bool,
	pub exe:bool,
	pub pp:bool,
	pub fr:u8,
	#[bits(20)] pub pv:u32,
	#[bits(2)] pub at:u32,
	pub t1:bool,
	pub f:bool
}

#[bitfield(u32)] pub struct ProtectedMemoryEnableRegister
{
	/// ## Protected Region Status
	/// This field indicates the status of protected memory region(s):
	/// - 0: Protected memory region(s) disabled.
	/// - 1: Protected memory region(s) enabled.
	pub prs:bool,
	#[bits(30)] rsvd:u32,
	/// ## Enable Protected Memory
	/// This field controls DMA accesses to the protected low-memory and protected high-memory regions.
	/// - 0: Protected memory regions are disabled.
	/// - 1: Protected memory regions are enabled. DMA requests
	/// accessing protected memory regions are handled as follows:
	/// 	* PCI and CXL.io links
	/// 		+ Hardware implementations with Major Version 2 or higher
	/// (VER_REG) block all DMA requests accessing protected memory regions
	/// whether or not DMA remapping is enabled.
	/// 		+ Hardware implementations starting with 6th Generation
	/// Intel® Core™ (codename: Skylake) and Intel® Xeon® Scalable Processors
	/// (codename: Skylake) block all DMA requests accessing protected memory
	/// regions whether or not DMA remapping is enabled.
	/// 		+ Some earlier hardware implementations (earlier than those
	/// referred above) do not block DMA requests that are subject to address
	/// remapping (i.e. requests other than passthrough and translated) from
	/// accessing the protected memory when DMA remapping is enabled. On such
	/// old hardware, software must ensure that there are no mappings programmed
	/// in the remapping structures that map to the protected memory region.
	/// 	* CXL.cache links
	/// 		+ PMR ranges do not protect accesses through CXL.cache. \
	/// Software enabling CXL.cache must protect desired memory
	/// region via DMA remapping page-tables and not depend on PMR.
	/// 
	/// Remapping hardware access to the remapping structures are not
	/// subject to protected memory region checks.DMA requests blocked
	/// due to protected memory region violation are not recorded or
	/// reported as remapping faults.
	/// 
	/// Hardware reports the status of the protected memory enable/disable
	/// operation through the PRS field in this register. Hardware
	/// implementations supporting DMA draining must drain any in-flight
	/// translated DMA requests queued within the Root-Complex before
	/// indicating the protected memory region as enabled through the PRS
	/// field.
	/// 
	/// After writing to this field software must wait for the operation to be
	/// completed and reflected in the PRS status field (bit 0) before
	/// changing the value of this field again.
	pub epm:bool
}

impl IommuRegister for ProtectedMemoryEnableRegister
{
	const MMIO_OFFSET:usize = 0x064;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct ProtectedLowMemoryBaseRegister
{
	/// ## Protected Low Memory Base Address
	/// This register specifies the base of protected low-memory region in system memory.
	pub plmb:u32
}

impl IommuRegister for ProtectedLowMemoryBaseRegister
{
	const MMIO_OFFSET:usize = 0x068;
	derive_mmio_rw!();
}

#[bitfield(u32)] pub struct ProtectedLowMemoryLimitRegister
{
	/// ## Protected Low Memory Base Address
	/// This register specifies the last host physical address of the DMA-protected
	/// low-memory region in system memory.
	pub plml:u32
}

impl IommuRegister for ProtectedLowMemoryLimitRegister
{
	const MMIO_OFFSET:usize = 0x06C;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub struct ProtectedHighMemoryBaseRegister
{
	/// ## Protected High Memory Base Address
	/// This register specifies the base of protected high-memory region in system memory. \
	/// Hardware may ignore and not implement bits 63:HAW, where HAW is the host address width.
	pub phmb:u64
}

impl IommuRegister for ProtectedHighMemoryBaseRegister
{
	const MMIO_OFFSET:usize = 0x070;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub struct ProtectedHighMemoryLimitRegister
{
	/// ## Protected High Memory Limit Address
	/// This register specifies the last host physical address of the DMA-protected
	/// high-memory region in system memory. \
	/// Hardware may ignore and not implement bits 63:HAW, where HAW is the host address width.
	pub phml:u64
}

impl IommuRegister for ProtectedHighMemoryLimitRegister
{
	const MMIO_OFFSET:usize = 0x078;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub struct InvalidationQueueHeadRegister
{
	#[bits(4)] rsvd0:u64,
	/// ## Queue Head Pointer
	/// Specifies the offset (128-bit or 256-bit aligned) to the invalidation
	/// queue for the command that will be processed next by hardware. \
	/// When Descriptor Width (DW) field in Invalidation Queue Address
	/// Register (IQA_REG) is Set (256-bit descriptors), hardware treats bit-
	/// 4 as reserved and will always write a value of 0 in the bit. \
	/// Hardware resets this field to 0 whenever the queued invalidation is
	/// disabled (QIES field Clear in the Global Status register).
	#[bits(15)] pub qh:u64,
	#[bits(45)] rsvd1:u64
}

impl IommuRegister for InvalidationQueueHeadRegister
{
	const MMIO_OFFSET:usize = 0x080;
	derive_mmio_rw!();
}

#[bitfield(u64)] pub struct InvalidationQueueTailRegister
{
	#[bits(4)] rsvd0:u64,
	/// ## Queue Tail Pointer
	/// Specifies the offset (128-bit or 256-bit aligned) to the invalidation
	/// queue for the command that will be written next by software. \
	/// When Descriptor Width (DW) field in Invalidation Queue Address Register
	/// (IQA_REG) is Set (256-bit descriptors), hardware treats bit-4 as reserved
	/// and a value of 1 in the bit will result in invalidation queue error.
	#[bits(15)] pub qt:u64,
	#[bits(45)] rsvd1:u64
}

#[bitfield(u64)] pub struct InvalidationQueueAddressRegister
{
	/// ## Queue Size
	/// This field specifies the size of the invalidation request queue. A value
	/// of X in this field indicates an invalidation request queue of (2^X) 4KB
	/// pages. The number of entries in the invalidation queue is 2^(X + 8) if
	/// Descriptor Width is 0 and 2^(X + 7)if Descriptor Width is 1.
	#[bits(3)] pub qs:u64,
	rsvd:u8,
	/// ## Descriptor Width
	/// This field specifies the size of the descriptors submitted into
	/// invalidation request queue.
	/// - 0: 128-bit descriptors
	/// - 1: 256-bit descriptors
	/// 
	/// When both scalable mode translation and abort-dma mode are
	/// not supported (ECAP_REG.SMTS=0 and ECAP_REG.ADMS=0),
	/// hardware treats a value of 1 in this field as an Invalidation
	/// Queue Error (see Invalidation Queue Error Info field in
	/// Section 11.4.9.9 for details).
	pub dw:bool,
	/// ## Invalidation Queue Address
	/// This field points to the base of 4KB aligned invalidation request
	/// queue. Hardware may ignore and not implement bits 63:HAW,
	/// where HAW is the host address width.
	/// 
	/// Reads of this field return the value that was last programmed to it.
	#[bits(52)] pub iqa:u64
}

#[bitfield(u32)] pub struct InvalidationCompletionStatusRegister
{
	/// ## Invalidation Wait Descriptor Complete
	/// Indicates completion of Invalidation Wait Descriptor with Interrupt
	/// Flag (IF) field Set. Hardware implementations not supporting queued
	/// invalidations implement this field as RsvdZ.
	pub iwc:bool,
	#[bits(31)] rsvd:u32
}

#[bitfield(u64)] pub struct InvalidationQueueErrorRecordRegister
{
	/// This field is valid only when the IQE field is Set in FSTS_REG. The
	/// value read from this field is undefined when the IQE field is Clear.
	/// This field provides additional details about the what caused IQE field
	/// to be Set.
	/// - 0: info not available. (This is an older version of DMA Remapping
	/// hardware which does not provide additional details about IQ
	/// errors.)
	/// - 1: hardware detected an invalid Tail Pointer.
	/// - 2: hardware attempt to fetch descriptor resulted in error.
	/// - 3: hardware detected an invalid descriptor type. (See Table 23 for
	/// details.)
	/// - 4: hardware detected a reserved field violation for a valid
	/// descriptor type
	/// - 5: hardware detected an invalid descriptor width programmed in
	/// the Invalidation Queue Address Register (IQA_REG)
	/// 	+ Descriptor width of 128-bit (IQA_REG.DW=0) when operating in
	/// scalable mode (RTADDR_REG.TTM=01b).
	/// 	+ Descriptor width of 256-bit (IQA_REG.DW=1) when both Scalable
	/// Mode Translation Support and Abort-DMA Mode Support are reported
	/// as Clear (ECAP_REG.SMTS=0 and ECAP_REG.ADMS=0).
	/// - 6: hardware detected that Queue Tail is not aligned to the
	/// descriptor width (i.e. IQA_REG.DW=1 and IQT.b[4]≠0).
	/// - 7: hardware detected an invalid value in the TTM field of the Root
	/// Table Address (RTADDR_REG) register.
	/// - 8-15: undefined.
	#[bits(4)] pub iqei:u64,
	#[bits(28)] rsvd:u64,
	/// ## Invalidation Time-out Error Source ID
	/// Requester-id associated with Invalidation Time-out Error.
	/// 
	/// A value of 0 in this field indicates that this is an older version of DMA
	/// remapping hardware which does not provide additional details about
	/// the Invalidation Time-out Error.
	/// 
	/// If the ITE field is Clear at the time of the Invalidation Time-out error detection,
	/// hardware copies the Requester-id associated with error into this field.
	/// 
	/// If multiple Invalidation Time-out errors are detected at the same
	/// time, hardware chooses one of them to be reported.
	/// 
	/// This field is valid only when the ITE field is Set in FSTS_REG. The
	/// value read from this field is undefined when the ITE field is Clear.
	pub itesid:u16,
	/// ## Invalidation Completion Error Source ID
	/// Requester-id associated with Device-TLB invalidation completion that
	/// causes an Invalidation Completion Error.
	/// 
	/// A value of 0 in this field indicates that this is an older version of DMA
	/// remapping hardware which does not provide additional details about
	/// the Invalidation Completion Error.
	/// 
	/// If the ICE field is Clear at the time of the Invalidation Completion
	/// Error detection, hardware copies the Requester-id in the Invalidation
	/// Completion Message that resulted in the error into this field.
	/// 
	/// This field is valid only when the ICE field is Set in FSTS_REG. The
	/// value read from this field is undefined when the ICE field is Clear.
	pub icesid:u16
}

#[bitfield(u64)] pub struct InterruptRemappingTableAddressRegister
{
	/// ## Size
	/// This field specifies the size of the interrupt remapping table. The
	/// number of entries in the interrupt remapping table is 2X+1, where X is
	/// the value programmed in this field.
	/// 
	/// The value of this field takes effect only after software executes Set
	/// Interrupt Remap Table Pointer command.
	#[bits(4)] pub s:u64,
	#[bits(7)] rsvd:u64,
	/// ## Extended Interrupt Mode Enable
	/// This field is used by hardware on Intel® 64 platforms as follows:
	/// - 0: xAPIC mode is active. Hardware interprets only 8-bits ([15:8])
	/// of Destination-ID field in the IRTEs. The high 16-bits and low 8-
	/// bits of the Destination-ID field are treated as reserved.
	/// - 1: x2APIC mode is active. Hardware interprets all 32-bits of
	/// Destination-ID field in the IRTEs.
	/// 
	/// This field is implemented as RsvdZ on implementations reporting
	/// Extended Interrupt Mode (EIM) field as Clear in Extended Capability
	/// register.
	/// 
	/// Software must not modify this field while Interrupt remapping is
	/// active (IRES=1 in Global Status register).
	/// 
	/// The value of this field takes effect only after software executes Set
	/// Interrupt Remap Table Pointer command.
	pub eime:bool,
	/// ## Interrupt Remapping Table Address
	/// This field points to the base of 4KB aligned interrupt remapping table.
	/// Hardware may ignore and not implement bits 63:HAW, where HAW is
	/// the host address width.
	/// 
	/// The value of this field takes effect only after software executes Set
	/// Interrupt Remap Table Pointer command.
	#[bits(52)] pub irta:u64
}

// TODO: Add remaining registers in Intel VT-d:
// - Page Request Queue Interface Registers
// - Memory Type Range Registers
// - Performance Monitoring Registers
// - Enhanced Command Interface Registers
// - Virtual Command Interface Registers