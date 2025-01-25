/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the AMD-V driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, ptr::*};
use alloc::vec::Vec;
#[cfg(target_os="uefi")]
use exit::svm_apic_output_handler;
use npt::SvmNptManager;
use xpf_core::{bitmap::set_bitmap, hv_host::x86::*, ioflt::{IoAddressSpace, IoRegion}};

use crate::{xpf_core::{asm::{cpuid::cpuid,msr::*,svm::*,crdr::*,seg::*},nvstatus::*,x86::{cpuid::*,msr::*},nvbdk::*,dlalloc::*},*};
use amd64::{cpuid::*,msr::*};
use vmcb::*;

pub mod amd64;
// These modules aren't supposed to be public, but we need to permit dead_code for future utility.
#[allow(dead_code)] mod vmcb;
#[allow(dead_code)] mod decode;
#[allow(dead_code)] mod exit;
#[allow(dead_code)] mod npt;

// Limit stack size to 64KiB. Should be enough for most circumstances.
// FIXME: Implement runtime stack overflow detector.
pub const HYPERVISOR_STACK_SIZE:usize=PAGE_SIZE*16;

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

#[repr(C)] pub struct SvmNestedVcpu
{
	pub svme:bool,
	pub ignne:bool,
	pub hsave_pa:u64,
	pub vmcr:u64,
	pub svm_key:u64
}

#[repr(C)] pub struct SvmVcpu
{
	pub vmcb:MemoryDescriptor,
	pub hsave:MemoryDescriptor,
	pub hvmcb:MemoryDescriptor,
	pub hv_stack:*mut c_void,
	pub hypervisor:*mut c_void,
	pub ist:[*mut c_void;8],
	pub vcpu_id:u32,
	pub apic_id:u8,
	pub x2apic_id:u32,
	pub cpuid_fms:u32,
	pub host_cpu:HostProcessor,
	pub nested_hvm:SvmNestedVcpu,
	pub under_hvm:bool,
	// Features supported by the processors.
	pub decode_assists:bool,
	pub nrip_saving:bool,
	pub vmcb_clean:bool
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
			ist:[null_mut();8],
			vcpu_id:0,
			apic_id:0,
			x2apic_id:0,
			cpuid_fms:0,
			host_cpu:HostProcessor::default(),
			nested_hvm:SvmNestedVcpu
			{
				svme:false,
				ignne:false,
				hsave_pa:0,
				vmcr:0,
				svm_key:0
			},
			under_hvm:false,
			decode_assists:false,
			nrip_saving:false,
			vmcb_clean:false
		}
	}
}

extern "C"
{
	fn nvc_svm_subvert_processor_a(stack:*mut SvmStackTop);
	fn nvc_svm_guest_start();
}

/// # Safety
/// This function is unsafe because it's called from assembly.
/// DO NOT CALL THIS FUNCTION FROM RUST!
#[no_mangle] pub unsafe extern "C" fn nvc_svm_subvert_processor_i(vcpu:*mut SvmVcpu,gsp:u64)->u64
{
	(*vcpu).subvert_i(gsp)
}

impl SvmVcpu
{
	fn subvert_i(&mut self,gsp:u64)->u64
	{
		unsafe
		{
			let mut d=0;
			cpuid(CPUID_EXT_SECURE_VIRTUAL_MACHINE_FEATURE,0,None,None,None,Some(&mut d));
			// Setup supported features.
			self.decode_assists=(d&CPUID_SVM_DECODE_ASSIST)==CPUID_SVM_DECODE_ASSIST;
			self.nrip_saving=(d&CPUID_SVM_NEXT_RIP_SAVING)==CPUID_SVM_NEXT_RIP_SAVING;
			self.vmcb_clean=(d&CPUID_SVM_VMCB_CLEAN)==CPUID_SVM_VMCB_CLEAN;
			let hv=self.hypervisor as *mut SvmHypervisor;
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
			let mut ist:[*mut c_void;8]=self.ist;
			ist[1]=self.ist[1].byte_add(HYPERVISOR_STACK_SIZE);
			HostProcessor::build(&mut self.host_cpu,&ist);
			vmsave(self.hvmcb.phys);
			let idtr=(*hv).host.idt.get_reg();
			write_idtr(&raw const idtr);
			let gdtr=self.host_cpu.gdt.get_reg();
			write_gdtr(&raw const gdtr);
			write_tr(self.host_cpu.tr_sel);
			write_cr3((*hv).host.paging.cr3.phys);
			// Setup APIC ID.
			let (_,xid,c,_)=cpuid2(CPUID_STD_PROCESSOR_FEATURE,0);
			let (_,_,_,x2id)=cpuid2(CPUID_STD_EXTENDED_TOPOLOGY_INFORMATION,0);
			self.apic_id=(xid&0xFF) as u8;
			self.x2apic_id=x2id;
			// Check if we are under nested hypervisor.
			self.under_hvm=(c&CPUID_UNDER_HYPERVISOR)!=0;
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
			// Save Task Priority Register (CR8)
			vmwrite(self.vmcb.virt,AVIC_CONTROL,state.cr8&0xF);
			// Save Debug Registers.
			vmwrite(self.vmcb.virt,GUEST_DR6,state.dr6);
			vmwrite(self.vmcb.virt,GUEST_DR7,state.dr7);
			// Save rflags, rsp and rip.
			vmwrite(self.vmcb.virt,GUEST_RFLAGS,2u64);
			vmwrite(self.vmcb.virt,GUEST_RSP,gsp);
			vmwrite(self.vmcb.virt,GUEST_RIP,nvc_svm_guest_start as usize as u64);
			// Save Processor Hidden State.
			vmsave(self.hvmcb.phys);
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
			// Setup NPT.
			vmwrite(self.vmcb.virt,NPT_CR3,(*hv).nptm.pml4e.phys);
			vmwrite(self.vmcb.virt,NPT_CONTROL,NPT_CONTROL_ENABLE);
			// ASID is required in AMD-V.
			vmwrite(self.vmcb.virt,GUEST_ASID,1u32);
			// Load Guest State.
			vmload(self.vmcb.phys);
		}
		println!("Processor {} Completed setting up VMCB! (0x{:X})",self.vcpu_id,self.vmcb.phys);
		// "Return" puts the VMCB on rax register.
		self.vmcb.phys
	}

