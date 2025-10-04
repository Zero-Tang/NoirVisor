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

use core::{ffi::c_void, slice, sync::atomic::{AtomicBool, Ordering}};

use alloc::{boxed::Box, sync::Arc, vec::Vec};

use log::{error, warn};
use nvcvm::{interface::*, status::Status};

use crate::{cvm_core::{x86::CvmX86Vcpu, *}, xpf_core::{allocator::{kmalloc::KernelAllocator, InternalPageAllocator, SystemPageAllocator}, asm::svm::vmmcall, hv_host::{NOIR_HYPERCALL_CODE_CVM_ALLOC_TLB_TAG, NOIR_HYPERCALL_CODE_CVM_FREE_TLB_TAG}, nvbdk::*, pushlock::PushLock}};
use super::npt::*;

const SVM_CUSTOM_VCPU_LIMIT:usize=256;

struct SvmCustomAsid(u32);

pub(super) static LA57_IN_NPT:AtomicBool=AtomicBool::new(false);

impl SvmCustomAsid
{
	fn new()->Option<Self>
	{
		let mut asid:u32=0;
		match Status(vmmcall(NOIR_HYPERCALL_CODE_CVM_ALLOC_TLB_TAG,&raw mut asid as usize))
		{
			Status::SUCCESS=>Some(Self(asid)),
			_=>None
		}
	}
}

impl Drop for SvmCustomAsid
{
	fn drop(&mut self)
	{
		vmmcall(NOIR_HYPERCALL_CODE_CVM_FREE_TLB_TAG,&raw const self.0 as usize);
	}
}

pub struct SvmCustomVcpu
{
	/// The processor state of the vCPU.
	pub state:CvmX86Vcpu,
	/// The VMCB of the vCPU.
	vmcb:MemoryDescriptor<1,c_void,SystemPageAllocator>,
	apic_backing:Option<MemoryDescriptor<1,c_void,SystemPageAllocator>>,
	current_as_id:u32,
	proc_id:u32,
	apic_id:u32
}

impl SvmCustomVcpu
{
	fn new(vcpu_id:u32)->Result<Self,Status>
	{
		let Some(vmcb)=MemoryDescriptor::alloc_in(SystemPageAllocator) else
		{
			error!("Failed to allocate VMCB for vCPU {vcpu_id}!");
			return Err(Status::INSUFFICIENT_RESOURCES);
		};
		let Some(state)=CvmX86Vcpu::new() else
		{
			error!("Failed to allocate XSAVES state for vcpu {vcpu_id}!");
			return Err(Status::INSUFFICIENT_RESOURCES);
		};
		Ok
		(
			Self
			{
				vmcb,
				state,
				apic_backing:None,
				current_as_id:CVM_MAPPING_ASID_DEFAULT,
				proc_id:u32::MAX,
				apic_id:vcpu_id
			}
		)
	}
}

pub struct SvmCustomVm
{
	vcpus:Arc<PushLock<Vec<Option<Arc<PushLock<Box<SvmCustomVcpu,KernelAllocator>>,KernelAllocator>>,KernelAllocator>>,KernelAllocator>,
	iopm:Option<MemoryDescriptor<3,usize,SystemPageAllocator>>,
	msrpm:Option<MemoryDescriptor<2,usize,SystemPageAllocator>>,
	/// Built-in NPTMs have a fixed number of them, so use a slice to contain them. \
	/// Any vCPU must hold NPT Manager with shared access before running. \
	/// To set mapping, the NPT Manager must be held with exclusive access.
	builtin_nptm:[PushLock<SvmCustomNptManager>;CVM_MAPPING_ASID_RESERVED_START as usize],
	/// Customized NPTMs have a dynamic number of them, so use a vector to contain them. \
	/// To run a vCPU, the assigned NPTM must be held with shared access. \
	/// To set mapping, the NPT Manager must be held with exclusive access.
	custom_as_nptm:Vec<PushLock<SvmCustomNptManager>,KernelAllocator>,
	process_id:u32
}

