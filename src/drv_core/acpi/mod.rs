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
use tables::{AcpiSystemDescriptorSignature, ExtendedSystemDescriptorTable, RootSystemDescriptionTable, SystemDescriptionHeader};
use crate::xpf_core::{nvbdk::{noir_map_physical_memory, noir_unmap_physical_memory}, nvstatus::*};
use crate::{print,println,dbg_print};

pub mod tables;

static RSDT_BASE_ADDRESS:AtomicPtr<SystemDescriptionHeader>=AtomicPtr::new(null_mut());
static RSDT_LENGTH:AtomicUsize=AtomicUsize::new(0);

static mut ACPI_MANAGER:AcpiManager=AcpiManager{table:Vec::new()};
static ACPI_MANAGER_PTR:AtomicPtr<AcpiManager>=AtomicPtr::new(&raw mut ACPI_MANAGER);

unsafe extern "C"
{
	fn noir_locate_acpi_rsdt(length:*mut usize)->*mut SystemDescriptionHeader;
}

struct AcpiManager
{
	table:Vec<*mut SystemDescriptionHeader>
}

impl AcpiManager
{
	fn init_via_rsdt(rsdt:*const RootSystemDescriptionTable)->Self
	{
		let count=(unsafe{(*rsdt).header.length as usize}-size_of::<SystemDescriptionHeader>())>>2;
		let mut s:Self=Self{table:Vec::with_capacity(count)};
		let rsdt_entries=unsafe{slice::from_raw_parts((*rsdt).entries.as_ptr(),count)};
		unsafe
		{
			for phys in rsdt_entries
			{
				let tmp:*mut SystemDescriptionHeader=noir_map_physical_memory(*phys as u64,size_of::<SystemDescriptionHeader>()).cast();
				if tmp.is_null()
				{
					panic!("Failed to map ACPI Table at 0x{phys:08X}!");
				}
				let virt:*mut SystemDescriptionHeader=noir_map_physical_memory(*phys as u64,(*tmp).length as usize).cast();
				noir_unmap_physical_memory(tmp.cast(),size_of::<SystemDescriptionHeader>());
				if virt.is_null()
				{
					panic!("Failed to map ACPI Table at 0x{phys:08X}!");
				}
				println!("Enumerated ACPI Table {}! Mapped to {virt:p} (Size={} bytes)...",(*virt).signature,(*virt).length);
				s.table.push(virt);
			}
		}
		s
	}

	fn init_via_xsdt(xsdt:*const ExtendedSystemDescriptorTable)->Self
	{
		let count=(unsafe{(*xsdt).header.length as usize}-size_of::<SystemDescriptionHeader>())>>3;
		let mut s:Self=Self{table:Vec::with_capacity(count)};
		println!("XSDT Base Address: {xsdt:p}");
		unsafe
		{
			let xsdt_ptr:*const u64=xsdt.byte_add(offset_of!(ExtendedSystemDescriptorTable,entries)).cast();
			for i in 0..count
			{
				let phys:u64=xsdt_ptr.add(i).read_unaligned();
				let tmp:*mut SystemDescriptionHeader=noir_map_physical_memory(phys,size_of::<SystemDescriptionHeader>()).cast();
				if tmp.is_null()
				{
					panic!("Failed to map ACPI Table at 0x{phys:016X}!");
				}
				let virt:*mut SystemDescriptionHeader=noir_map_physical_memory(phys,(*tmp).length as usize).cast();
				noir_unmap_physical_memory(tmp.cast(),size_of::<SystemDescriptionHeader>());
				if virt.is_null()
				{
					panic!("Failed to map ACPI Table at 0x{phys:016X}!");
				}
				s.table.push(virt);
			}
		}
		s
	}

	pub fn search(&self,signature:AcpiSystemDescriptorSignature,mut f:impl FnMut(*mut SystemDescriptionHeader)->bool)
	{
		for &virt in &self.table
		{
			if unsafe{(*virt).signature.0}==signature.0 && !f(virt)
			{
				break;
			}
		}
	}
}

pub fn search_acpi_table(signature:AcpiSystemDescriptorSignature,f:impl FnMut(*mut SystemDescriptionHeader)->bool)
{
	let acpi_mgr:&'static AcpiManager=unsafe{&*ACPI_MANAGER_PTR.load(Ordering::Relaxed)};
	acpi_mgr.search(signature,f);
}

#[unsafe(no_mangle)] extern "C" fn nvc_acpi_initialize()->Status
{
	let mut rsdt_len:usize=0;
	RSDT_BASE_ADDRESS.store(unsafe{noir_locate_acpi_rsdt(&raw mut rsdt_len)},Ordering::Relaxed);
	RSDT_LENGTH.store(rsdt_len,Ordering::Relaxed);
	let ptr_head=RSDT_BASE_ADDRESS.load(Ordering::Relaxed);
	unsafe
	{
		ACPI_MANAGER=match (*ptr_head).signature
		{
			AcpiSystemDescriptorSignature::ROOT_SYSTEM_DESCRIPTION_TABLE=>AcpiManager::init_via_rsdt(ptr_head.cast()),
			AcpiSystemDescriptorSignature::EXTENDED_SYSTEM_DESCRIPTION_TABLE=>AcpiManager::init_via_xsdt(ptr_head.cast()),
			_=>panic!("Unknown Signature for Root System Description Table is detected!")
		};
	}
	NOIR_SUCCESS
}

#[unsafe(no_mangle)] extern "C" fn nvc_acpi_finalize()
{
	let acpi_mgr:&'static AcpiManager=unsafe{&*ACPI_MANAGER_PTR.load(Ordering::Relaxed)};
	for virt in &acpi_mgr.table
	{
		unsafe
		{
			noir_unmap_physical_memory(virt.cast(),(**virt).length as usize);
		}
	}
}