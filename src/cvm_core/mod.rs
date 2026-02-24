/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the Customizable Virtual Machine API of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, mem::MaybeUninit, ops::Range};

use alloc::{boxed::Box, vec::Vec};
use log::error;
use nvcvm::{interface::{CvmHandle, CvmMapping}, status::Status};

use crate::{cvm_core::ffi::noir_maximum_memslot_shift, xpf_core::{allocator::kmalloc::KernelAllocator, pushlock::PushLock}};

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

unsafe extern "C"
{
	#[cfg(windows)]
	fn noir_create_memory_slot(uva:*mut c_void,length:usize,slot:*mut *mut c_void)->bool;
	#[cfg(windows)]
	fn noir_remove_memory_slot(slot:*mut c_void);
	#[cfg(windows)]
	fn noir_get_pfn_from_memory_slot(slot:*const *mut c_void,offset:usize,result:*mut u64)->bool;
}

#[repr(C)] pub struct CvmMemorySlot
{
	start_gpa:u64,
	length:usize,
	#[cfg(windows)]
	opaque:*mut c_void,
}

impl PartialEq for CvmMemorySlot
{
	fn eq(&self, other: &Self) -> bool
	{
		self.start_gpa==other.start_gpa
	}
}

impl PartialOrd for CvmMemorySlot
{
	fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering>
	{
		self.start_gpa.partial_cmp(&other.start_gpa)
	}
}

impl PartialEq<u64> for CvmMemorySlot
{
	fn eq(&self, other: &u64) -> bool
	{
		self.as_gpa_range().contains(other)
	}
}

impl PartialOrd<u64> for CvmMemorySlot
{
	fn partial_cmp(&self, other: &u64) -> Option<core::cmp::Ordering>
	{
		use core::cmp::Ordering;
		if self.start_gpa>*other
		{
			Some(Ordering::Greater)
		}
		else if (self.start_gpa+self.length as u64)<=*other
		{
			Some(Ordering::Less)
		}
		else
		{
			Some(Ordering::Equal)
		}
	}
}

impl CvmMemorySlot
{
	/// Creates a memory slot that pins user-mode memory.
	/// 
	/// ## Safety
	/// You must ensure `uva` and `length` are valid.
	pub unsafe fn create(uva:*mut c_void,start_gpa:u64,length:usize)->Option<Self>
	{
		let mut ret:MaybeUninit<Self>=MaybeUninit::uninit();
		unsafe
		{
			let r=ret.assume_init_mut();
			r.start_gpa=start_gpa;
			r.length=length;
			if noir_create_memory_slot(uva,length,&raw mut r.opaque)
			{
				Some(ret.assume_init())
			}
			else
			{
				None
			}
		}
	}

	/// Get PFN of the memory in slot specified by `offset`.
	pub fn get_pfn(&self,offset:usize)->Option<u64>
	{
		let mut ret:u64=0;
		unsafe
		{
			if noir_get_pfn_from_memory_slot(&raw const self.opaque,offset,&raw mut ret)
			{
				Some(ret)
			}
			else
			{
				None
			}
		}
	}

	pub const fn as_gpa_range(&self)->Range<u64>
	{
		self.start_gpa..self.start_gpa+self.length as u64
	}

	pub const fn is_overlapped(&self,range:&Range<u64>)->bool
	{
		let this=self.as_gpa_range();
		(this.end>range.start)&&(range.end>this.start)
	}
}

impl Drop for CvmMemorySlot
{
	fn drop(&mut self)
	{
		unsafe
		{
			noir_remove_memory_slot(self.opaque);
		}
	}
}

pub struct CvmGpaSpace
{
	// The memory slots must be kept sorted.
	mem_slots:Vec<CvmMemorySlot,KernelAllocator>
}

impl Default for CvmGpaSpace
{
	fn default() -> Self
	{
		Self::new()
	}
}

impl CvmGpaSpace
{
	pub const fn new()->Self
	{
		Self
		{
			mem_slots:Vec::new_in(KernelAllocator)
		}
	}

	pub fn unregister_memory(&mut self,start_gpa:u64,length:usize)->Result<(),Status>
	{
		match self.mem_slots.binary_search_by(|slot| slot.partial_cmp(&start_gpa).unwrap())
		{
			Ok(i)=>
			{
				if self.mem_slots[i].start_gpa==start_gpa && self.mem_slots[i].length==length
				{
					self.mem_slots.remove(i);
					Ok(())
				}
				else
				{
					Err(Status::INVALID_PARAMETER)
				}
			}
			_=>Err(Status::INVALID_PARAMETER)
		}
	}

