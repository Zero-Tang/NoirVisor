/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the customizable VM engine for AMD-V.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, ptr::null_mut, slice, sync::atomic::{AtomicBool, Ordering}};
use alloc::{sync::{Arc, Weak}, vec::Vec};

use log::*;
use nvcvm::{interface::*, status::Status};
use spin::{Mutex, RwLock};

use crate::{HVM, cvm_core::x86::{CvmHvOps, CvmX86Vcpu}, disasm::emulator::Instruction, svm_core::{SvmHypervisor, VmcbOps, amd64::msr::SECURITY_EXCEPTION_FAULT, vmcb::*}, xpf_core::{allocator::InternalPageAllocator, asm::crdr::*, nvbdk::*, x86::{crdr::{Cr0, Cr4, Dr6, Dr7}, interrupts::MACHINE_CHECK_ABORT, msr::Efer, rflags::Rflags}}};
use super::{npt::*, SvmVcpu};

const SVM_CUSTOM_VCPU_LIMIT:usize=256;

pub(super) static LA57_IN_NPT:AtomicBool=AtomicBool::new(false);

pub struct SvmCvHostVcpuState
{
	pub gpr:GprState,
	pub dr0:u64,
	pub dr1:u64,
	pub dr2:u64,
	pub dr3:u64,
	pub from_vcpu:Option<(CvmHandle,u32)>
}

impl Default for SvmCvHostVcpuState
{
	fn default()->Self
	{
		unsafe
		{
			core::mem::zeroed()
		}
	}
}

impl SvmVcpu
{
	pub(super) fn switch_world_to_guest(&mut self,cvcpu:&mut SvmCustomVcpu)
	{
		// Save states not defined in VMCB.
		self.cv_host_save.gpr=self.get_stack_top().gpr_state;
		self.cv_host_save.dr0=read_dr0();
		self.cv_host_save.dr1=read_dr1();
		self.cv_host_save.dr2=read_dr2();
		self.cv_host_save.dr3=read_dr3();
		// Load states not defined in VMCB.
		self.get_stack_top_mut().gpr_state=cvcpu.state.gpr;
		write_dr0(cvcpu.state.drs.dr0);
		write_dr1(cvcpu.state.drs.dr1);
		write_dr2(cvcpu.state.drs.dr2);
		write_dr3(cvcpu.state.drs.dr3);
		// XSAVE State's save/load will be defered to assembly part.
		// Synchronize the state from VPCB.
		cvcpu.state.sync_from_vpcb(unsafe{&mut *cvcpu.vpcb});
		// Synchronize the state into VMCB.
		cvcpu.sync_to_vmcb();
		// Switch VMCB.
		self.get_stack_top_mut().guest_vmcb_pa=cvcpu.vmcb.phys;
		self.get_stack_top_mut().custom_vcpu=(&raw mut *cvcpu).cast();
		// Prevent the guest from blocking maskable interrupts.
		unsafe
		{
			core::arch::asm!("sti");
		}
	}

	pub(super) fn switch_world_to_host(&mut self,cvcpu:&mut SvmCustomVcpu)
	{
		// Save states not defined in VMCB.
		cvcpu.state.gpr=self.get_stack_top().gpr_state;
		cvcpu.state.drs.dr0=read_dr0();
		cvcpu.state.drs.dr1=read_dr1();
		cvcpu.state.drs.dr2=read_dr2();
		cvcpu.state.drs.dr3=read_dr3();
		// Load states not defined in VMCB.
		self.get_stack_top_mut().gpr_state=self.cv_host_save.gpr;
		write_dr0(self.cv_host_save.dr0);
		write_dr1(self.cv_host_save.dr1);
		write_dr2(self.cv_host_save.dr2);
		write_dr3(self.cv_host_save.dr3);
		// Synchronize the state from VMCB.
		cvcpu.sync_from_vmcb();
		// Synchronize the state into VPCB.
		cvcpu.state.sync_to_vpcb(unsafe{&mut *cvcpu.vpcb});
		// Switch VMCB.
		self.get_stack_top_mut().guest_vmcb_pa=self.vmcb.phys;
		self.get_stack_top_mut().custom_vcpu=null_mut();
		// We don't need to prevent guest from blocking maskable interrupts anymore.
		unsafe
		{
			core::arch::asm!("cli");
		}
	}
}

