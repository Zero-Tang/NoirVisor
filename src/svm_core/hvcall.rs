/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file handles Hypercalls in AMD-V of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{alloc::Layout, ffi::c_void, mem::MaybeUninit};

use log::warn;
use nvcvm::{hvcall::*, interface::{CvmHandle, CvmMapping}, status::{Status, unwrap_status}};
use paste::paste;

use crate::{cvm_core::x86::*, disasm::emulator::EmulatorOps, xpf_core::x86::interrupts::*};
use super::*;

macro_rules! check_context_size
{
	($name:tt,$size:expr)=>
	{
		paste!
		{
			if $size<size_of::<[<CvmHypercall $name Context>]>()
			{
				return Ok(Status::BUFFER_TOO_SMALL);
			}
		}
	};
}

impl SvmVcpu
{
	fn hvcall_unknown(&mut self,context:*mut c_void,_context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		let code=self.get_gpr(1) as u32;
		warn!("Unknown Hypercall Code 0x{code:X} is called! Context={context:p}");
		Err((INVALID_OPCODE_FAULT,None))
	}

	fn hvcall_restore(&mut self,_context:*mut c_void,_context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		// Since NoirVisor is strictly Type-I Hypervisor, this hypercall code is reserved now.
		Err((INVALID_OPCODE_FAULT,None))
	}

	fn hvcall_alloc_tlb_tag(&mut self,_context:*mut c_void,_context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		// This hypercall code is reserved now.
		Err((INVALID_OPCODE_FAULT,None))
	}

	fn hvcall_exit_boot_services(&mut self,_context:*mut c_void,_context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		// If NoirVisor is loaded as a UEFI runtime-driver, this routine will be called by Guest OS.
		// Current implementation just outputs a log and returns.
		info!("ExitBootServices event is triggered! UEFI now enters Runtime Stage!");
		Ok(Status::SUCCESS)
	}

	fn hvcall_get_cap(&mut self,_context:*mut c_void,_context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		error!("Get-Capability is not supported!");
		Ok(Status::NOT_IMPLEMENTED)
	}

	fn hvcall_create_vm(&mut self,context:*mut c_void,context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		check_context_size!(CreateVm,context_size);
		match create_vm()
		{
			Ok(vm)=>
			{
				let buff=vm.0.to_ne_bytes();
				match self.write_virt(context as u64,&buff)
				{
					Ok(_)=>Ok(Status::SUCCESS),
					Err((code,fault_va))=>
					{
						error!("Page-Fault while writing to CVM-Hypercall Context! CR2: {fault_va}, Code: {code}");
						self.write_cr2(fault_va);
						Err((PAGE_FAULT,Some(code.into_bits())))
					}
				}
			}
			Err(st)=>Ok(st)
		}
	}

	fn hvcall_delete_vm(&mut self,context:*mut c_void,context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		check_context_size!(DeleteVm,context_size);
		let mut buff =[0;4];
		let vm=match self.read_virt(context as u64,&mut buff)
		{
			Ok(_)=>CvmHandle(u32::from_ne_bytes(buff)),
			Err((code,fault_va))=>
			{
				error!("Page-Fault while reading from CVM-Hypercall Context! CR2: {fault_va}, Code: {code}");
				self.write_cr2(fault_va);
				return Err((PAGE_FAULT,Some(code.into_bits())));
			}
		};
		Ok(unwrap_status(delete_vm(vm)))
	}

	fn hvcall_create_vcpu(&mut self,context:*mut c_void,context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		check_context_size!(CreateVcpu,context_size);
		let mut buff:MaybeUninit<CvmHypercallCreateVcpuContext>=MaybeUninit::uninit();
		let ctxt=match self.read_virt(context as u64,unsafe{slice::from_raw_parts_mut(buff.as_mut_ptr().cast(),size_of_val(&buff))})
		{
			Ok(_)=>unsafe
			{
				buff.assume_init_ref()
			}
			Err((code,fault_va))=>
			{
				error!("Page-Fault while reading from CVM-Hypercall Context! CR2: {fault_va}, Code: {code}");
				self.write_cr2(fault_va);
				return Err((PAGE_FAULT,Some(code.into_bits())));
			}
		};
		Ok(unwrap_status(create_vcpu(CvmHandle(ctxt.handle),ctxt.vcpu_id,ctxt.vpcb_hpa)))
	}

