/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the Customizable Virtual Machine API of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use nvcvm::status::Status;

mod ffi;
#[cfg(target_arch="x86_64")] pub mod x86;

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmHandle(pub usize);

pub trait CvmHvOps
{
	fn create_vm(&mut self)->Result<CvmHandle,Status>;
	fn release_vm(&mut self,vm:CvmHandle);
	fn reference_vm(&self,vm:CvmHandle)->Option<&impl CvmVmOps>;
	fn reference_vm_mut(&mut self,vm:CvmHandle)->Option<&mut impl CvmVmOps>;
	fn check_cap(&self,code:u32,buffer:&mut [u8])->Status;
}

pub trait CvmVmOps
{
	fn create_vcpu(&mut self,index:usize)->Status;
	fn release_vcpu(&mut self,index:usize);
	fn set_mapping(&mut self)->Status;
	fn reference_vcpu(&self,index:usize)->Option<&impl CvmVcpuOps>;
	fn reference_vcpu_mut(&mut self,index:usize)->Option<&mut impl CvmVcpuOps>;
}

pub trait CvmVcpuOps
{
	fn run(&mut self)->Status;
}