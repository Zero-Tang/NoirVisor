/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the entry point of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![no_std]

extern crate alloc;

pub mod drv_core;
pub mod xpf_core;
pub mod vt_core;
pub mod svm_core;
pub mod mshv_core;

use alloc::boxed::Box;
use core::str;

use xpf_core::{asm::cpuid::cpuid2, nvstatus::*};
pub use xpf_core::debug::*;
use svm_core::SvmHypervisor;

pub enum VirtualCpuOption
{
	Vt,
	Svm,
	None
}

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

impl ProcessorManufacturer
{
	fn query(vendor_string:&mut [u8;12])->Self
	{
		let (_,b,c,d)=cpuid2(0,0);
		let mut str_raw:[u8;12]=[0;12];
		str_raw[..4].copy_from_slice(&b.to_le_bytes());
		str_raw[4..8].copy_from_slice(&d.to_le_bytes());
		str_raw[8..].copy_from_slice(&c.to_le_bytes());
		*vendor_string=str_raw;
		let r=str::from_utf8(&str_raw);
		match r
		{
			Ok(s)=>
			{
				// Let's hope Rust's string match has O(logn) or better performance...
				// Otherwise, we will setup a pair of sorted lists and do binary search.
				match s.trim_matches('\0')
				{
					"GenuineIntel"=>Self::Intel,
					"AuthenticAMD"=>Self::AMD,
					"AMDisbetter!"=>Self::AMD,
					"VIA VIA VIA "=>Self::VIA,
					" Shanghai "=>Self::ZhaoXin,
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
			Err(e)=>
			{
				println!("Encountered UTF-8 Exception! Reason: {}",e);
				Self::Unknown
			}
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

// This global variable is required to export to driver framework written in C.
// Public core variables in NoirVisor are required to be named in lower-snake-case.
#[allow(non_upper_case_globals)]
#[no_mangle] pub static mut system_cr3:u64=0;

#[no_mangle] pub extern "C" fn noir_get_virtualization_supportability()->u32
{
	let mut vstr_raw:[u8;12]=[0;12];
	let cpu_manuf=ProcessorManufacturer::query(&mut vstr_raw);
	match cpu_manuf
	{
		ProcessorManufacturer::Intel|ProcessorManufacturer::VIA|ProcessorManufacturer::ZhaoXin=>0,
		ProcessorManufacturer::AMD|ProcessorManufacturer::Hygon=>SvmHypervisor::check_support(),
		_=>0
	}
}

#[no_mangle] pub extern "C" fn noir_is_virtualization_enabled()->bool
{
	SvmHypervisor::check_enabled()
}

#[no_mangle] pub extern "C" fn nvc_teardown_hypervisor()
{
	unsafe
	{
		#[allow(static_mut_refs)]
		if let Some(ref mut hypervisor)=&mut HVM
		{
			hypervisor.restore_system();
		}
	}
}

#[no_mangle] pub extern "C" fn nvc_build_hypervisor()->Status
{
	println!("Subverting the system...");
	let mut vstr_raw:[u8;12]=[0;12];
	let cpu_manuf=ProcessorManufacturer::query(&mut vstr_raw);
	let hv:Option<Box<dyn HypervisorEssentials>>=match cpu_manuf
	{
		ProcessorManufacturer::Intel|ProcessorManufacturer::VIA|ProcessorManufacturer::ZhaoXin=>
		{
			// Use Intel VT-x.
			unimplemented!("Intel VT-x is not supported yet!");
		}
		ProcessorManufacturer::AMD|ProcessorManufacturer::Hygon=>
		{
			// Use AMD-V.
			Some(Box::<SvmHypervisor>::new(SvmHypervisor::default()))
		}
		_=>
		{
			// Either this processor does not support virtualization at all,
			// or we don't know what kind of virtualization this processor supports.
			let r=core::str::from_utf8(&vstr_raw);
			if let Ok(s)=r
			{
				panic!("The processor vendor (\"{}\") is unknown!",s);
			}
			None
		}
	};
	if let Some(mut hypervisor)=hv
	{
		let st=hypervisor.subvert_system();
		unsafe 
		{
			HVM=Some(hypervisor);
		}
		st
	}
	else
	{
		NOIR_NOT_IMPLEMENTED
	}
}

// Panic handler is required for no_std crates. But it is NOT FOR tests!
// Put it under non-test conditional-compilation, or otherwise
// the rust-analyzer of VSCode will report duplicate panic_impl.

#[cfg(not(test))]
use core::panic::PanicInfo;

#[cfg(not(test))]
#[panic_handler] fn panic(panic: &PanicInfo)->!
{
	println!("\x1b[91m[PANIC] NoirVisor {}",panic);
	loop{}
}