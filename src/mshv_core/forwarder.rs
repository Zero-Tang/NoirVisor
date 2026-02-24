/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file forwards TLFS Hypercalls for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::sync::atomic::{AtomicU64, Ordering};

use log::*;

use super::{msr::HV_X64_MSR_HYPERCALL,cpuid::*};
use crate::xpf_core::{asm::msr::rdmsr, nvbdk::{noir_find_virt_by_phys, page_4kb_base, GprState, VolatileXmmState, PAGE_SIZE}, x86::cpuid::{CpuidLeaf, StandardProcessorFeatureIdentifiers}};

#[unsafe(no_mangle)] static MSHV_HVCALL_VA:AtomicU64=AtomicU64::new(0);

#[derive(Default)]
#[repr(C)] pub struct MshvForwardStack
{
	pub rax:u64,
	pub rcx:u64,
	pub rdx:u64,
	pub r8:u64,
	pub xmm_state:*mut VolatileXmmState
}

impl MshvForwardStack
{
	pub fn from_context(gpr_state:&GprState,xmm_state:*mut VolatileXmmState)->Self
	{
		Self
		{
			rax:gpr_state.rax,
			rcx:gpr_state.rcx,
			rdx:gpr_state.rdx,
			r8:gpr_state.r8,
			xmm_state
		}
	}

	pub fn to_context(&self,gpr_state:&mut GprState)
	{
		gpr_state.rax=self.rax;
		gpr_state.rdx=self.rdx;
		gpr_state.r8=self.r8;
	}
}

pub struct MshvCallForwarder
{
	hvcall_page:(u64,u64)
}

impl MshvCallForwarder
{
	pub fn new()->Option<Self>
	{
		let cpu_feat_id=StandardProcessorFeatureIdentifiers::cpuid();
		if cpu_feat_id.hypervisor()
		{
			let hv_vendor=MaxHypervisorLeafAndVendorString::cpuid();
			let hv_vendor_name=hv_vendor.vendor_name();
			let if_name=HypervisorVendorNeutralInterface::cpuid();
			let sig_name=if_name.interface_name();
			debug!("Higher-Level Hypervisor is detected from CPUID! Vendor: {hv_vendor_name}, Signature: {sig_name}");
			if sig_name=="Hv#1"
			{
				let phys=page_4kb_base(rdmsr(HV_X64_MSR_HYPERCALL));
				let virt=unsafe{noir_find_virt_by_phys(phys)};
				MSHV_HVCALL_VA.store(virt as u64,Ordering::Relaxed);
				debug!("Microsoft TLFS Hypercall interface: [phys: 0x{phys:X}, virt: {virt:p}]");
				Some
				(
					Self
					{
						hvcall_page:(phys,virt as u64)
					}
				)
			}
			else
			{
				info!("Higher-Level Hypervisor is not compliant to Microsoft TLFS!");
				None
			}
		}
		else
		{
			info!("No hypervisor is detected from CPUID!");
			None
		}
	}

	#[cfg(target_os="uefi")]
	pub fn new()->Option<Self>
	{
		None
	}

	pub fn is_rip_in_range(&self,rip:u64)->bool
	{
		let v=self.get_virt();
		(v..v+PAGE_SIZE as u64).contains(&rip)
	}

	pub fn get_phys(&self)->u64
	{
		self.hvcall_page.0
	}

	pub fn get_virt(&self)->u64
	{
		self.hvcall_page.1
	}
}