	fn hvcall_delete_vcpu(&mut self,context:*mut c_void,context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		check_context_size!(DeleteVcpu,context_size);
		let mut buff:MaybeUninit<CvmHypercallDeleteVcpuContext>=MaybeUninit::uninit();
		let ctxt=match self.read_virt(context as u64,unsafe{slice::from_raw_parts_mut(buff.as_mut_ptr().cast(),size_of_val(&buff))})
		{
			Ok(_)=>unsafe
			{
				buff.assume_init_ref()
			}
			Err((code,fault_va))=>
			{
				error!("Page-Fault while reading from CVM-Hypercall Context! CR2: {fault_va}, Code: {code}");
				self.write_cr2(fault_va);
				return Err((PAGE_FAULT,Some(code.into_bits())));
			}
		};
		Ok(unwrap_status(delete_vcpu(CvmHandle(ctxt.handle),ctxt.vcpu_id)))
	}

	fn hvcall_set_mapping(&mut self,context:*mut c_void,context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		check_context_size!(SetMapping,context_size);
		let mut buff:MaybeUninit<CvmHypercallSetMappingContext>=MaybeUninit::uninit();
		let ctxt=match self.read_virt(context as u64,unsafe{slice::from_raw_parts_mut(buff.as_mut_ptr().cast(),size_of_val(&buff))})
		{
			Ok(_)=>unsafe
			{
				buff.assume_init_ref()
			}
			Err((code,fault_va))=>
			{
				error!("Page-Fault while reading from CVM-Hypercall Context! CR2: {fault_va}, Code: {code}");
				self.write_cr2(fault_va);
				return Err((PAGE_FAULT,Some(code.into_bits())));
			}
		};
		let pages=ctxt.pages as usize;
		let hpa_list_size=pages<<3;
		if hpa_list_size<(context_size-size_of_val(&buff))
		{
			error!("Context is too small to contain HPA List!");
			return Ok(Status::BUFFER_TOO_SMALL);
		}
		unsafe
		{
			let layout=Layout::from_size_align_unchecked(hpa_list_size,align_of::<u64>());
			let hpa_list_buff=alloc::alloc::alloc(layout);
			if hpa_list_buff.is_null()
			{
				return Ok(Status::BUFFER_TOO_SMALL);
			}
			let r=match self.read_virt(context as u64+size_of_val(&buff) as u64,slice::from_raw_parts_mut(hpa_list_buff,hpa_list_size))
			{
				Ok(_)=>
				{
					let hpa_list:&[u64]=slice::from_raw_parts(hpa_list_buff.cast(),pages);
					let mapping=CvmMapping
					{
						base_gpa:ctxt.gpa,
						size:page_4kb_mult(pages as u64),
						as_id:ctxt.as_id,
						flags:ctxt.flags
					};
					match set_mapping(CvmHandle(ctxt.handle),&mapping,hpa_list)
					{
						Ok(_)=>Ok(Status::SUCCESS),
						Err(st)=>Ok(st)
					}
				}
				Err((code,fault_va))=>
				{
					error!("Page-Fault while reading from CVM-Hypercall Context! CR2: {fault_va}, Code: {code}");
					self.write_cr2(fault_va);
					Err((PAGE_FAULT,Some(code.into_bits())))
				}
			};
			alloc::alloc::dealloc(hpa_list_buff,layout);
			r
		}
	}

