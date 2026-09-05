// NoirVisor CVM virtual machine scheduler

use core::{hint::cold_path, ptr, slice};

use alloc::{alloc::Layout ,sync::Arc, vec::Vec};
use htable::HandleTable;
use log::*;
use nvcvm::{hvcall::*, interface::{CvmMappingFlags, Vpcb}, status::{Status, unwrap_status}};

use crate::{hvcall::*, platform::{sync::{Mutex, RwLock}, kmap::UniversalPage}};

pub struct VirtualMachine
{
	pub(crate) handle:u32,
	process_id:u32,
	vcpus:Vec<Option<Arc<Mutex<VirtualProcessor>>>>
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
			ptr::write(&raw mut self.vcpus,Vec::with_capacity(Self::VCPU_PER_VM_LIMIT as usize));
		}
	}

	pub fn create_vcpu(&mut self,vcpu_id:u32)->Result<*mut Vpcb,Status>
	{
		if vcpu_id>Self::VCPU_PER_VM_LIMIT
		{
			return Err(Status::ACCESS_DENIED);
		}
		let i=vcpu_id as usize;
		while self.vcpus.len()<=i
		{
			self.vcpus.push(None);
		}
		if self.vcpus[i].is_some()
		{
			Err(Status::VCPU_ALREADY_CREATED)
		}
		else
		{
			let arc_vp:Arc<Mutex<VirtualProcessor>>=unsafe{Arc::try_new_uninit()?.assume_init()};
			if unsafe{arc_vp.init()}
			{
				let (vpcb_hpa,vpcb_uva)=
				{
					let mut vp=arc_vp.lock();
					unsafe
					{
						ptr::write(&raw mut vp.vpcb,UniversalPage::new()?);
					}
					vp.vcpu_id=vcpu_id;
					(vp.vpcb.hpa(),vp.vpcb.uva())
					// Mutex must be dropped before doing hypercall.
				};
				let mut ctxt=CvmHypercallCreateVcpuContext
				{
					handle:self.handle,
					vcpu_id,
					vpcb_hpa
				};
				unsafe
				{
					hypercall(CVM_HYPERCALL_CREATE_VCPU,&raw mut ctxt,size_of::<CvmHypercallCreateVcpuContext>())?;
				}
				self.vcpus[i]=Some(arc_vp);
				Ok(vpcb_uva)
			}
			else
			{
				cold_path();
				error!("Failed to initialize vCPU Mutex!");
				Err(Status::UNSUCCESSFUL)
			}
		}
	}

	pub fn delete_vcpu(&mut self,vcpu_id:u32)->Status
	{
		type Context=CvmHypercallDeleteVcpuContext;
		if vcpu_id>Self::VCPU_PER_VM_LIMIT
		{
			return Status::ACCESS_DENIED;
		}
		let mut ctxt=Context
		{
			handle:self.handle,
			vcpu_id
		};
		if let Err(st)=unsafe{hypercall(CVM_HYPERCALL_DELETE_VCPU,&raw mut ctxt,size_of::<Context>())}
		{
			error!("Failed to delete vCPU in hypervisor! Reason: {st}");
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

	pub fn set_mapping(&mut self,as_id:u32,gpa:u64,hpa_iter:&mut impl Iterator<Item=u64>,pages:usize,flags:CvmMappingFlags)->Status
	{
		type Context=CvmHypercallSetMappingContext;
		let size=size_of::<Context>()+(pages<<3);
		let layout=unsafe{Layout::from_size_align_unchecked(size,align_of::<Context>())};
		let ctxt:*mut Context=unsafe{alloc::alloc::alloc(layout).cast()};
		let st=
		{
			let ctxt=if ctxt.is_null()
			{
				return Status::INSUFFICIENT_RESOURCES;
			}
			else
			{
				unsafe
				{
					&mut *ctxt
				}
			};
			ctxt.handle=self.handle;
			ctxt.flags=flags;
			ctxt.as_id=as_id;
			ctxt.pages=pages as u32;
			ctxt.gpa=gpa;
			let hpa_list=unsafe{slice::from_raw_parts_mut(ctxt.hpa.as_mut_ptr(),pages)};
			for (i,hpa) in hpa_iter.enumerate()
			{
				hpa_list[i]=hpa;
			}
			unsafe
			{
				hypercall(CVM_HYPERCALL_SET_MAPPING,ctxt,size)
			}
		};
		unsafe
		{
			alloc::alloc::dealloc(ctxt.cast(),layout);
		}
		unwrap_status(st)
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
					handle:self.handle,
					vcpu_id
				};
				unsafe
				{
					unwrap_status(hypercall(CVM_HYPERCALL_RUN_VCPU,&raw mut ctxt,size_of::<CvmHypercallRunVcpuContext>()))
				}
			}
			_=>Status::VCPU_NOT_EXIST
		}
	}
}

pub static VM_LIST:RwLock<HandleTable<Arc<RwLock<VirtualMachine>>>>=RwLock::new(HandleTable::new());

#[allow(dead_code)]
pub struct VirtualProcessor
{
	vpcb:UniversalPage,
	vcpu_id:u32
}

impl VirtualProcessor
{
	
}
