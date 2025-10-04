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

use alloc::boxed::Box;
use nvcvm::{interface::{CvmHandle, CvmMapping}, status::Status};

use crate::xpf_core::{allocator::kmalloc::KernelAllocator, pushlock::PushLock};

mod ffi;
#[cfg(target_arch="x86_64")] pub mod x86;

pub trait CvmHvOps
{
	fn create_vm(&mut self,process_id:u32)->Result<CvmHandle,Status>;
	fn release_vm(&mut self,vm:CvmHandle);
	fn create_vcpu(&self,vm:CvmHandle,vcpu_id:usize)->Result<(),Status>;
	fn release_vcpu(&self,vm:CvmHandle,vcpu_id:usize)->Result<(),Status>;
	fn run_vcpu(&self,vm:CvmHandle,vcpu_id:usize)->Result<(),Status>;
	fn set_mapping(&self,vm:CvmHandle,mapping:&CvmMapping)->Result<(),Status>;
	fn check_cap(&self,code:u32,buffer:&mut [u8])->Status;
}

pub static CUSTOMIZABLE_HYPERVISOR:PushLock<Option<Box<dyn CvmHvOps,KernelAllocator>>>=PushLock::new(None);

#[cfg(test)]
mod test
{
	use core::mem::MaybeUninit;
	use alloc::boxed::Box;
	use nvcvm::interface::{CvmHandle, CvmMapping, CvmMappingFlags, CVM_MAPPING_ASID_DEFAULT};

	use crate::{cvm_core::CUSTOMIZABLE_HYPERVISOR, svm_core::custom::SvmCustomHypervisor, xpf_core::allocator::kmalloc::KernelAllocator};

	#[repr(C,align(4096))]
	struct GuestPages<const N:usize>
	{
		pages:MaybeUninit<[u8;N]>
	}

	static GUEST_PAGES:GuestPages<65536>=GuestPages{pages:MaybeUninit::uninit()};

	fn svm_init()
	{
		let mut lk=CUSTOMIZABLE_HYPERVISOR.write();
		*lk=Some(Box::new_in(SvmCustomHypervisor::new(false).unwrap(),KernelAllocator));
		assert!(lk.is_some(),"Mock Hypervisor is not successfuly initialized!");

	}

	#[test] fn svm_create_vm_vcpu()
	{
		svm_init();
		let mut lk=CUSTOMIZABLE_HYPERVISOR.write();
		if let Some(hv)=&mut *lk
		{
			let r=hv.create_vm(0);
			assert_eq!(r,Ok(CvmHandle(0)));
			let vm_handle=r.unwrap();
			let r=hv.create_vcpu(vm_handle,0);
			assert_eq!(r,Ok(()));
		}
	}

	#[test] fn svm_set_mapping()
	{
		svm_init();
		let mut lk=CUSTOMIZABLE_HYPERVISOR.write();
		if let Some(hv)=&mut *lk
		{
			let r=hv.create_vm(0);
			assert_eq!(r,Ok(CvmHandle(0)));
			let vm_handle=r.unwrap();
			let mut map_info=CvmMappingFlags::from_bits(0);
			map_info.set_read(true);
			map_info.set_write(true);
			map_info.set_execute(true);
			map_info.with_cache_type(6);
			let map_info=CvmMapping
			{
				base_gpa:0,
				base_hva:unsafe{GUEST_PAGES.pages.assume_init_ref().as_ptr() as u64},
				size:size_of_val(&GUEST_PAGES) as u64,
				as_id:CVM_MAPPING_ASID_DEFAULT,
				flags:map_info
			};
			let r=hv.set_mapping(vm_handle,&map_info);
			assert!(r.is_ok());
		}
	}
}