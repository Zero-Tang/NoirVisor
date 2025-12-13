/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines IOMMU operations for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::vec::Vec;

pub mod intel;
pub mod amd;

/// The `IommuOps` trait must be implemented on all IOMMU drivers.
pub trait IommuOps
{
	/// The `subvert` method will activate IOMMU and protect memory.
	fn subvert(&mut self);
	/// The `restore` method will deactivate IOMMU.
	fn restore(&mut self);
	/// Initialize the IOMMU with identity-mapping.
	fn init(&mut self);
	/// Map a normal page with specified permission in global page-table.
	fn set_pte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool);
	/// Map a large page with specified permission in global page-table.
	fn set_pde(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool);
	/// Map a huge page with specified permission in global page-table.
	fn set_pdpte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool);
	/// List all IOMMU BAR pages in order to filter I/O to them.
	fn get_bar_pages(&self)->Vec<u64>;
}
