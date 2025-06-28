/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
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
use tables::{AcpiSystemDescriptorSignature, ExtendedSystemDescriptorTable, RootSystemDescriptionTable, SystemDescriptionHeader};
use crate::xpf_core::{nvbdk::{noir_map_physical_memory, noir_unmap_physical_memory}, nvstatus::*};
use crate::{print,println,dbg_print};

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
	table:Vec<AtomicPtr<SystemDescriptionHeader>>
}

impl AcpiManager
{
	fn add_header(&mut self,phys:impl Into<u64>)
	{
		let phys:u64=phys.into();
		let tmp:*mut SystemDescriptionHeader=unsafe{noir_map_physical_memory(phys,size_of::<SystemDescriptionHeader>()).cast()};
		if tmp.is_null()
		{
			panic!("Failed to map ACPI Table at 0x{phys:016X}!");
		}
		let virt:*mut SystemDescriptionHeader=unsafe{noir_map_physical_memory(phys,(*tmp).length as usize).cast()};
		unsafe{noir_unmap_physical_memory(tmp.cast(),size_of::<SystemDescriptionHeader>())};
		if virt.is_null()
		{
			panic!("Failed to map ACPI Table at 0x{phys:08X}!");
		}
		unsafe{println!("Enumerated ACPI Table {}! Mapped to {virt:p} (Size={} bytes)...",(*virt).signature,(*virt).length)};
		self.table.push(AtomicPtr::new(virt));
	}

	fn init_via_rsdt(&mut self,rsdt:*const RootSystemDescriptionTable)
	{
		let count=(unsafe{(*rsdt).header.length as usize}-size_of::<SystemDescriptionHeader>())>>2;
		let rsdt_entries=unsafe{slice::from_raw_parts((*rsdt).entries.as_ptr(),count)};
		for phys in rsdt_entries
		{
			self.add_header(*phys);
		}
	}

	fn init_via_xsdt(&mut self,xsdt:*const ExtendedSystemDescriptorTable)
	{
		let count=(unsafe{(*xsdt).header.length as usize}-size_of::<SystemDescriptionHeader>())>>3;
		println!("XSDT Base Address: {xsdt:p}");
		let xsdt_ptr:*const u64=unsafe{xsdt.byte_add(offset_of!(ExtendedSystemDescriptorTable,entries)).cast()};
		for i in 0..count
		{
			let phys:u64=unsafe{xsdt_ptr.add(i).read_unaligned()};
			self.add_header(phys);
		}
	}

	pub fn search(&self,signature:AcpiSystemDescriptorSignature,mut f:impl FnMut(*mut SystemDescriptionHeader)->bool)
	{
		for virt in &self.table
		{
			let v=virt.load(Ordering::Relaxed);
			if unsafe{(*v).signature.0}==signature.0 && !f(v)
			{
				break;
			}
		}
	}
}

pub fn search_acpi_table(signature:AcpiSystemDescriptorSignature,f:impl FnMut(*mut SystemDescriptionHeader)->bool)
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
			_=>panic!("Unknown Signature for Root System Description Table is detected!")
		};
	}
	NOIR_SUCCESS
}

#[unsafe(no_mangle)] extern "C" fn nvc_acpi_finalize()
{
	let acpi_mgr=ACPI_MANAGER.read();
	for virt in &acpi_mgr.table
	{
		let virt=virt.load(Ordering::Relaxed);
		unsafe
		{
			noir_unmap_physical_memory(virt.cast(),(*virt).length as usize);
		}
	}
}