impl SvmCustomVm
{
	fn new(process_id:u32)->Result<Self,Status>
	{
		let def_nptm=match SvmCustomNptManager::new()
		{
			Some(m)=>m,
			None=>return Err(Status::INSUFFICIENT_RESOURCES)
		};
		let smm_nptm=match SvmCustomNptManager::new()
		{
			Some(m)=>m,
			None=>return Err(Status::INSUFFICIENT_RESOURCES)
		};
		Ok
		(
			Self
			{
				vcpus:Arc::new_in(PushLock::new(Vec::new_in(KernelAllocator)),KernelAllocator),
				iopm:None,
				msrpm:None,
				custom_as_nptm:Vec::new_in(KernelAllocator),
				builtin_nptm:[PushLock::new(def_nptm),PushLock::new(smm_nptm)],
				process_id
			}
		)
	}
}

pub struct SvmCustomHypervisor
{
	vm_list:Vec<Option<Arc<PushLock<Box<SvmCustomVm,KernelAllocator>>,KernelAllocator>>,KernelAllocator>,
	iopm:MemoryDescriptor<3,usize,InternalPageAllocator>,
	msrpm:MemoryDescriptor<2,usize,InternalPageAllocator>,
	l5_npt:bool
}

impl SvmCustomHypervisor
{
	pub fn new(l5_npt:bool)->Option<Self>
	{
		let iopm=MemoryDescriptor::alloc()?;
		let msrpm=MemoryDescriptor::alloc()?;
		Some
		(
			Self
			{
				vm_list:Vec::new_in(KernelAllocator),
				iopm,
				msrpm,
				l5_npt
			}
		)
	}
}

impl CvmHvOps for SvmCustomHypervisor
{
	fn check_cap(&self,_code:u32,_buffer:&mut [u8])->Status
	{
		Status::NOT_IMPLEMENTED
	}

	fn create_vm(&mut self,process_id:u32)->Result<CvmHandle,Status>
	{
		let mut handle:Option<usize>=None;
		// Search for a handle which points to a `None`.
		for (i,vm) in self.vm_list.iter_mut().enumerate()
		{
			if vm.is_none()
			{
				handle=Some(i);
				break;
			}
		}
		// Cannot recycle a released handle. Reserve some spots for handles.
		if handle.is_none()
		{
			// Rust Vec doesn't have `try_push`, so we have to `try_reserve` then `push`.
			if let Err(e)=self.vm_list.try_reserve(1)
			{
				error!("Failed to reserve spot for VM Handle-Table! Reason: {e}");
				return Err(Status::INSUFFICIENT_RESOURCES);
			}
			handle=Some(self.vm_list.len());
		}
		// Because we have previously reserved a spot, this push won't panic.
		let Ok(vm)=Box::try_new_in(SvmCustomVm::new(process_id)?,KernelAllocator) else
		{
			return Err(Status::INSUFFICIENT_RESOURCES);
		};
		let Ok(vm)=Arc::try_new_in(PushLock::new(vm),KernelAllocator) else
		{
			return Err(Status::INSUFFICIENT_RESOURCES);
		};
		self.vm_list.push(Some(vm));
		Ok(CvmHandle(handle.unwrap() as u64))
	}

	fn release_vm(&mut self,vm:CvmHandle)
	{
		match self.vm_list.get_mut(vm.0 as usize)
		{
			Some(vm)=>
			{
				*vm=None;
			}
			None=>warn!("Removing non-existent VM {}...",vm.0)
		}
	}