	/// The `register_memory` function registers physical memory into GPA Space.
	/// 
	/// Current implementation does not allow overlapped memory slots.
	/// 
	/// ## Safety
	/// You must make sure `uva` and `length` covers a range of valid memory!
	pub unsafe fn register_memory(&mut self,uva:*mut c_void,start_gpa:u64,length:usize)->Result<(),Status>
	{
		// Check for overlaps. We currently don't allow overlapping memory-slots.
		let index=match self.mem_slots.binary_search_by(|slot| slot.partial_cmp(&start_gpa).unwrap())
		{
			Ok(i)=>i,
			Err(i)=>i
		};
		let new_range=start_gpa..start_gpa+length as u64;
		if index<self.mem_slots.len()
		{
			if self.mem_slots[index].is_overlapped(&new_range)
			{
				error!("Overlapped!");
				return Err(Status::INVALID_PARAMETER);
			}
			if let Some(slot)=self.mem_slots.get(index+1) && slot.is_overlapped(&new_range)
			{
				error!("Overlapped!");
				return Err(Status::INVALID_PARAMETER);
			}
		}
		// Calculate increment size.
		let increment:usize=1<<unsafe{noir_maximum_memslot_shift};
		for i in (0..length).step_by(increment)
		{
			let remainder=length-(i<<unsafe{noir_maximum_memslot_shift});
			let size=if remainder>increment {increment} else {remainder};
			if let Some(slot)=unsafe{CvmMemorySlot::create(uva,start_gpa,size)}
			{
				self.mem_slots.insert(index,slot);
			}
			else
			{
				return Err(Status::INSUFFICIENT_RESOURCES)
			}
		}
		Ok(())
	}
}

pub static CUSTOMIZABLE_HYPERVISOR:PushLock<Option<Box<dyn CvmHvOps,KernelAllocator>>>=PushLock::new(None);

#[cfg(test)]
mod test
{
	use core::{ffi::c_void, mem::MaybeUninit};
	use alloc::boxed::Box;
	use nvcvm::{interface::{CVM_MAPPING_ASID_DEFAULT, CvmHandle, CvmMapping, CvmMappingFlags}, status::Status};

	use crate::{cvm_core::{CUSTOMIZABLE_HYPERVISOR, CvmGpaSpace}, svm_core::custom::SvmCustomHypervisor, xpf_core::{allocator::kmalloc::KernelAllocator, nvbdk::page_4kb_mult}};

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

	#[test] fn gpa_space_add_slot()
	{
		let mut gpa_space=CvmGpaSpace::new();
		// Map the pages out-of-order.
		let order:[usize;16]=[9,1,8,0,11,15,12,2,4,14,3,7,5,6,10,13];
		for (index,shift) in order.iter().enumerate()
		{
			let r=unsafe
			{
				let s=GUEST_PAGES.pages.assume_init_ref();
				gpa_space.register_memory(s.as_ptr().byte_add(page_4kb_mult(index)) as *mut c_void,0x400000+page_4kb_mult(*shift) as u64,0x1000)
			};
			assert!(r.is_ok(),"Failed to register memory! index={index}, shift={shift}, status=0x{:X}",r.unwrap_err().0);
		}
		assert_eq!(gpa_space.mem_slots.len(),order.len());
		// Check if the slots are sorted in spite of the out-of-order registration.
		assert!(gpa_space.mem_slots.is_sorted());
		// Insert existing memory slots.
		for i in 0..16
		{
			let offset:usize=page_4kb_mult(i);
			let r=unsafe
			{
				let s=GUEST_PAGES.pages.assume_init_ref();
				gpa_space.register_memory(s.as_ptr().byte_add(offset) as *mut c_void,0x400000+offset as u64,0x1000)
			};
			assert_eq!(r,Err(Status::INVALID_PARAMETER),"Did not encounter invalid-parameter failure! status=0x{:X}",r.unwrap_err().0);
		}
	}

	#[test] fn gpa_space_removal()
	{
		let mut gpa_space=CvmGpaSpace::new();
		// Map the pages out-of-order.
		let order:[usize;16]=[9,1,8,0,11,15,12,2,4,14,3,7,5,6,10,13];
		for (index,shift) in order.iter().enumerate()
		{
			let r=unsafe
			{
				let s=GUEST_PAGES.pages.assume_init_ref();
				gpa_space.register_memory(s.as_ptr().byte_add(page_4kb_mult(index)) as *mut c_void,0x400000+page_4kb_mult(*shift) as u64,0x1000)
			};
			assert!(r.is_ok(),"Failed to register memory! index={index}, shift={shift}, status=0x{:X}",r.unwrap_err().0);
		}
		// Remove a few of them.
		let shift:[u64;6]=[14,8,11,2,0,6];
		for s in shift
		{
			let offset:u64=page_4kb_mult(s);
			let r=gpa_space.unregister_memory(0x400000+offset,0x1000);
			assert!(r.is_ok(),"Failed to unregister memory! offset=0x{offset:X}, status=0x{:X}",r.unwrap_err().0);
		}
	}
}