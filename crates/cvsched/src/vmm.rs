// NoirVisor CVM virtual machine scheduler

use core::ptr;

use alloc::{boxed::Box, sync::Arc, vec::Vec};
use htable::HandleTable;
use nvcvm::{interface::CvmHandle, status::Status};

use crate::{hvcall::*, sync::{Mutex, RwLock}};

pub struct VirtualMachine
{
	handle:u32,
	process_id:u32,
	vcpus:Vec<Option<Mutex<VirtualProcessor>>>
}

impl VirtualMachine
{
	const VCPU_PER_VM_LIMIT:u32=255;

	pub unsafe fn init(&mut self,handle:u32,process_id:u32)
	{
		self.handle=handle;
		self.process_id=process_id;
		unsafe
		{
			ptr::write(&raw mut self.vcpus,Vec::with_capacity(8));
		}
	}

	pub fn create_vcpu(&mut self,vcpu_id:u32)->Status
	{
		if vcpu_id>Self::VCPU_PER_VM_LIMIT
		{
			return Status::ACCESS_DENIED;
		}
		let i=vcpu_id as usize;
		while self.vcpus.len()<i
		{
			self.vcpus.push(None);
		}
		if self.vcpus[i].is_some()
		{
			Status::VCPU_ALREADY_CREATED
		}
		else
		{
			match VirtualProcessor::new(vcpu_id)
			{
				Ok(vcpu)=>
				{
					let mut vp_lk=Mutex::new(vcpu);
					unsafe
					{
						vp_lk.init();
					}
					self.vcpus[i]=Some(vp_lk);
					Status::SUCCESS
				}
				Err(st)=>st
			}
		}
	}

	pub fn delete_vcpu(&mut self,vcpu_id:u32)->Status
	{
		if vcpu_id>Self::VCPU_PER_VM_LIMIT
		{
			return Status::ACCESS_DENIED;
		}
		let i=vcpu_id as usize;
		match self.vcpus.get_mut(i)
		{
			Some(vp)=>
			{
				*vp=None;
				Status::SUCCESS
			}
			_=>Status::VCPU_NOT_EXIST
		}
	}

	pub fn run_vcpu(&self,vcpu_id:u32)->Status
	{
		let i=vcpu_id as usize;
		match self.vcpus.get(i)
		{
			Some(Some(vp))=>
			{
				// Lock must be exclusively acquired even though we do not access the internal contents.
				let _lk=vp.lock();
				let mut ctxt=CvmHypercallRunVcpuContext
				{
					handle:CvmHandle(self.handle),
					vcpu_id
				};
				unsafe
				{
					hypercall(CVM_HYPERCALL_RUN_VCPU,&raw mut ctxt)
				}
			}
			_=>Status::VCPU_NOT_EXIST
		}
	}
}

pub static VM_LIST:RwLock<HandleTable<Arc<RwLock<Box<VirtualMachine>>>>>=RwLock::new(HandleTable::new());

#[allow(dead_code)]
pub struct VirtualProcessor
{
	vcpu_id:u32,
}

impl VirtualProcessor
{
	pub fn new(vcpu_id:u32)->Result<Self,Status>
	{
		Ok
		(
			Self
			{
				vcpu_id
			}
		)
	}
}