pub struct SvmCustomVcpu
{
	/// The processor state of the vCPU.
	pub state:CvmX86Vcpu,
	pub shared:Weak<RwLock<SvmCustomVmSharedState>>,
	pub(super) decoded_instruction:Instruction,
	/// The VMCB of the vCPU.
	vmcb:MemoryDescriptor<1,c_void>,
	/// The VPCB of the vCPU.
	pub(super) vpcb:*mut Vpcb,
	apic_backing:Option<MemoryDescriptor<1,c_void>>,
	current_as_id:u32,
	proc_id:u32,
	apic_id:u32
}

unsafe impl Send for SvmCustomVcpu {}
unsafe impl Sync for SvmCustomVcpu {}

impl SvmCustomVcpu
{
	fn new(vcpu_id:u32,shared:&Arc<RwLock<SvmCustomVmSharedState>>,vpcb:*mut Vpcb)->Result<Self,Status>
	{
		let Some(vmcb)=MemoryDescriptor::alloc() else
		{
			error!("Failed to allocate VMCB for vCPU {vcpu_id}!");
			return Err(Status::INSUFFICIENT_RESOURCES);
		};
		let state=CvmX86Vcpu::new()?;
		unsafe
		{
			memset(vmcb.virt,0,PAGE_SIZE);
			memset(vpcb.cast(),0,PAGE_SIZE);
		}
		let mut vcpu=Self
		{
			vmcb,
			vpcb,
			state,
			shared:Arc::downgrade(shared),
			decoded_instruction:Instruction::new([0;15]),
			apic_backing:None,
			current_as_id:CVM_MAPPING_ASID_DEFAULT,
			proc_id:u32::MAX,
			apic_id:vcpu_id
		};
		vcpu.state.sync_to_vpcb(unsafe{&mut *vpcb});
		vcpu.init_vmcb();
		Ok(vcpu)
	}

	const INITIAL_IV1:InterceptVector1=InterceptVector1::new()
		.with_phys_intr(true)
		.with_nmi(true)
		.with_smi(true)
		.with_cpuid(true)
		.with_rsm(true)
		.with_hlt(true)
		.with_invlpga(true)
		.with_io(true)
		.with_msr(true)
		.with_shutdown(true);
	const INITIAL_IV2:InterceptVector2=InterceptVector2::new()
		.with_vmrun(true)
		.with_vmmcall(true)
		.with_vmload(true)
		.with_vmsave(true)
		.with_stgi(true)
		.with_clgi(true)
		.with_skinit(true)
		.with_xsetbv(true);

	fn init_vmcb(&mut self)
	{
		let shared=self.shared.upgrade().unwrap();
		let shared_lk=shared.read();
		// Intercept CR4 accesses as we need to shadow the CR4.MCE bit.
		self.write_intercept_cr_readv(1<<4);
		self.write_intercept_cr_writev(1<<4);
		// #MC and #SX must be intercepted for scheduler's sake.
		self.write_intercept_exception((1<<MACHINE_CHECK_ABORT)|(1<<SECURITY_EXCEPTION_FAULT));
		// Intercept sensitive instructions.
		self.write_intercept_vector1(Self::INITIAL_IV1);
		self.write_intercept_vector2(Self::INITIAL_IV2);
		// Intercept I/O and MSRs.
		self.write_iopm(HVM.get().unwrap().iopm());
		self.write_msrpm(HVM.get().unwrap().msrpm());
		// Initialize ASID.
		self.write_asid(shared_lk.builtin_nptm[CVM_MAPPING_ASID_DEFAULT as usize].asid());
		// Initialize Local APIC Controls.
		self.write_avic_control(AvicControl::new().with_v_intr_mask(true));
		// Initialize Nested Paging.
		self.write_npt_control(NptControl::new().with_enable_npt(true));
		self.write_ncr3(shared_lk.builtin_nptm[CVM_MAPPING_ASID_DEFAULT as usize].ncr3());
		info!("CVM VMCB: 0x{:X}, NPT: 0x{:X}",self.vmcb.phys,self.read_ncr3());
	}

