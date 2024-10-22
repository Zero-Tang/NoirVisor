/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file is the AMD-V driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, ptr::null_mut};
use alloc::vec::Vec;

use crate::{xpf_core::{asm::{cpuid::cpuid,msr::*},nvstatus::*,x86::{cpuid::*,msr::*},nvbdk::*},*};
use amd64::{cpuid::*,msr::*};
use vmcb::*;

pub mod amd64;
pub mod vmcb;
pub mod exit;

pub const HYPERVISOR_STACK_SIZE:usize=PAGE_SIZE*4;

#[repr(C)] pub struct SvmStackTop
{
	pub guest_vmcb_pa:u64,
	pub host_vmcb_pa:u64,
	pub vcpu:*mut SvmVcpu,
	pub custom_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub nested_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub proc_id:u32,
	pub reserved:u32
}

#[derive(Copy,Clone)] #[repr(C)] pub struct SvmVcpu
{
	pub hsave:MemoryDescriptor,
	pub vmcb:MemoryDescriptor,
	pub hvmcb:MemoryDescriptor,
	pub hv_stack:*mut c_void,
	pub vcpu_id:u32,
	pub apic_id:u32,
	pub cpuid_fms:u32,
}

impl SvmVcpu
{
	fn new()->Self
	{
		Self
		{
			hsave:MemoryDescriptor::null(),
			vmcb:MemoryDescriptor::null(),
			hvmcb:MemoryDescriptor::null(),
			hv_stack:null_mut(),
			vcpu_id:0,
			apic_id:0,
			cpuid_fms:0,
		}
	}
}

extern "C"
{
	fn nvc_svm_subvert_processor_a(stack:*mut SvmStackTop);
	fn nvc_svm_guest_start();
}

#[no_mangle] pub unsafe extern "C" fn nvc_svm_subvert_processor_i(vcpu:*mut SvmVcpu,gsp:u64)
{
	(*vcpu).subvert_i(gsp);
}

impl SvmVcpu
{
	extern "C" fn subvert_i(&mut self,gsp:u64)->u64
	{
		println!("Processor {} is setting up Guest VMCB...",self.vcpu_id);
		unsafe
		{
			let mut state=ProcessorState::default();
			noir_save_processor_state(&raw mut state);
			vmwrite(self.vmcb.virt,GUEST_RSP,gsp);
			vmwrite(self.vmcb.virt,GUEST_RIP,nvc_svm_guest_start as u64);
		}
		self.vmcb.phys
	}

	fn subvert(&mut self)
	{
		println!("Processor {} entered subversion routine!",self.vcpu_id);
		// Enable SVM in EFER.
		let efer=rdmsr(MSR_EFER)|MSR_EFER_SVME;
		wrmsr(MSR_EFER,efer);
		// Block A20M & Redirect INIT
		// Intel blocks A20M in vmxon, why not we do this as well?
		// Redirecting INIT signal to #SX exception will allow us to
		// intercept INIT signal without leaving it pending.
		let vmcr=rdmsr(MSR_VMCR)|MSR_VMCR_R_INIT&!MSR_VMCR_DISA20M;
		wrmsr(MSR_VMCR,vmcr);
		// Set the HSAVE Area.
		wrmsr(MSR_HSAVE_PA,self.hsave.phys);
		// Cache the Family-Model-Stepping Information. We'll use it for INIT-Signal Emulation.
		cpuid(CPUID_STD_PROCESSOR_FEATURE,0,Some(&mut self.cpuid_fms),None,None,None);
		// Initialize Hypervisor Context stack.
		unsafe
		{
			let stack:*mut SvmStackTop=(self.hv_stack as *mut u8).add(HYPERVISOR_STACK_SIZE-size_of::<SvmStackTop>()) as *mut SvmStackTop;
			(*stack).guest_vmcb_pa=self.vmcb.phys;
			(*stack).host_vmcb_pa=self.hvmcb.phys;
			(*stack).proc_id=self.vcpu_id;
			(*stack).custom_vcpu=null_mut();
			(*stack).nested_vcpu=null_mut();
			(*stack).reserved=0;
			nvc_svm_subvert_processor_a(stack);
		}
		println!("Processor {} completed subversion!",self.vcpu_id);
	}
}

#[repr(C)] pub struct SvmHypervisor
{
	pub vcpu_count:u32,
	pub vcpus:Vec<SvmVcpu>,
	pub msrpm:MemoryDescriptor,
	pub iopm:MemoryDescriptor
}

