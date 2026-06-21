/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the ACPI Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{mem::offset_of, ptr::null_mut, slice, sync::atomic::{AtomicPtr, AtomicUsize, Ordering}};
use alloc::vec::Vec;

use spin::RwLock;
use log::*;

use nvcvm::status::Status;

use tables::{AcpiSystemDescriptorSignature, ExtendedSystemDescriptorTable, RootSystemDescriptionTable, SystemDescriptionHeader};
use crate::xpf_core::nvbdk::MappedDescriptor;

pub mod tables;

static RSDT_BASE_ADDRESS:AtomicPtr<SystemDescriptionHeader>=AtomicPtr::new(null_mut());
static RSDT_LENGTH:AtomicUsize=AtomicUsize::new(0);

static ACPI_MANAGER:RwLock<AcpiManager>=RwLock::new(AcpiManager{table:Vec::new()});

unsafe extern "C"
{
	fn noir_locate_acpi_rsdt(length:*mut usize)->*mut SystemDescriptionHeader;
}

struct AcpiManager
{
	table:Vec<MappedDescriptor<SystemDescriptionHeader>>
}

impl AcpiManager
{
	fn add_header(&mut self,phys:impl Into<u64>)
	{
		let phys:u64=phys.into();
		let tmp:MappedDescriptor<SystemDescriptionHeader>=MappedDescriptor::map(phys,size_of::<SystemDescriptionHeader>());
		if tmp.is_null()
		{
			error!("Failed to map ACPI Table at 0x{phys:016X}!");
			return;
		}
		let virt:MappedDescriptor<SystemDescriptionHeader>=MappedDescriptor::map(phys,tmp.as_ref().get_length() as usize);
		if virt.is_null()
		{
			error!("Failed to map ACPI Table at 0x{phys:016X}!");
			return;
		}
		debug!("Enumerated ACPI Table {}! Mapped to {:p} (Size={} bytes)...",virt.as_ref().signature,virt.virt,virt.as_ref().get_length());
		self.table.push(virt);
	}

	fn init_via_rsdt(&mut self,rsdt:*const RootSystemDescriptionTable)
	{
		let count=(unsafe{(*rsdt).header.get_length() as usize}-size_of::<SystemDescriptionHeader>())>>2;
		let rsdt_entries=unsafe{slice::from_raw_parts((*rsdt).entries.as_ptr(),count)};
		for phys in rsdt_entries
		{
			self.add_header(*phys);
		}
	}

	fn init_via_xsdt(&mut self,xsdt:*const ExtendedSystemDescriptorTable)
	{
		let count=(unsafe{(*xsdt).header.get_length() as usize}-size_of::<SystemDescriptionHeader>())>>3;
		debug!("XSDT Base Address: {xsdt:p}");
		let xsdt_ptr:*const u64=unsafe{xsdt.byte_add(offset_of!(ExtendedSystemDescriptorTable,entries)).cast()};
		for i in 0..count
		{
			let phys:u64=unsafe{xsdt_ptr.add(i).read_unaligned()};
			self.add_header(phys);
		}
	}

	fn init_empty(&mut self)
	{
		// OVMF in Bochs could not not properly initialize ACPI.
		debug!("No ACPI support!");
	}

	pub fn search(&self,signature:AcpiSystemDescriptorSignature,mut f:impl FnMut(*const SystemDescriptionHeader)->bool)
	{
		for virt in &self.table
		{
			let v=virt.as_ref();
			if v.signature.0==signature.0 && !f(&raw const *v)
			{
				break;
			}
		}
	}
}

pub fn search_acpi_table(signature:AcpiSystemDescriptorSignature,f:impl FnMut(*const SystemDescriptionHeader)->bool)
{
	let acpi_mgr=ACPI_MANAGER.read();
	acpi_mgr.search(signature,f);
}

#[unsafe(no_mangle)] extern "C" fn nvc_acpi_initialize()->Status
{
	let mut rsdt_len:usize=0;
	RSDT_BASE_ADDRESS.store(unsafe{noir_locate_acpi_rsdt(&raw mut rsdt_len)},Ordering::Relaxed);
	RSDT_LENGTH.store(rsdt_len,Ordering::Relaxed);
	let ptr_head=RSDT_BASE_ADDRESS.load(Ordering::Relaxed);
	let mut acpi_mgr=ACPI_MANAGER.write();
	unsafe
	{
		match (*ptr_head).signature
		{
			AcpiSystemDescriptorSignature::ROOT_SYSTEM_DESCRIPTION_TABLE=>acpi_mgr.init_via_rsdt(ptr_head.cast()),
			AcpiSystemDescriptorSignature::EXTENDED_SYSTEM_DESCRIPTION_TABLE=>acpi_mgr.init_via_xsdt(ptr_head.cast()),
			_=>acpi_mgr.init_empty()
		};
	}
	Status::SUCCESS
}

#[unsafe(no_mangle)] extern "C" fn nvc_acpi_finalize()
{
	let mut acpi_mgr=ACPI_MANAGER.write();
	acpi_mgr.table.clear();
}

#[unsafe(no_mangle)] extern "C" fn nvc_acpi_get_rsdt_ptr()->*mut SystemDescriptionHeader
{
	RSDT_BASE_ADDRESS.load(Ordering::Relaxed)
}