	fn sync_from_vmcb(&mut self)
	{
		self.state.gpr.rax=self.read_rax();
		self.state.gpr.rsp=self.read_rsp();
		self.state.rflags=self.read_rflags().into_bits();
		self.state.rip=self.read_rip();

		self.state.seg.cs=self.read_cs().into();
		self.state.seg.ds=self.read_ds().into();
		self.state.seg.es=self.read_es().into();
		self.state.seg.fs=self.read_fs().into();
		self.state.seg.gs=self.read_gs().into();
		self.state.seg.ss=self.read_ss().into();
		self.state.seg.tr=self.read_tr().into();
		self.state.seg.ldtr=self.read_ldtr().into();
		self.state.seg.idtr=self.read_idtr().into();
		self.state.seg.gdtr=self.read_gdtr().into();

		self.state.crs.cr0=self.read_cr0().into_bits();
		self.state.crs.cr2=self.read_cr2();
		self.state.crs.cr3=self.read_cr3();
		self.state.crs.cr4=self.read_cr4().into_bits();
		self.state.crs.cr8=self.read_avic_control().v_tpr() as u64;

		self.state.drs.dr6=self.read_dr6().into_bits();
		self.state.drs.dr7=self.read_dr7().into_bits();

		self.state.msrs.efer=self.read_efer().into_bits();
		self.state.msrs.star=self.read_star();
		self.state.msrs.lstar=self.read_lstar();
		self.state.msrs.cstar=self.read_cstar();
		self.state.msrs.sfmask=self.read_sfmask();
		self.state.msrs.gsswap=self.read_kgsbase();
		self.state.msrs.sysenter_cs=self.read_sysenter_cs();
		self.state.msrs.sysenter_esp=self.read_sysenter_esp();
		self.state.msrs.sysenter_eip=self.read_sysenter_eip();
	}

	fn sync_to_vmcb(&mut self)
	{
		self.write_rax(self.state.gpr.rax);
		self.write_rsp(self.state.gpr.rsp);
		self.write_rflags(Rflags::from_bits(self.state.rflags));
		self.write_rip(self.state.rip);

		self.write_cs(SvmSegmentRegister::from(self.state.seg.cs));
		self.write_ds(SvmSegmentRegister::from(self.state.seg.ds));
		self.write_es(SvmSegmentRegister::from(self.state.seg.es));
		self.write_fs(SvmSegmentRegister::from(self.state.seg.fs));
		self.write_gs(SvmSegmentRegister::from(self.state.seg.gs));
		self.write_ss(SvmSegmentRegister::from(self.state.seg.ss));
		self.write_tr(SvmSegmentRegister::from(self.state.seg.tr));
		self.write_ldtr(SvmSegmentRegister::from(self.state.seg.ldtr));
		self.write_idtr(SvmSegmentRegister::from(self.state.seg.idtr));
		self.write_gdtr(SvmSegmentRegister::from(self.state.seg.gdtr));

		self.write_cr0(Cr0::from_bits(self.state.crs.cr0));
		self.write_cr2(self.state.crs.cr2);
		self.write_cr3(self.state.crs.cr3);
		self.write_cr4(Cr4::from_bits(self.state.crs.cr4));
		let tpr=self.state.crs.cr8 as u8;
		self.ref_avic_control_mut().set_v_tpr(tpr);

		self.write_dr6(Dr6::from_bits(self.state.drs.dr6));
		self.write_dr7(Dr7::from_bits(self.state.drs.dr7));

		self.write_efer(Efer::from_bits(self.state.msrs.efer).with_svme(true));
		self.write_star(self.state.msrs.star);
		self.write_lstar(self.state.msrs.lstar);
		self.write_cstar(self.state.msrs.cstar);
		self.write_sfmask(self.state.msrs.sfmask);
		self.write_kgsbase(self.state.msrs.gsswap);
		self.write_sysenter_cs(self.state.msrs.sysenter_cs);
		self.write_sysenter_esp(self.state.msrs.sysenter_esp);
		self.write_sysenter_eip(self.state.msrs.sysenter_eip);

		// Force a full flush.
		self.write_clean_field(VmcbCleanField::NONE_CACHED);
		self.write_tlb_control(TLB_CONTROL_FLUSH_GUEST_TLB);
	}