impl SvmHypervisor
{
	pub fn new()->Self
	{
		Self
		{
			vcpu_count:0,
			vcpus:Vec::new(),
			msrpm:MemoryDescriptor::null(),
			iopm:MemoryDescriptor::null()
		}
	}

	fn cleanup(&mut self)->()
	{
		println!("Cleaning up...")
	}
}

impl HypervisorCapabilities for SvmHypervisor
{
	fn check_support()->u32
	{
		let mut c:u32=0;
		cpuid(CPUID_EXT_PROCESSOR_FEATURE,0,None,None,Some(&mut c),None);
		if c&CPUID_SVM==CPUID_SVM
		{
			let mut b:u32=0;
			let mut d:u32=0;
			cpuid(CPUID_EXT_SECURE_VIRTUAL_MACHINE_FEATURE,0,None,Some(&mut b),None,Some(&mut d));
			// At least one ASID should be available.
			if b>0
			{
				let mut ret:u32=1;
				// Nested Paging
				ret|=if (d&CPUID_SVM_NPT)==CPUID_SVM_NPT {2} else {0};
				return ret;
			}
		}
		0
	}

	fn check_enabled()->bool
	{
		let vmcr=rdmsr(MSR_VMCR);
		return (vmcr&MSR_VMCR_SVMDIS)==0;
	}
}

impl HypervisorEssentials for SvmHypervisor
{
	fn subvert_system(&mut self)->Status
	{
		// Use a macro to help cleanup-on-fail and return.
		macro_rules! fail_cleanup
		{
			($($arg:tt)*) =>
			{
				{
					print!("{}\n",format_args!($($arg)*));
					self.cleanup();
					return NOIR_INSUFFICIENT_RESOURCES;
				}
			};
		}
		println!("Subverting the system with AMD-V...");
		// Allocate various stuff. Note that they are required to be raw-pointer.
		let msrpm=alloc_contd_pages(PAGE_SIZE*2);
		let iopm=alloc_contd_pages(PAGE_SIZE*3);
		match msrpm
		{
			Some(md)=>self.msrpm=md,
			None=>fail_cleanup!("Failed to allocate MSR Permission-Map!")
		}
		match iopm
		{
			Some(md)=>self.iopm=md,
			None=>fail_cleanup!("Failed to allocate I/O Permission-Map!")
		}
		self.vcpu_count=unsafe{noir_get_processor_count()};
		for i in 0..self.vcpu_count
		{
			let mut vcpu=SvmVcpu::new();
			let hsave=alloc_contd_pages(PAGE_SIZE);
			let vmcb=alloc_contd_pages(PAGE_SIZE);
			let hvmcb=alloc_contd_pages(PAGE_SIZE);
			let stack=alloc_contd_pages(HYPERVISOR_STACK_SIZE);
			match hsave
			{
				Some(md)=>vcpu.hsave=md,
				None=>fail_cleanup!("Failed to allocate HSAVE Area for processor {}!",i)
			}
			match vmcb
			{
				Some(md)=>vcpu.vmcb=md,
				None=>fail_cleanup!("Failed to allocate VMCB for processor {}!",i)
			}
			match hvmcb
			{
				Some(md)=>vcpu.hvmcb=md,
				None=>fail_cleanup!("Failed to allocate Host-VMCB for processor {}!",i)
			}
			match stack
			{
				Some(md)=>vcpu.hv_stack=md.virt,
				None=>fail_cleanup!("Failed to allocate hypervisor stack for processor {}!",i)
			}
			vcpu.vcpu_id=i;
			self.vcpus.push(vcpu);
		}
		unsafe
		{
			noir_generic_call(nvc_svm_subvert_processor_thunk,self as *mut Self as *mut c_void);
		}
		NOIR_SUCCESS
	}

	fn restore_system(&mut self)->Status
	{
		println!("System restoration for AMD-V is not yet implemented!");
		NOIR_NOT_IMPLEMENTED
	}
}

#[no_mangle] extern "C" fn nvc_svm_subvert_processor_thunk(context:*mut c_void,processor_id:u32)->()
{
	let hv=context as *mut SvmHypervisor;
	let vp=unsafe{(*hv).vcpus.get_mut(processor_id as usize)};
	println!("Subverting processor {} with AMD-V...",processor_id);
	match vp
	{
		Some(vcpu)=>vcpu.subvert(),
		None=>panic!("WTF? Processor ID out of bounds!\n")
	}
}