	fn create_vcpu(&self,vm:CvmHandle,vcpu_id:usize)->Result<(),Status>
	{
		let Some(Some(vm))=self.vm_list.get(vm.0 as usize) else
		{
			error!("The VM Handle is invalid!");
			return Err(Status::INVALID_PARAMETER);
		};
		let vm_lk=vm.read();
		let mut vcpu_list_lk=vm_lk.vcpus.write();
		if vcpu_id>=vcpu_list_lk.len()
		{
			// The vCPU ID is greater than the list. Reserve space.
			let rsvd_count=vcpu_id-vcpu_list_lk.len()+1;
			if let Err(e)=vcpu_list_lk.try_reserve(rsvd_count)
			{
				error!("Failed to reserve vCPU list space! Reason: {e}");
				return Err(Status::INSUFFICIENT_RESOURCES);
			}
			// Fill the new area with None.
			for _ in 0..rsvd_count
			{
				vcpu_list_lk.push(None);
			}
		}
		vcpu_list_lk[vcpu_id]=Some(Arc::new_in(PushLock::new(Box::new_in(SvmCustomVcpu::new(vcpu_id as u32)?,KernelAllocator)),KernelAllocator));
		Ok(())
	}

	fn release_vcpu(&self,vm:CvmHandle,vcpu_id:usize)->Result<(),Status>
	{
		match self.vm_list.get(vm.0 as usize)
		{
			Some(Some(vm))=>
			{
				match vm.read().vcpus.write().get_mut(vcpu_id)
				{
					Some(vcpu_opt)=>
					{
						*vcpu_opt=None;
						Ok(())
					}
					None=>Err(Status::VCPU_NOT_EXIST)
				}
			}
			_=>Err(Status::INVALID_PARAMETER)
		}
	}

	fn run_vcpu(&self,vm:CvmHandle,vcpu_id:usize)->Result<(),Status>
	{
		let vcpu_list=
		{
			match self.vm_list.get(vm.0 as usize)
			{
				Some(Some(vm))=>vm.read().vcpus.clone(),
				_=>return Err(Status::INVALID_PARAMETER)
			}
			// Drop Shared Lock.
		};
		let vcpu=
		{
			// Hold Shared Lock on vCPU-List.
			let vcpu_list_lk=vcpu_list.read();
			match vcpu_list_lk.get(vcpu_id)
			{
				Some(Some(vcpu))=>vcpu.clone(),
				_=>return Err(Status::INVALID_PARAMETER)
			}
			// Drop Shared Lock.
		};
		// Hold Exclusive Lock on vCPU.
		let lk=vcpu.write();
		warn!("Running vCPU {vcpu_id} on CPU {}...",lk.proc_id);
		Ok(())
	}

	fn set_mapping(&self,vm:CvmHandle,mapping:&CvmMapping)->Result<(),Status>
	{
		let Some(Some(vm))=self.vm_list.get(vm.0 as usize) else
		{
			error!("The VM Handle is invalid!");
			return Err(Status::INVALID_PARAMETER);
		};
		let vm_arc=vm.clone();
		let mut vm_lk=vm_arc.write();
		let nptm=match mapping.as_id
		{
			CVM_MAPPING_ASID_DEFAULT..CVM_MAPPING_ASID_RESERVED_START=>&mut vm_lk.builtin_nptm[mapping.as_id as usize],
			CVM_MAPPING_ASID_FREE_START..=u32::MAX=>match vm_lk.custom_as_nptm.get_mut((mapping.as_id-CVM_MAPPING_ASID_FREE_START) as usize)
			{
				Some(nptm)=>nptm,
				None=>return Err(Status::INVALID_PARAMETER)
			}
			_=>return Err(Status::INVALID_PARAMETER)
		};
		nptm.write().set_mapping(mapping.base_gpa,mapping.base_hva,mapping.size,mapping.flags)
	}
}

pub enum SvmCustomNptManager
{
	L4(SvmCustomNptManagerL4),
	L5(SvmCustomNptManagerL5)
}

impl SvmCustomNptManager
{
	fn new()->Option<Self>
	{
		let asid=SvmCustomAsid::new()?;
		if LA57_IN_NPT.load(Ordering::Relaxed)
		{
			let nptm=SvmCustomNptManagerL5::new(asid)?;
			Some(Self::L5(nptm))
		}
		else
		{
			let nptm=SvmCustomNptManagerL4::new(asid)?;
			Some(Self::L4(nptm))
		}
	}