	pub(super) fn translate(&self,gpa:u64)->Option<(u64,bool,bool)>
	{
		let shared=self.shared.upgrade().unwrap();
		let shared_lk=shared.read();
		let nptm=shared_lk.get_nptm(self.current_as_id)?;
		nptm.translate(gpa)
	}
}

impl VmcbOps for SvmCustomVcpu
{
	fn get_vmcb(&self)->*mut c_void
	{
		self.vmcb.virt
	}
}

pub struct SvmCustomVmSharedState
{
	/// Built-in NPTMs have a fixed number of them, so use a slice to contain them. \
	/// Any vCPU must hold NPT Manager with shared access before running. \
	/// To set mapping, the NPT Manager must be held with exclusive access.
	pub builtin_nptm:[SvmCustomNptManager;CVM_MAPPING_ASID_RESERVED_START as usize],
	/// Customized NPTMs have a dynamic number of them, so use a vector to contain them. \
	/// To run a vCPU, the assigned NPTM must be held with shared access. \
	/// To set mapping, the NPT Manager must be held with exclusive access.
	pub custom_as_nptm:Vec<SvmCustomNptManager>,
}

impl SvmCustomVmSharedState
{
	pub(super) fn get_nptm(&self,as_id:u32)->Option<&SvmCustomNptManager>
	{
		match as_id
		{
			CVM_MAPPING_ASID_DEFAULT..CVM_MAPPING_ASID_RESERVED_START=>Some(&self.builtin_nptm[as_id as usize]),
			CVM_MAPPING_ASID_FREE_START..=u32::MAX=>self.custom_as_nptm.get((as_id-CVM_MAPPING_ASID_FREE_START) as usize),
			_=>None
		}
	}

	pub(super) fn get_nptm_mut(&mut self,as_id:u32)->Option<&mut SvmCustomNptManager>
	{
		match as_id
		{
			CVM_MAPPING_ASID_DEFAULT..CVM_MAPPING_ASID_RESERVED_START=>Some(&mut self.builtin_nptm[as_id as usize]),
			CVM_MAPPING_ASID_FREE_START..=u32::MAX=>self.custom_as_nptm.get_mut((as_id-CVM_MAPPING_ASID_FREE_START) as usize),
			_=>None
		}
	}
}

unsafe impl Send for SvmCustomVmSharedState {}
unsafe impl Sync for SvmCustomVmSharedState {}

type SvmCustomVcpuLockedList=Arc<RwLock<Vec<Option<Arc<Mutex<SvmCustomVcpu>>>>>>;

pub struct SvmCustomVm
{
	pub vcpus:SvmCustomVcpuLockedList,
	pub shared_state:Arc<RwLock<SvmCustomVmSharedState>>
}

unsafe impl Send for SvmCustomVm {}
unsafe impl Sync for SvmCustomVm {}