	fn subvert(&mut self)
	{
		println!("Processor {} entered subversion routine!",self.vcpu_id);
		// Enable SVM in EFER.
		let efer=rdmsr(MSR_EFER)|MSR_EFER_SVME|MSR_EFER_NXE;
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
			println!("Stack-Top of vCPU {}: {stack:p}",self.vcpu_id);
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
	pub vcpus:Vec<SvmVcpu>,
	pub msrpm:MemoryDescriptor,
	pub iopm:MemoryDescriptor,
	pub nptm:SvmNptManager,
	pub host:HostSystem,
	pub pio_space:IoAddressSpace<u16>,
	pub mmio_space:IoAddressSpace<u64>,
}

impl Default for SvmHypervisor
{
	fn default() -> Self
	{
		Self
		{
			vcpus:Vec::with_capacity(unsafe{noir_get_processor_count() as usize}),
			msrpm:MemoryDescriptor::null(),
			iopm:MemoryDescriptor::null(),
			nptm:SvmNptManager::default(),
			host:HostSystem::build(),
			pio_space:IoAddressSpace{regions:Vec::new()},
			mmio_space:IoAddressSpace{regions:Vec::new()}
		}
	}
}

impl SvmHypervisor
{
	fn cleanup(&mut self)
	{
		unimplemented!("Cleaning up...")
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
		(vmcr&MSR_VMCR_SVMDIS)==0
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
			Some(md)=>
			{
				self.msrpm=md;
				// Setup basic interceptions to MSRs that may interfere with SVM normal operations.
				// This is also for nested virtualization.
				unsafe
				{
					// Use macros to build MSR Permission Map more elegantly.
					macro_rules! intercept_read
					{
						($index:expr) =>
						{
							set_bitmap(self.msrpm.virt,0x2000,svm_msrpm_bit($index,false) as usize);
						};
					}
					macro_rules! intercept_write
					{
						($index:expr) =>
						{
							set_bitmap(self.msrpm.virt,0x2000,svm_msrpm_bit($index,true) as usize);
						};
					}
					macro_rules! intercept_any
					{
						($index:expr) =>
						{
							intercept_read!($index);
							intercept_write!($index);
						};
					}
					intercept_any!(MSR_EFER);
					intercept_any!(MSR_TSC_RATIO);
					intercept_any!(MSR_VMCR);
					intercept_any!(MSR_IGNNE);
					intercept_any!(MSR_SMM_CTRL);
					intercept_any!(MSR_HSAVE_PA);
					// There is no need to intercept read because the processor will always return zero on reads.
					intercept_write!(MSR_SVM_KEY);
				}
			}
			None=>fail_cleanup!("Failed to allocate MSR Permission-Map!")
		}
		match iopm
		{
			Some(md)=>self.iopm=md,
			None=>fail_cleanup!("Failed to allocate I/O Permission-Map!")
		}
		println!("MSRPM: 0x{:016X}, IOPM: 0x{:016X}",msrpm.unwrap().phys,iopm.unwrap().phys);
		let vcpu_count=unsafe{noir_get_processor_count()};
		for i in 0..vcpu_count
		{
			let mut vcpu=SvmVcpu::new();
			let hsave=alloc_contd_pages(PAGE_SIZE);
			let vmcb=alloc_contd_pages(PAGE_SIZE);
			let hvmcb=alloc_contd_pages(PAGE_SIZE);
			let stack=alloc_contd_pages(HYPERVISOR_STACK_SIZE);
			let ist1=alloc_contd_pages(HYPERVISOR_STACK_SIZE);
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
			match ist1
			{
				Some(md)=>vcpu.ist[1]=md.virt,
				None=>fail_cleanup!("Failed to allocate host IST1 stack for processor {}!",i)
			}
			vcpu.hypervisor=self as *mut Self as *mut c_void;
			vcpu.vcpu_id=i;
			self.vcpus.push(vcpu);
		}
		// Intercept APIC Accesses.
		#[cfg(target_os="uefi")]
		{
			let apic_bar=rdmsr(MSR_APIC_BASE);
			self.mmio_space.add_region(IoRegion::new("lapic",None,svm_apic_output_handler,page_4kb_base(apic_bar as usize) as u64,PAGE_SIZE as u64));
		}
		// Initialize NPT.
		self.nptm.build_identity_map();
		self.nptm.protect_allocated_pages();
		self.nptm.setup_mmio_filter(&self.mmio_space);
		self.nptm.protect_ci();
		unsafe
		{
			noir_generic_call(nvc_svm_subvert_processor_thunk,self as *mut Self as *mut c_void);
		}
		println!("System subversion completed!");
		NOIR_SUCCESS
	}

	fn restore_system(&mut self)->Status
	{
		println!("System restoration for AMD-V is not yet implemented!");
		NOIR_NOT_IMPLEMENTED
	}
}

#[no_mangle] extern "C" fn nvc_svm_subvert_processor_thunk(context:*mut c_void,processor_id:u32)
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