	fn hvcall_run_vcpu(&mut self,context:*mut c_void,context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		check_context_size!(RunVcpu,context_size);
		let mut buff:MaybeUninit<CvmHypercallRunVcpuContext>=MaybeUninit::uninit();
		let ctxt=match self.read_virt(context as u64,unsafe{slice::from_raw_parts_mut(buff.as_mut_ptr().cast(),context_size)})
		{
			Ok(_)=>unsafe
			{
				buff.assume_init_ref()
			}
			Err((code,fault_va))=>
			{
				error!("Page-Fault while reading from CVM-Hypercall Context! CR2: {fault_va}, Code: {code}");
				self.write_cr2(fault_va);
				return Err((PAGE_FAULT,Some(code.into_bits())));
			}
		};
		let hv=unsafe{&*(self.hypervisor as *const SvmHypervisor)};
		let vm=match hv.vm_list.clone().read().get(ctxt.handle as usize)
		{
			Some(Some(vm))=>vm.read().vcpus.clone(),
			_=>return Ok(Status::INVALID_PARAMETER)
		};
		let Some(vcpu_list_lk)=vm.try_read() else
		{
			error!("Failed to acquire vcpu list lock!");
			return Ok(Status::SYNCHRONIZATION_VIOLATION);
		};
		let vcpu=match vcpu_list_lk.get(ctxt.vcpu_id as usize)
		{
			Some(Some(vcpu))=>vcpu.clone(),
			_=>return Ok(Status::SYNCHRONIZATION_VIOLATION)
		};
		let Some(vcpu_lk)=vcpu.try_lock() else
		{
			error!("Failed to acquire vCPU mutex!");
			return Ok(Status::SYNCHRONIZATION_VIOLATION);
		};
		// Save the VM-Handle and vCPU ID.
		self.cv_host_save.from_vcpu=Some((CvmHandle(ctxt.handle),ctxt.vcpu_id));
		// We need to circumvent the RAII in order to keep the vCPU's mutex locked.
		self.switch_world_to_guest(spin::MutexGuard::leak(vcpu_lk));
		// We did not save the success code in the rax yet. Do it here.
		self.cv_host_save.gpr.rax=Status::SUCCESS.0 as u64;
		Ok(Status::SUCCESS)
	}

	fn hvcall_request_event(&mut self,_context:*mut c_void,_context_size:usize)->Result<Status,(u8,Option<u32>)>
	{
		error!("Request-Event is not supported!");
		Ok(Status::NOT_IMPLEMENTED)
	}
}

pub(super) type SvmHypercallHandler=fn(&mut SvmVcpu,context:*mut c_void,context_size:usize)->Result<Status,(u8,Option<u32>)>;

static SVM_HYPERCALL_HANDLER_BASE:[SvmHypercallHandler;4]=
[
	SvmVcpu::hvcall_unknown,
	SvmVcpu::hvcall_restore,
	SvmVcpu::hvcall_alloc_tlb_tag,
	SvmVcpu::hvcall_exit_boot_services
];

static SVM_HYPERCALL_HANDLER_CVM:[SvmHypercallHandler;0x30]=
{
	let mut x:[SvmHypercallHandler;0x30]=[SvmVcpu::hvcall_unknown;0x30];
	const fn set(array:&mut [SvmHypercallHandler],code:u32,handler:SvmHypercallHandler)
	{
		array[(code as usize)&0xFFFF]=handler;
	}
	set(&mut x,CVM_HYPERCALL_GET_CAPABILITY,SvmVcpu::hvcall_get_cap);
	set(&mut x,CVM_HYPERCALL_CREATE_VM,SvmVcpu::hvcall_create_vm);
	set(&mut x,CVM_HYPERCALL_DELETE_VM,SvmVcpu::hvcall_delete_vm);
	set(&mut x,CVM_HYPERCALL_CREATE_VCPU,SvmVcpu::hvcall_create_vcpu);
	set(&mut x,CVM_HYPERCALL_DELETE_VCPU,SvmVcpu::hvcall_delete_vcpu);
	set(&mut x,CVM_HYPERCALL_SET_MAPPING,SvmVcpu::hvcall_set_mapping);
	set(&mut x,CVM_HYPERCALL_RUN_VCPU,SvmVcpu::hvcall_run_vcpu);
	set(&mut x,CVM_HYPERCALL_REQUEST_EVENT,SvmVcpu::hvcall_request_event);
	x
};

static SVM_HYPERCALL_HANDLER_GROUPS:[&[SvmHypercallHandler];2]=[&SVM_HYPERCALL_HANDLER_BASE,&SVM_HYPERCALL_HANDLER_CVM];

#[inline] pub(super) fn dispatch_hypercall(code:u32)->SvmHypercallHandler
{
	let group=(code>>16) as usize;
	match SVM_HYPERCALL_HANDLER_GROUPS.get(group)
	{
		Some(&g)=>
		{
			let index=(code&0xffff) as usize;
			match g.get(index)
			{
				Some(&h)=>h,
				None=>SvmVcpu::hvcall_unknown
			}
		}
		None=>SvmVcpu::hvcall_unknown
	}
}