impl SvmCustomVm
{
	fn new(asid:u32)->Result<Self,Status>
	{
		let def_nptm=match SvmCustomNptManager::new(asid)
		{
			Some(m)=>m,
			None=>return Err(Status::INSUFFICIENT_RESOURCES)
		};
		let smm_nptm=match SvmCustomNptManager::new(asid)
		{
			Some(m)=>m,
			None=>return Err(Status::INSUFFICIENT_RESOURCES)
		};
		let shared_arc:Arc<RwLock<SvmCustomVmSharedState>>=Arc::try_new(RwLock::new(SvmCustomVmSharedState
		{
			builtin_nptm:[def_nptm,smm_nptm],
			custom_as_nptm:Vec::new()
		}))?;
		Ok
		(
			Self
			{
				vcpus:Arc::new(RwLock::new(Vec::new())),
				shared_state:shared_arc
			}
		)
	}
}

impl CvmHvOps for SvmHypervisor
{
	fn create_vm(&self)->Result<CvmHandle,Status>
	{
		let mut handle:Option<usize>=None;
		// Search for a handle which points to a `None`.
		let mut vm_list_lk=self.vm_list.write();
		for (i,vm) in vm_list_lk.iter_mut().enumerate()
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
			if let Err(e)=vm_list_lk.try_reserve(1)
			{
				error!("Failed to reserve spot for VM Handle-Table! Reason: {e}");
				return Err(Status::INSUFFICIENT_RESOURCES);
			}
			handle=Some(vm_list_lk.len());
		}
		// Because we have previously reserved a spot, this push won't panic.
		vm_list_lk.push(Some(Arc::new(RwLock::new(SvmCustomVm::new(2)?))));
		Ok(CvmHandle(handle.unwrap() as u32))
	}

	fn delete_vm(&self,vm:CvmHandle)->Result<(),Status>
	{
		match self.vm_list.write().get_mut(vm.0 as usize)
		{
			Some(vm)=>
			{
				*vm=None;
				Ok(())
			}
			None=>Err(Status::INVALID_PARAMETER)
		}
	}

	fn create_vcpu(&self,vm:CvmHandle,vcpu_id:u32,vpcb_hpa:u64)->Result<(),Status>
	{
		let vcpu_id=vcpu_id as usize;
		let vm_list_lk=self.vm_list.read();
		let Some(Some(vm))=vm_list_lk.get(vm.0 as usize) else
		{
			error!("The VM Handle is invalid!");
			return Err(Status::INVALID_PARAMETER);
		};
		let vm_lk=vm.read();
		let mut vcpu_list_lk=vm_lk.vcpus.write();
		if vcpu_id>=vcpu_list_lk.len()
		{
			// The vCPU ID is greater than the list. Reserve space.
			let rsvd_count=vcpu_list_lk.len()+1;
			vcpu_list_lk.try_reserve(rsvd_count)?;
			// Fill the new area with None.
			for _ in 0..rsvd_count
			{
				vcpu_list_lk.push(None);
			}
		}
		info!("Using VPCB 0x{vpcb_hpa:X} for vCPU {vcpu_id}...");
		vcpu_list_lk[vcpu_id]=Some(Arc::new(Mutex::new(SvmCustomVcpu::new(vcpu_id as u32,&vm_lk.shared_state,vpcb_hpa as *mut Vpcb)?)));
		Ok(())
	}

	fn delete_vcpu(&self,vm:CvmHandle,vcpu_id:u32)->Result<(),Status>
	{
		let vcpu_id=vcpu_id as usize;
		match self.vm_list.read().get(vm.0 as usize)
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

	fn set_mapping(&self,vm:CvmHandle,info:&CvmMapping,hpa_list:&[u64])->Result<(),Status>
	{
		if page_mult(hpa_list.len())!=info.size as usize
		{
			error!("The mapping size (0x{:X}) does not equal to HPA-list length (0x{:X})!",hpa_list.len(),info.size);
			return Err(Status::INVALID_PARAMETER);
		}
		let vm_list_lk=self.vm_list.read();
		let Some(Some(vm))=vm_list_lk.get(vm.0 as usize) else
		{
			error!("The VM Handle is invalid!");
			return Err(Status::INVALID_PARAMETER);
		};
		let vm_arc=vm.clone();
		let vm_lk=vm_arc.write();
		let shared_arc=vm_lk.shared_state.clone();
		let mut shared_lk=shared_arc.write();
		let nptm=match info.as_id
		{
			CVM_MAPPING_ASID_DEFAULT..CVM_MAPPING_ASID_RESERVED_START=>&mut shared_lk.builtin_nptm[info.as_id as usize],
			CVM_MAPPING_ASID_FREE_START..=u32::MAX=>match shared_lk.custom_as_nptm.get_mut((info.as_id-CVM_MAPPING_ASID_FREE_START) as usize)
			{
				Some(nptm)=>nptm,
				None=>return Err(Status::INVALID_PARAMETER)
			}
			_=>return Err(Status::INVALID_PARAMETER)
		};
		nptm.set_mapping(info.base_gpa,hpa_list,info.flags)
	}

	fn iopm(&self)->u64
	{
		self.cvm_iopm.phys
	}

	fn msrpm(&self)->u64
	{
		self.cvm_msrpm.phys
	}
}

