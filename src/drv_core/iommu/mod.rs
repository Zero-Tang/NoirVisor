/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines IOMMU operations for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::{boxed::Box, vec::Vec};
use log::*;

use crate::xpf_core::{allocator::PAGE_ALLOC_MANAGER, ci::CI_MANAGER, ioflt::IoRegionOps, nvbdk::{PAGE_1GB_SIZE, SYSTEM_PHYSICAL_MEMORY_RANGES, page_1gb_base, page_mult}};
use super::acpi::{search_acpi_table, tables::*};

pub mod intel;
pub mod amd;
pub mod virtio;

/// The `IommuOps` trait must be implemented on all IOMMU drivers.
pub trait IommuOps
{
	/// The `subvert` method will activate IOMMU and protect memory.
	fn subvert(&mut self);
	/// The `restore` method will deactivate IOMMU.
	fn restore(&mut self);
	/// Initialize the IOMMU with identity-mapping.
	/// ## Safety
	/// The `self` is not initialized. Must use `ptr::write` to circumvent
	/// any RAII actions for members that implement `Drop` trait. \
	/// Do not call this method on initialized IOMMU Driver. It may bypass
	/// `Drop` trait and thereby cause resource-leakage.
	unsafe fn init(&mut self);
	/// Map a normal page with specified permission in global page-table.
	fn set_pte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool);
	/// Map a large page with specified permission in global page-table.
	fn set_pde(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool);
	/// Map a huge page with specified permission in global page-table.
	fn set_pdpte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool);
	/// List all IOMMU BAR pages in order to filter I/O to them.
	fn get_bar_pages(&self)->Vec<Box<dyn IoRegionOps>>;

	// Setup Identity Mapping.
	fn setup_mapping(&mut self)
	{
		for range in &*SYSTEM_PHYSICAL_MEMORY_RANGES
		{
			let base=page_1gb_base(range.start);
			let end=range.start+range.length;
			for pa in (base..end).step_by(PAGE_1GB_SIZE)
			{
				self.set_pdpte(pa,pa,true,true,true);
			}
		}
	}

	/// Protect pages in Code-Integrity.
	fn protect_ci(&mut self)
	{
		let ci=CI_MANAGER.read();
		for p in ci.into_iter()
		{
			let phys=page_mult(p.pfn());
			self.set_pte(phys,phys,false,false,false);
		}
	}

	/// Protected pages allocated by NoirVisor
	fn protect_allocated_pages(&mut self)
	{
		for p in PAGE_ALLOC_MANAGER.lock().iter()
		{
			self.set_pde(p,p,false,false,false);
		}
	}
}

pub fn create_iommu()->Option<Box<dyn IommuOps>>
{
	// Enumerate ACPI tables to decide which IOMMU driver to invoke.
	let mut dmar_list:Vec<*const DmaRemappingReportingStructure>=Vec::new();
	let mut ivrs_list:Vec<*const IoVirtualizationReportingStructure>=Vec::new();
	search_acpi_table(AcpiSystemDescriptorSignature::DMA_REMAPPING_TABLE,|x| {dmar_list.push(x.cast()); true});
	search_acpi_table(AcpiSystemDescriptorSignature::IO_VIRTUALIZATION_REPORTING_STRUCTURE,|x| {ivrs_list.push(x.cast()); true});
	if !(dmar_list.is_empty() || ivrs_list.is_empty())
	{
		warn!("NoirVisor does not support using both Intel VT-d and AMD-Vi!");
		None
	}
	else if !dmar_list.is_empty()
	{
		info!("Intel VT-d is supported.");
		intel::create_iommu(&dmar_list)
	}
	else if !ivrs_list.is_empty()
	{
		warn!("AMD-Vi is not supported yet.");
		None
	}
	else
	{
		info!("No IOMMU is present in this machine.");
		None
	}
}