	fn set_mapping(&mut self,gpa:u64,hva:u64,size:u64,flags:CvmMappingFlags)->Result<(),Status>
	{
		match self
		{
			Self::L4(m)=>m.set_mapping(gpa,hva,size,flags),
			Self::L5(m)=>m.set_mapping(gpa,hva,size,flags)
		}
	}
}

pub struct SvmCustomNptManagerL5
{
	pml5e:MemoryDescriptor<1,NptPml5e,SystemPageAllocator>,
	pml4e:Vec<MemoryDescriptor<1,NptPml4e,SystemPageAllocator>,KernelAllocator>,
	pdpte:Vec<MemoryDescriptor<1,NptPdpte,SystemPageAllocator>,KernelAllocator>,
	pde:Vec<MemoryDescriptor<1,NptPde,SystemPageAllocator>,KernelAllocator>,
	pte:Vec<MemoryDescriptor<1,NptPte,SystemPageAllocator>,KernelAllocator>,
	asid:SvmCustomAsid
}

impl SvmCustomNptManagerL5
{
	fn new(asid:SvmCustomAsid)->Option<Self>
	{
		MemoryDescriptor::alloc_in(SystemPageAllocator).map(|md| Self
		{
			pml5e:md,
			pml4e:Vec::new_in(KernelAllocator),
			pdpte:Vec::new_in(KernelAllocator),
			pde:Vec::new_in(KernelAllocator),
			pte:Vec::new_in(KernelAllocator),
			asid
		})
	}

	fn set_mapping(&mut self,_gpa:u64,_hva:u64,_size:u64,_flags:CvmMappingFlags)->Result<(),Status>
	{
		// 5-level paging support will be delayed for quite a while.
		Err(Status::NOT_IMPLEMENTED)
	}
}

pub struct SvmCustomNptManagerL4
{
	pml4e:MemoryDescriptor<1,NptPml4e,SystemPageAllocator>,
	pdpte:Vec<SvmNptPageTableDescriptor<NptPdpte,SystemPageAllocator>,KernelAllocator>,
	pde:Vec<SvmNptPageTableDescriptor<NptPde,SystemPageAllocator>,KernelAllocator>,
	pte:Vec<SvmNptPageTableDescriptor<NptPte,SystemPageAllocator>,KernelAllocator>,
	asid:SvmCustomAsid
}

impl SvmCustomNptManagerL4
{
	fn new(asid:SvmCustomAsid)->Option<Self>
	{
		MemoryDescriptor::alloc_in(SystemPageAllocator).map(|md| Self
		{
			pml4e:md,
			pdpte:Vec::new_in(KernelAllocator),
			pde:Vec::new_in(KernelAllocator),
			pte:Vec::new_in(KernelAllocator),
			asid
		})
	}

	fn create_pml4e_map(&mut self,gpa:u64,pdpte_hpa:u64)
	{
		let pml4e_index=page_entry_index(page_512gb_count(gpa) as usize);
		let s=unsafe{slice::from_raw_parts_mut(self.pml4e.virt,PAGE_TABLE_ENTRIES64)};
		s[pml4e_index]=NptPml4e::from_bits(0);
		s[pml4e_index].set_present(true);
		s[pml4e_index].set_write(true);
		s[pml4e_index].set_user(true);
		s[pml4e_index].set_pdpte_base(page_4kb_count(pdpte_hpa));
	}