pub type SvmLockedVmList=Arc<RwLock<Vec<Option<Arc<RwLock<SvmCustomVm>>>>>>;

pub struct SvmCustomHypervisor
{
	vm_list:SvmLockedVmList,
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
		unsafe
		{
			memset(msrpm.virt as *mut c_void,0xFF,page_4kb_mult(2));
			memset(iopm.virt as *mut c_void,0xFF,page_4kb_mult(2)+1);
		}
		Some
		(
			Self
			{
				vm_list:Arc::new(RwLock::new(Vec::new())),
				iopm,
				msrpm,
				l5_npt
			}
		)
	}
}

impl SvmCustomHypervisor
{
	fn check_cap(&self,_code:u32,_buffer:&mut [u32])->Status
	{
		Status::NOT_IMPLEMENTED
	}

	fn set_mapping(&self,vm:CvmHandle,mapping:&CvmMapping,hpa:&[u64])->Result<(),Status>
	{
		if hpa.len()!=mapping.size as usize
		{
			error!("The mapping size does not equal to HPA-list length!");
			return Err(Status::INVALID_PARAMETER);
		}
		let vm_list_lk=self.vm_list.read();
		let Some(Some(vm))=vm_list_lk.get(vm.0 as usize) else
		{
			error!("The VM Handle is invalid!");
			return Err(Status::INVALID_PARAMETER);
		};
		let vm_arc=vm.clone();
		let vm_lk=vm_arc.write();
		let shared_arc=vm_lk.shared_state.clone();
		let mut shared_lk=shared_arc.write();
		let nptm=match shared_lk.get_nptm_mut(mapping.as_id)
		{
			Some(nptm)=>nptm,
			None=>return Err(Status::INVALID_PARAMETER)
		};
		nptm.set_mapping(mapping.base_gpa,hpa,mapping.flags)
	}
}

pub enum SvmCustomNptManager
{
	L4(SvmCustomNptManagerL4),
	L5(SvmCustomNptManagerL5)
}

impl SvmCustomNptManager
{
	fn new(asid:u32)->Option<Self>
	{
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

	fn asid(&self)->u32
	{
		match self
		{
			Self::L4(nptm)=>nptm.asid,
			Self::L5(nptm)=>nptm.asid
		}
	}

	fn ncr3(&self)->u64
	{
		match self
		{
			Self::L4(nptm)=>nptm.pml4e.phys,
			Self::L5(nptm)=>nptm.pml5e.phys
		}
	}

	fn set_mapping(&mut self,gpa:u64,hpa:&[u64],flags:CvmMappingFlags)->Result<(),Status>
	{
		match self
		{
			Self::L4(m)=>m.set_mapping(gpa,hpa,flags),
			Self::L5(m)=>m.set_mapping(gpa,hpa,flags)
		}
	}

	fn translate(&self,gpa:u64)->Option<(u64,bool,bool)>
	{
		match self
		{
			Self::L4(m)=>translate(m.pml4e.phys,gpa,3,true,true),
			Self::L5(m)=>translate(m.pml5e.phys,gpa,4,true,true)
		}
	}
}

pub struct SvmCustomNptManagerL5
{
	pml5e:MemoryDescriptor<1,NptPml5e>,
	pml4e:Vec<MemoryDescriptor<1,NptPml4e>>,
	pdpte:Vec<MemoryDescriptor<1,NptPdpte>>,
	pde:Vec<MemoryDescriptor<1,NptPde>>,
	pte:Vec<MemoryDescriptor<1,NptPte>>,
	asid:u32
}

impl SvmCustomNptManagerL5
{
	fn new(asid:u32)->Option<Self>
	{
		MemoryDescriptor::alloc().map(|md| Self
		{
			pml5e:md,
			pml4e:Vec::new(),
			pdpte:Vec::new(),
			pde:Vec::new(),
			pte:Vec::new(),
			asid
		})
	}

