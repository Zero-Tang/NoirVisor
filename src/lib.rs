/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the entry point of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![no_std]
// We will use the unstable allocator_api feature for two things:
// 1. alternate allocator (i.e. the `Allocator` trait)
// 2. try-allocate (e.g.: `Box::try_new`, `Vec::try_reserve`)
#![feature(allocator_api)]

extern crate alloc;

pub mod drv_core;
pub mod xpf_core;
pub mod vt_core;
pub mod svm_core;
pub mod cvm_core;
pub mod mshv_core;
pub mod disasm;

use core::{slice, str};
use alloc::boxed::Box;

use log::*;

use nvcvm::status::Status;

use xpf_core::{allocator::set_alloc_checker, x86::cpuid::*, nvbdk::PAGE_SIZE};
pub use xpf_core::debug::*;
use vt_core::VtHypervisor;
use svm_core::SvmHypervisor;

// Limit stack size to 64KiB. Should be enough for most circumstances.
// FIXME: Implement runtime stack overflow detector.
pub const HYPERVISOR_STACK_SIZE:usize=PAGE_SIZE*16;

pub enum ProcessorManufacturer
{
	Intel,
	AMD,
	VIA,
	ZhaoXin,
	Hygon,
	Centaur,
	Cyrix,
	Transmeta,
	NexGen,
	SiS,
	NationalSemiconductor,
	Rise,
	UMC,
	Vortex,
	Unknown
}

#[unsafe(no_mangle)] unsafe extern "C" fn noir_get_vendor_string(vstr:*mut u8)
{
	let vendor_id=MaxStandardLeafAndVendorString::cpuid();
	let s=unsafe{slice::from_raw_parts_mut(vstr,12)};
	s.copy_from_slice(vendor_id.vendor_name().as_bytes());
}

#[unsafe(no_mangle)] unsafe extern "C" fn noir_get_processor_name(pstr:*mut u8)
{
	let brand_str=ProcessorBrandString::cpuid();
	let s=unsafe{slice::from_raw_parts_mut(pstr,48)};
	s.copy_from_slice(&brand_str.buffer);
}

impl ProcessorManufacturer
{
	fn query(vendor_string:&mut [u8;12])->Self
	{
		let vendor_id=MaxStandardLeafAndVendorString::cpuid();
		// Let's hope Rust's string match has O(logn) or better performance...
		// Otherwise, we will setup a pair of sorted lists and do binary search.
		// Note: Zhaoxin CPUs might use three different CPUID vendor names.
		// It can be one of Centaur, VIA and Zhaoxin.
		// Note: Montage Jintide CPUs will use Intel's vendor name.
		let s:&str=vendor_id.vendor_name();
		vendor_string.copy_from_slice(s.as_bytes());
		match s
		{
			"GenuineIntel"=>Self::Intel,
			"AuthenticAMD"=>Self::AMD,
			"AMDisbetter!"=>Self::AMD,
			"VIA VIA VIA "=>Self::VIA,
			"  Shanghai  "=>Self::ZhaoXin,
			"HygonGenuine"=>Self::Hygon,
			"CentaurHauls"=>Self::Centaur,
			"CyrixInstead"=>Self::Cyrix,
			"GenuineTMx86"=>Self::Transmeta,
			"NexGenDriven"=>Self::NexGen,
			"SiS SiS SiS "=>Self::SiS,
			"Geode by NSC"=>Self::NationalSemiconductor,
			"RiseRiseRise"=>Self::Rise,
			"UMC UMC UMC "=>Self::UMC,
			"Vortex86 SoC"=>Self::Vortex,
			_=>Self::Unknown
		}
	}
}

pub trait HypervisorCapabilities
{
	fn check_support()->u32;
	fn check_enabled()->bool;
}

pub trait HypervisorEssentials
{
	fn subvert_system(&mut self)->Status;
	fn restore_system(&mut self)->Status;
}

pub trait VirtualCpu
{
	fn init(self);
}

static mut HVM:Option<Box<dyn HypervisorEssentials>>=None;

#[unsafe(no_mangle)] extern "C" fn noir_get_virtualization_supportability()->u32
{
	use ProcessorManufacturer::*;
	let mut vstr_raw:[u8;12]=[0;12];
	let cpu_manuf=ProcessorManufacturer::query(&mut vstr_raw);
	match cpu_manuf
	{
		Intel|VIA|ZhaoXin|Centaur=>VtHypervisor::check_support(),
		AMD|Hygon=>SvmHypervisor::check_support(),
		_=>0
	}
}

#[unsafe(no_mangle)] extern "C" fn noir_is_virtualization_enabled()->bool
{
	use ProcessorManufacturer::*;
	let mut vstr_raw:[u8;12]=[0;12];
	let cpu_manuf=ProcessorManufacturer::query(&mut vstr_raw);
	match cpu_manuf
	{
		Intel|VIA|ZhaoXin|Centaur=>VtHypervisor::check_enabled(),
		AMD|Hygon=>SvmHypervisor::check_enabled(),
		_=>false
	}
}

#[unsafe(no_mangle)] extern "C" fn nvc_teardown_hypervisor()
{
	unsafe
	{
		let hv=&raw mut HVM;
		if let Some(hypervisor)=&mut *hv
		{
			hypervisor.restore_system();
		}
		// Setting it to None will call the drop method.
		HVM=None;
	}
}

#[unsafe(no_mangle)] extern "C" fn nvc_build_hypervisor()->Status
{
	use ProcessorManufacturer::*;
	// Subvert the system.
	info!("Subverting the system...");
	let mut vstr_raw:[u8;12]=[0;12];
	let hv:Option<Box<dyn HypervisorEssentials>>=match ProcessorManufacturer::query(&mut vstr_raw)
	{
		Intel|VIA|ZhaoXin|Centaur=>
		{
			// Use Intel VT-x.
			Some(Box::<VtHypervisor>::new(VtHypervisor::default()))
		}
		AMD|Hygon=>
		{
			// Use AMD-V.
			Some(Box::<SvmHypervisor>::new(SvmHypervisor::default()))
		}
		_=>
		{
			// Either this processor does not support virtualization at all,
			// or we don't know what kind of virtualization this processor supports.
			if let Ok(s)=core::str::from_utf8(&vstr_raw)
			{
				panic!("The processor vendor (\"{}\") is unknown!",s);
			}
			None
		}
	};
	if let Some(mut hypervisor)=hv
	{
		// Subvert the system.
		let st=hypervisor.subvert_system();
		set_alloc_checker(true);
		unsafe 
		{
			HVM=Some(hypervisor);
		}
		st
	}
	else
	{
		Status::NOT_IMPLEMENTED
	}
}

// Panic handler is required for no_std crates. But it is NOT FOR tests!
// Put it under non-test conditional-compilation, or otherwise
// the rust-analyzer of VSCode will report duplicate panic_impl.

#[cfg(not(test))]
#[allow(dead_code)]
mod panicking
{
	use crate::{print,println,dbg_print};
	use core::panic::PanicInfo;

	#[panic_handler] fn panic(panic: &PanicInfo)->!
	{
		println!("\x1b[91m[PANIC] NoirVisor {} \x1b[39m",panic);
		loop{}
	}
}