	fn create_pdpte_map(&mut self,gpa:u64,pde_hpa:u64)->Result<(),Status>
	{
		// Find the descriptor. We keep the descriptors sorted.
		let i=match self.pdpte.binary_search_by(|pdpte_d| pdpte_d.partial_cmp(&gpa).unwrap())
		{
			Ok(i)=>i,
			Err(i)=>
			{
				// Create and insert the descriptor, and keep it sorted.
				match MemoryDescriptor::alloc_in(SystemPageAllocator)
				{
					Some(md)=>
					{
						let pdpte_d:SvmNptPageTableDescriptor<NptPdpte,SystemPageAllocator>=SvmNptPageTableDescriptor
						{
							table:md,
							gpa_start:page_512gb_base(gpa)
						};
						self.pdpte.insert(i,pdpte_d);
					}
					None=>return Err(Status::INSUFFICIENT_RESOURCES)
				}
				// Set the PML4E.
				self.create_pml4e_map(gpa,self.pdpte[i].table.phys);
				i
			}
		};
		let pdpte_index=page_entry_index(page_1gb_count(gpa) as usize);
		let s=self.pdpte[i].as_slice_mut();
		s[pdpte_index]=NptPdpte::from_bits(0);
		s[pdpte_index].set_present(true);
		s[pdpte_index].set_write(true);
		s[pdpte_index].set_user(true);
		s[pdpte_index].set_pde_base(page_4kb_count(pde_hpa));
		Ok(())
	}

	fn create_pde_map(&mut self,gpa:u64,pte_hpa:u64)->Result<(),Status>
	{
		// Find the descriptor. We keep the descriptors sorted.
		let i=match self.pde.binary_search_by(|pde_d| pde_d.partial_cmp(&gpa).unwrap())
		{
			Ok(i)=>i,
			Err(i)=>
			{
				// Create and insert the descriptor, and keep it sorted.
				match MemoryDescriptor::alloc_in(SystemPageAllocator)
				{
					Some(md)=>
					{
						let pde_d:SvmNptPageTableDescriptor<NptPde,SystemPageAllocator>=SvmNptPageTableDescriptor
						{
							table:md,
							gpa_start:page_1gb_base(gpa)
						};
						self.pde.insert(i,pde_d);
					}
					None=>return Err(Status::INSUFFICIENT_RESOURCES)
				}
				// Set the PDPTE.
				self.create_pdpte_map(gpa,self.pde[i].table.phys)?;
				i
			}
		};
		let pde_index=page_entry_index(page_2mb_count(gpa) as usize);
		let s=self.pde[i].as_slice_mut();
		s[pde_index]=NptPde::from_bits(0);
		s[pde_index].set_present(true);
		s[pde_index].set_write(true);
		s[pde_index].set_pte_base(page_4kb_count(pte_hpa));
		Ok(())
	}

	fn set_pte_map(&mut self,gpa:u64,hpa:u64,flags:CvmMappingFlags)->Result<(),Status>
	{
		// Find the descriptor. We keep the descriptors sorted.
		let i=match self.pte.binary_search_by(|pte_d| pte_d.partial_cmp(&gpa).unwrap())
		{
			Ok(i)=>i,
			Err(i)=>
			{
				// Create and insert the descriptor, and keep it sorted.
				match MemoryDescriptor::alloc_in(SystemPageAllocator)
				{
					Some(md)=>
					{
						let pte_d:SvmNptPageTableDescriptor<NptPte,SystemPageAllocator>=SvmNptPageTableDescriptor
						{
							table:md,
							gpa_start:page_2mb_base(gpa)
						};
						self.pte.insert(i,pte_d);
					}
					None=>return Err(Status::INSUFFICIENT_RESOURCES)
				}
				// Set the PDE.
				self.create_pde_map(gpa,self.pte[i].table.phys)?;
				i
			}
		};
		let pte_index=page_entry_index(page_4kb_count(gpa) as usize);
		let s=self.pte[i].as_slice_mut();
		s[pte_index]=NptPte::from_bits(0);
		s[pte_index].set_present(flags.read());
		s[pte_index].set_write(flags.read());
		s[pte_index].set_nx(!flags.execute());
		s[pte_index].set_page_base(page_4kb_count(hpa));
		Ok(())
	}

	fn set_mapping(&mut self,gpa:u64,hva:u64,size:u64,flags:CvmMappingFlags)->Result<(),Status>
	{
		for i in (0..size).step_by(PAGE_4KB_SIZE)
		{
			self.set_pte_map(gpa+i,hva+i,flags)?;
		}
		Ok(())
	}
}