	fn set_mapping(&mut self,_gpa:u64,_hpa:&[u64],_flags:CvmMappingFlags)->Result<(),Status>
	{
		// 5-level paging support will be delayed for quite a while.
		Err(Status::NOT_IMPLEMENTED)
	}
}

pub struct SvmCustomNptManagerL4
{
	pml4e:MemoryDescriptor<1,NptPml4e>,
	pdpte:Vec<SvmNptPageTableDescriptor<NptPdpte>>,
	pde:Vec<SvmNptPageTableDescriptor<NptPde>>,
	pte:Vec<SvmNptPageTableDescriptor<NptPte>>,
	asid:u32
}

impl SvmCustomNptManagerL4
{
	fn new(asid:u32)->Option<Self>
	{
		MemoryDescriptor::alloc().map(|md| Self
		{
			pml4e:md,
			pdpte:Vec::new(),
			pde:Vec::new(),
			pte:Vec::new(),
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
				match MemoryDescriptor::alloc()
				{
					Some(md)=>
					{
						let pdpte_d:SvmNptPageTableDescriptor<NptPdpte>=SvmNptPageTableDescriptor
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
				match MemoryDescriptor::alloc()
				{
					Some(md)=>
					{
						let pde_d:SvmNptPageTableDescriptor<NptPde>=SvmNptPageTableDescriptor
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
		s[pde_index].set_user(true);
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
				match MemoryDescriptor::alloc()
				{
					Some(md)=>
					{
						let pte_d:SvmNptPageTableDescriptor<NptPte>=SvmNptPageTableDescriptor
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
		s[pte_index]=NptPte::from_bits(0).with_user(true);
		s[pte_index].set_present(flags.read());
		s[pte_index].set_write(flags.read());
		s[pte_index].set_nx(!flags.execute());
		s[pte_index].set_page_base(page_4kb_count(hpa));
		Ok(())
	}

	fn set_mapping(&mut self,gpa:u64,hpa:&[u64],flags:CvmMappingFlags)->Result<(),Status>
	{
		for (i,&pa) in hpa.iter().enumerate()
		{
			info!("Mapping page {i} for GPA-Base: 0x{gpa:X}, HPA=0x{pa:X}");
			self.set_pte_map(gpa+i as u64,pa,flags)?;
		}
		Ok(())
	}
}

fn translate(pxl_base:u64,gpa:u64,level:u8,prev_w:bool,prev_x:bool)->Option<(u64,bool,bool)>
{
	let array=unsafe{&mut *(pxl_base as *mut [NptPmlxe;PAGE_TABLE_ENTRIES64])};
	let index=page_index_from_level(gpa,level) as usize;
	let entry=array[index];
	if !entry.present()
	{
		return None;
	}
	if level==0 || entry.page_size()
	{
		let base=phys_page_base_from_level(entry.into_bits(),level);
		let offset=page_offset_from_level(gpa,level);
		Some((base+offset,prev_w&entry.write(),prev_x&!entry.nx()))
	}
	else
	{
		// This entry has its next level.
		let next_base=page_base(entry.into_bits());
		translate(next_base,gpa,level-1,prev_w&entry.write(),prev_x&!entry.nx())
	}
}