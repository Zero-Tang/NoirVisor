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

use crate::{xpf_core::{asm::{cpuid::cpuid,msr::*,svm::*},nvstatus::*,x86::{cpuid::*,msr::*},nvbdk::*},*};
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
	pub vmcb:MemoryDescriptor,
	pub hsave:MemoryDescriptor,
	pub hvmcb:MemoryDescriptor,
	pub hv_stack:*mut c_void,
	pub hypervisor:*mut c_void,
	pub vcpu_id:u32,
	pub apic_id:u8,
	pub x2apic_id:u32,
	pub cpuid_fms:u32,
}

impl SvmVcpu
{
	fn new()->Self
	{
		Self
		{
			vmcb:MemoryDescriptor::null(),
			hsave:MemoryDescriptor::null(),
			hvmcb:MemoryDescriptor::null(),
			hv_stack:null_mut(),
			hypervisor:null_mut(),
			vcpu_id:0,
			apic_id:0,
			x2apic_id:0,
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
	fn subvert_i(&mut self,gsp:u64)->u64
	{
		unsafe
		{
			let mut state=ProcessorState::default();
			noir_save_processor_state(&raw mut state);
			// Setup Control Area.
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR1,INTERCEPT_VECTOR1_CPUID);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR1,INTERCEPT_VECTOR1_INVLPGA);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR1,INTERCEPT_VECTOR1_IO);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR1,INTERCEPT_VECTOR1_MSR);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR1,INTERCEPT_VECTOR1_SHUTDOWN);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR2,INTERCEPT_VECTOR2_VMRUN);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR2,INTERCEPT_VECTOR2_VMMCALL);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR2,INTERCEPT_VECTOR2_VMLOAD);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR2,INTERCEPT_VECTOR2_VMSAVE);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR2,INTERCEPT_VECTOR2_STGI);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR2,INTERCEPT_VECTOR2_CLGI);
			vmcb_or(self.vmcb.virt,INTERCEPT_VECTOR2,INTERCEPT_VECTOR2_SKINIT);
			// Setup Host State.
			vmsave(self.hvmcb.phys);
			// Setup APIC ID.
			let (_,xid,_,_)=cpuid2(CPUID_STD_PROCESSOR_FEATURE,0);
			let (_,_,_,x2id)=cpuid2(CPUID_STD_EXTENDED_TOPOLOGY_INFORMATION,0);
			self.apic_id=(xid&0xFF) as u8;
			self.x2apic_id=x2id;
			// Save Segment States.
			vmwrite_segment(self.vmcb.virt,GUEST_CS_SELECTOR,state.cs);
			vmwrite_segment(self.vmcb.virt,GUEST_DS_SELECTOR,state.ds);
			vmwrite_segment(self.vmcb.virt,GUEST_ES_SELECTOR,state.es);
			vmwrite_segment(self.vmcb.virt,GUEST_FS_SELECTOR,state.fs);
			vmwrite_segment(self.vmcb.virt,GUEST_GS_SELECTOR,state.gs);
			vmwrite_segment(self.vmcb.virt,GUEST_SS_SELECTOR,state.ss);
			vmwrite_segment(self.vmcb.virt,GUEST_TR_SELECTOR,state.tr);
			vmwrite_segment(self.vmcb.virt,GUEST_LDTR_SELECTOR,state.ldtr);
			vmwrite_segment(self.vmcb.virt,GUEST_IDTR_SELECTOR,state.idtr);
			vmwrite_segment(self.vmcb.virt,GUEST_GDTR_SELECTOR,state.gdtr);
			// Save Control Registers.
			vmwrite(self.vmcb.virt,GUEST_CR0,state.cr0);
			vmwrite(self.vmcb.virt,GUEST_CR2,state.cr2);
			vmwrite(self.vmcb.virt,GUEST_CR3,state.cr3);
			vmwrite(self.vmcb.virt,GUEST_CR4,state.cr4);
			// Save Debug Registers.
			vmwrite(self.vmcb.virt,GUEST_DR6,state.dr6);
			vmwrite(self.vmcb.virt,GUEST_DR7,state.dr7);
			// Save rflags, rsp and rip.
			vmwrite(self.vmcb.virt,GUEST_RFLAGS,2u64);
			vmwrite(self.vmcb.virt,GUEST_RSP,gsp);
			vmwrite(self.vmcb.virt,GUEST_RIP,nvc_svm_guest_start as u64);
			// Save Processor Hidden State.
			vmsave(self.vmcb.phys);
			// Save Model-Specific Registers.
			vmwrite(self.vmcb.virt,GUEST_PAT,state.pat);
			vmwrite(self.vmcb.virt,GUEST_EFER,state.efer);
			vmwrite(self.vmcb.virt,GUEST_STAR,state.star);
			vmwrite(self.vmcb.virt,GUEST_LSTAR,state.lstar);
			vmwrite(self.vmcb.virt,GUEST_CSTAR,state.cstar);
			vmwrite(self.vmcb.virt,GUEST_SFMASK,state.sfmask);
			vmwrite(self.vmcb.virt,GUEST_KERNEL_GS_BASE,state.gsswap);
			vmwrite(self.vmcb.virt,GUEST_SYSENTER_CS,state.sysenter_cs);
			vmwrite(self.vmcb.virt,GUEST_SYSENTER_ESP,state.sysenter_esp);
			vmwrite(self.vmcb.virt,GUEST_SYSENTER_EIP,state.sysenter_eip);
			// Setup IOPM and MSRPM.
			let hv=self.hypervisor as *mut SvmHypervisor;
			vmwrite(self.vmcb.virt,IOPM_PHYSICAL_ADDRESS,(*hv).iopm.phys);
			vmwrite(self.vmcb.virt,MSRPM_PHYSICAL_ADDRESS,(*hv).msrpm.phys);
			vmwrite(self.vmcb.virt,GUEST_ASID,1u32);
			// Load Guest State.
			vmload(self.vmcb.phys);
		}
		println!("Processor {} Completed setting up VMCB!",self.vcpu_id);
		// "Return" puts the VMCB on rax register.
		self.vmcb.phys
	}

	fn subvert(&mut self)
	{
		println!("Processor {}/{:p} entered subversion routine!",self.vcpu_id,self as *const Self);
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
			let stack:*mut SvmStackTop=self.hv_stack.byte_add(HYPERVISOR_STACK_SIZE-size_of::<SvmStackTop>()) as *mut SvmStackTop;
			(*stack).guest_vmcb_pa=self.vmcb.phys;
			(*stack).host_vmcb_pa=self.hvmcb.phys;
			(*stack).vcpu=self as *mut Self;
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
			vcpu.hypervisor=self as *mut Self as *mut c_void;
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