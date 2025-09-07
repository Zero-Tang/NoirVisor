/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the customizable VM engine for AMD-V.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

use alloc::{vec::Vec,boxed::Box};
// use nvcvm::interface::Vpcb;

use nvcvm::status::Status;

use crate::{cvm_core::*, xpf_core::{allocator::kmalloc::KernelAllocator, nvbdk::MemoryDescriptor}};
use super::{SvmHypervisor,npt::*};

pub struct SvmCustomVcpu
{
	vpcb:MemoryDescriptor<1,c_void>,
	vmcb:MemoryDescriptor<1,c_void>,
	apic_backing:Option<MemoryDescriptor<1,c_void>>,
	proc_id:u32,
	apic_id:u32
}

pub struct SvmCustomVm
{
	asid:u32,
	vcpus:Vec<Option<Box<SvmCustomVcpu>>>,
	iopm:MemoryDescriptor<3,usize>,
	msrpm:MemoryDescriptor<2,usize>,
	nptm:SvmCustomNptManager,
	smm_nptm:SvmCustomNptManager
}

pub struct SvmCustomNptManager
{
	pml4e:MemoryDescriptor<1,NptPml4e>,
	pdpte:Vec<MemoryDescriptor<1,NptPdpte>,KernelAllocator>,
	pde:Vec<MemoryDescriptor<1,NptPde>,KernelAllocator>,
	pte:Vec<MemoryDescriptor<1,NptPte>,KernelAllocator>
}

impl CvmVcpuOps for SvmCustomVcpu
{
	fn run(&mut self)->Status
	{
		Status::NOT_IMPLEMENTED
	}
}

impl SvmCustomVcpu
{
	fn new()->Self
	{
		Self
		{
			vmcb:MemoryDescriptor::null(),
			vpcb:MemoryDescriptor::null(),
			apic_backing:None,
			proc_id:0,
			apic_id:0
		}
	}
}

impl CvmVmOps for SvmCustomVm
{
	fn create_vcpu(&mut self,index:usize)->Status
	{
		let p=Box::new(SvmCustomVcpu::new());
		self.vcpus[index]=Some(p);
		Status::SUCCESS
	}

	fn release_vcpu(&mut self,_index:usize)
	{
		
	}

	fn set_mapping(&mut self)->Status
	{
		Status::NOT_IMPLEMENTED
	}

	fn reference_vcpu(&self,index:usize)->Option<&impl CvmVcpuOps>
	{
		match self.vcpus.get(index)
		{
			Some(Some(vcpu))=>Some(vcpu.as_ref()),
			_=>Option::<&SvmCustomVcpu>::None
		}
	}

	fn reference_vcpu_mut(&mut self,index:usize)->Option<&mut impl CvmVcpuOps>
	{
		match self.vcpus.get_mut(index)
		{
			Some(Some(vcpu))=>Some(vcpu.as_mut()),
			_=>Option::<&mut SvmCustomVcpu>::None
		}
	}
}

impl CvmHvOps for SvmHypervisor
{
	fn check_cap(&self,_code:u32,_buffer:&mut [u8])->Status
	{
		Status::NOT_IMPLEMENTED
	}

	fn create_vm(&mut self)->Result<CvmHandle,Status>
	{
		Err(Status::NOT_IMPLEMENTED)
	}

	fn release_vm(&mut self,_vm:CvmHandle)
	{
		
	}

	fn reference_vm(&self,_vm:CvmHandle)->Option<&impl CvmVmOps>
	{
		None::<&SvmCustomVm>
	}

	fn reference_vm_mut(&mut self,_vm:CvmHandle)->Option<&mut impl CvmVmOps>
	{
		None::<&mut SvmCustomVm>
	}
}