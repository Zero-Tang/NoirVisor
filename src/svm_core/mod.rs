/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the AMD-V driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{arch::{global_asm, x86_64::_xgetbv}, ffi::c_void, ptr::*, sync::atomic::{AtomicBool, AtomicU8, Ordering}};
use alloc::{sync::Arc, vec, vec::Vec};

use bitfield_struct::bitfield;
use spin::RwLock;
use static_collections::bitmap::{BitmapSlice, RefBitmap};
use log::*;

use npt::SvmNptManager;
use crate::{disasm::emulator::Instruction, drv_core::iommu::{IommuOps, create_iommu}, svm_core::decode::{DECODE_ASSIST_SUPPORT, NEXT_RIP_SAVING_SUPPORT}, xpf_core::rmt::ReverseMappingTableRoot, *};
use xpf_core::{allocator::InternalPageAllocator, asm::{crdr::*, msr::*, seg::*, svm::*}, hv_host::x86::*, ioflt::IoAddressSpace, nvbdk::*, x86::{crdr::Cr4, interrupts::*, msr::*, xstate::BoxedXState}};
use custom::{SvmCustomVm, SvmCvHostVcpuState};
use decode::SoftwareDecodeAssistOps;
use mshv_core::{MshvVcpuContext,MshvVcpuOps};
use amd64::{cpuid::*,msr::*};
use vmcb::*;

pub mod amd64;
// These modules aren't supposed to be public, but we need to permit dead_code for future utility.
#[allow(dead_code)] mod vmcb;
#[allow(dead_code)] mod decode;
#[allow(dead_code)] mod exit;
mod hvcall;
#[allow(dead_code)] mod npt;
#[allow(dead_code)] pub mod custom;
pub mod cvexit;

#[repr(C,align(16))] pub struct SvmStackTop
{
	pub arg_home:[u64;4],
	pub gpr_state:GprState,
	pub guest_frame:InterruptStackFrameWithErrorCode,
	pub xsave_state:*mut c_void,
	pub guest_vmcb_pa:u64,
	pub host_vmcb_pa:u64,
	pub vcpu:*mut SvmVcpu,
	pub custom_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub nested_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub proc_id:u32,
	pub reserved:u32,
	pub guest_xcr0:u64,
	pub host_xcr0:u64
}

pub struct SvmNestedVcpu
{
	pub ignne:u64,
	pub hsave_pa:u64,
	pub vmcr:u64,
	pub svm_key:u64
}

#[bitfield(u64)] pub struct SvmVcpuFlags
{
	pub svme:bool,
	pub mce:bool,
	#[bits(62)] rsvd:u64
}

pub struct SvmVcpu
{
	pub vmcb:MemoryDescriptor<1,c_void>,
	pub hsave:MemoryDescriptor<1,c_void>,
	pub hvmcb:MemoryDescriptor<1,c_void>,
	pub hv_stack:MemoryDescriptor<HYPERVISOR_STACK_PAGE_COUNT,c_void>,
	pub hypervisor:*mut c_void,
	pub ist:[MemoryDescriptor<HYPERVISOR_STACK_PAGE_COUNT,c_void>;8],
	// Used for preserving XSAVE state.
	pub host_xsave:BoxedXState,
	pub vcpu_id:u32,
	pub apic_id:u8,
	pub x2apic_id:u32,
	pub cpuid_fms:u32,
	pub host_cpu:HostProcessor,
	pub mshv_ctxt:MshvVcpuContext,
	pub nested_hvm:SvmNestedVcpu,
	pub under_hvm:bool,
	pub wait_for_sipi:AtomicBool,
	pub sipi_vector:AtomicU8,
	pub flags:SvmVcpuFlags,
	pub decoded_instruction:Instruction,
	// Features supported by the processors.
	pub svm_feats:SvmFeatureIdentifier,
	// This context handles exceptions.
	pub gs_context:PerCpuGsException,
	pub cv_host_save:SvmCvHostVcpuState
}

static SVM_MSHV_VCPU_OPS:MshvVcpuOps=MshvVcpuOps
{
	inform_apic_icr:SvmVcpu::inform_apic_icr,
	in_long_mode:SvmVcpu::in_long_mode,
	get_vp_index:SvmVcpu::get_vp_index
};

impl SvmVcpu
{
	unsafe fn inform_apic_icr(_vcpu:*mut c_void,_icr_lo:u32,_icr_hi:u32)
	{
		panic!("APIC-ICR ops is unimplemented!");
	}

	fn in_long_mode(vcpu:*const c_void)->bool
	{
		let vcpu:&Self=unsafe{&*vcpu.cast()};
		vcpu.get_current_bitness()==64
	}

	fn get_vp_index(vcpu:*const c_void)->u32
	{
		let vcpu:&Self=unsafe{&*vcpu.cast()};
		vcpu.vcpu_id
	}

	fn new()->Self
	{
		Self
		{
			vmcb:MemoryDescriptor::null(),
			hsave:MemoryDescriptor::null(),
			hvmcb:MemoryDescriptor::null(),
			hv_stack:MemoryDescriptor::null(),
			hypervisor:null_mut(),
			ist:[const{MemoryDescriptor::null()};8],
			host_xsave:BoxedXState::null(),
			vcpu_id:0,
			apic_id:0,
			x2apic_id:0,
			cpuid_fms:0,
			host_cpu:HostProcessor::default(),
			mshv_ctxt:MshvVcpuContext::new(&SVM_MSHV_VCPU_OPS),
			nested_hvm:SvmNestedVcpu
			{
				ignne:0,
				hsave_pa:0,
				vmcr:0,
				svm_key:0
			},
			under_hvm:false,
			wait_for_sipi:AtomicBool::new(false),
			sipi_vector:AtomicU8::new(0),
			flags:SvmVcpuFlags::new(),
			decoded_instruction:Instruction::new([0;15]),
			svm_feats:SvmFeatureIdentifier::new(),
			gs_context:PerCpuGsException::default(),
			cv_host_save:SvmCvHostVcpuState::default()
		}
	}
}

unsafe extern "win64"
{
	#[allow(improper_ctypes)]
	fn nvc_svm_subvert_processor_a(stack:*mut SvmStackTop);
	fn nvc_svm_guest_start();
	pub fn nvc_svm_return(stack:*const GprState)->!;
}

global_asm!(include_str!("svm_hv.s"));

/// # Safety
/// This function is unsafe because it's called from assembly. \
/// **DO NOT CALL THIS FUNCTION FROM RUST!**
#[unsafe(no_mangle)] unsafe extern "win64" fn nvc_svm_subvert_processor_i(vcpu:*mut SvmVcpu,gsp:u64,gssp:u64)->u64
{
	unsafe
	{
		(*vcpu).subvert_i(gsp,gssp)
	}
}

impl SvmVcpu
{
	#[inline(always)] pub fn get_stack_top(&self)->&SvmStackTop
	{
		unsafe
		{
			&*self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<SvmStackTop>()).cast()
		}
	}

	#[inline(always)] pub fn get_stack_top_mut(&mut self)->&mut SvmStackTop
	{
		unsafe
		{
			&mut *self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<SvmStackTop>()).cast()
		}
	}

	#[inline(always)] unsafe fn host_vmwrite<T:Sized+Copy>(&mut self,offset:usize,value:T)
	{
		unsafe
		{
			*self.hvmcb.virt.byte_add(offset).cast()=value;
		}
	}
	
	fn subvert_i(&mut self,gsp:u64,gssp:u64)->u64
	{
		unsafe
		{
			let stack:&mut SvmStackTop=&mut *self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<SvmStackTop>()).cast();
			self.mshv_ctxt.root=(&raw mut *self).cast();
			// Setup supported features.
			self.svm_feats=SvmFeatureIdentifier::cpuid();
			let hv=self.hypervisor as *mut SvmHypervisor;
			let state=ProcessorState::new();
			// Setup Control Area.
			self.vmwrite(INTERCEPT_READ_CR,1u16<<4);
			self.vmwrite(INTERCEPT_WRITE_CR,1u16<<4);
			self.vmwrite(INTERCEPT_EXCEPTIONS,1u32<<SECURITY_EXCEPTION_FAULT);
			let mut iv1=InterceptVector1::from_bits(0);
			iv1.set_cpuid(true);
			iv1.set_invlpga(true);
			iv1.set_io(true);
			iv1.set_msr(true);
			iv1.set_shutdown(true);
			self.vmwrite(INTERCEPT_VECTOR1,iv1.into_bits());
			let mut iv2=InterceptVector2::from_bits(0);
			iv2.set_vmrun(true);
			iv2.set_vmmcall(true);
			iv2.set_vmload(true);
			iv2.set_vmsave(true);
			iv2.set_stgi(true);
			iv2.set_clgi(true);
			iv2.set_skinit(true);
			self.vmwrite(INTERCEPT_VECTOR2,iv2.into_bits());
			// Setup Host State.
			let mut ist:[*mut c_void;8]=[null_mut();8];
			ist[1]=self.ist[1].virt.byte_add(HYPERVISOR_STACK_SIZE);
			HostProcessor::build(&mut self.host_cpu,&ist);
			vmsave(self.hvmcb.phys);
			let idtr=(*hv).host.idt.get_reg();
			write_idtr(&raw const idtr);
			let gdtr=self.host_cpu.gdt.get_reg();
			write_gdtr(&raw const gdtr);
			write_tr(self.host_cpu.tr_sel);
			write_cr3((*hv).host.paging.cr3.phys);
			// The FXSR and XSAVE features must be required for CVM features.
			// #MC should be enabled to expect Machine-Check-induced system abort.
			let mut cr4=Cr4::from_bits(state.cr4 as u64);
			cr4.set_osfxsr(true);
			cr4.set_osxmmexcpt(true);
			cr4.set_osxsave(true);
			cr4.set_mce(true);
			write_cr4(cr4.into_bits());
			// Setup APIC ID.
			let cpu_feat_id=StandardProcessorFeatureIdentifiers::cpuid();
			let ext_topo_enum:ExtendedTopologyEnumeration<0>=ExtendedTopologyEnumeration::cpuid();
			self.apic_id=cpu_feat_id.local_apic_id();
			self.x2apic_id=ext_topo_enum.d.x2apic_id();
			// Setup Family-Model-Stepping.
			self.cpuid_fms=cpu_feat_id.into_bits() as u32;
			// Check if we are under nested hypervisor.
			self.under_hvm=cpu_feat_id.hypervisor();
			// Set to the maximum XCR0.
			// Current implementation would only support up to AVX, AVX512 excluded.
			stack.host_xcr0=1;
			stack.host_xcr0|=(cpu_feat_id.sse() as u64)<<1;
			stack.host_xcr0|=(cpu_feat_id.avx() as u64)<<2;
			stack.guest_xcr0=_xgetbv(0);
			trace!("Using Guest XCR0 as 0x{:X}, Host XCR0 as 0x{:X}...",stack.guest_xcr0,stack.host_xcr0);
			// Save Segment States.
			self.vmwrite_segment(GUEST_CS_SELECTOR,&state.cs);
			self.vmwrite_segment(GUEST_DS_SELECTOR,&state.ds);
			self.vmwrite_segment(GUEST_ES_SELECTOR,&state.es);
			self.vmwrite_segment(GUEST_FS_SELECTOR,&state.fs);
			self.vmwrite_segment(GUEST_GS_SELECTOR,&state.gs);
			self.vmwrite_segment(GUEST_SS_SELECTOR,&state.ss);
			self.vmwrite_segment(GUEST_TR_SELECTOR,&state.tr);
			self.vmwrite_segment(GUEST_LDTR_SELECTOR,&state.ldtr);
			self.vmwrite_segment(GUEST_IDTR_SELECTOR,&state.idtr);
			self.vmwrite_segment(GUEST_GDTR_SELECTOR,&state.gdtr);
			// Save Control Registers.
			self.vmwrite(GUEST_CR0,state.cr0);
			self.vmwrite(GUEST_CR2,state.cr2);
			self.vmwrite(GUEST_CR3,state.cr3);
			self.vmwrite(GUEST_CR4,state.cr4);
			// Save Task Priority Register (CR8)
			let mut avic_ctrl=AvicControl::from_bits(0);
			avic_ctrl.set_v_tpr(state.cr8 as u8);
			self.vmwrite(AVIC_CONTROL,avic_ctrl.into_bits());
			// Save Debug Registers.
			self.vmwrite(GUEST_DR6,state.dr6);
			self.vmwrite(GUEST_DR7,state.dr7);
			// Save rflags, rsp and rip.
			self.vmwrite(GUEST_RFLAGS,2u64);
			self.vmwrite(GUEST_SSP,gssp);
			self.vmwrite(GUEST_RSP,gsp);
			self.vmwrite(GUEST_RIP,nvc_svm_guest_start as *const c_void as u64);
			// Save Processor Hidden State.
			vmsave(self.hvmcb.phys);
			let host_gsbase=&raw mut self.gs_context as u64;
			self.host_vmwrite(GUEST_GS_BASE,host_gsbase);
			// Save Model-Specific Registers.
			self.vmwrite(GUEST_PAT,state.pat);
			self.vmwrite(GUEST_EFER,state.efer);
			self.vmwrite(GUEST_STAR,state.star);
			self.vmwrite(GUEST_LSTAR,state.lstar);
			self.vmwrite(GUEST_CSTAR,state.cstar);
			self.vmwrite(GUEST_SFMASK,state.sfmask);
			self.vmwrite(GUEST_KERNEL_GS_BASE,state.gsswap);
			self.vmwrite(GUEST_SYSENTER_CS,state.sysenter_cs);
			self.vmwrite(GUEST_SYSENTER_ESP,state.sysenter_esp);
			self.vmwrite(GUEST_SYSENTER_EIP,state.sysenter_eip);
			// Setup IOPM and MSRPM.
			let hv=self.hypervisor as *mut SvmHypervisor;
			self.vmwrite(IOPM_PHYSICAL_ADDRESS,(*hv).iopm.phys);
			self.vmwrite(MSRPM_PHYSICAL_ADDRESS,(*hv).msrpm.phys);
			// Setup NPT.
			// Note that Host CR4.LA57 bit determines whether NPT uses 5-level paging or not.
			self.vmwrite(NPT_CR3,if cr4.la57() {(*hv).nptm.pml5e.phys} else {(*hv).nptm.pml4e.phys});
			let mut npt_ctrl=NptControl::from_bits(0);
			npt_ctrl.set_enable_npt(true);
			self.vmwrite(NPT_CONTROL,npt_ctrl);
			// ASID is required in AMD-V.
			self.vmwrite(GUEST_ASID,1u32);
			// Load Guest State.
			vmload(self.vmcb.phys);
		}
		trace!("Processor {} Completed setting up VMCB! (0x{:X})",self.vcpu_id,self.vmcb.phys);
		// "Return" puts the VMCB on rax register.
		self.vmcb.phys
	}

	fn subvert(&mut self)
	{
		info!("Processor {} entered subversion routine!",self.vcpu_id);
		// Enable SVM in EFER.
		let mut efer=Efer::from_bits(rdmsr(MSR_EFER));
		efer.set_svme(true);
		efer.set_nxe(true);
		wrmsr(MSR_EFER,efer.into_bits());
		// Block A20M & Redirect INIT
		// Intel blocks A20M in vmxon, why not we do this as well?
		// Redirecting INIT signal to #SX exception will allow us to
		// intercept INIT signal without leaving it pending.
		let vmcr=VmCr::from_bits(rdmsr(MSR_VMCR)).with_disa20m(true).with_r_init(true);
		wrmsr(MSR_VMCR,vmcr.into_bits());
		// Set the HSAVE Area.
		wrmsr(MSR_HSAVE_PA,self.hsave.phys);
		// Initialize Hypervisor Context stack.
		unsafe
		{
			let stack:*mut SvmStackTop=self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<SvmStackTop>()) as *mut SvmStackTop;
			trace!("Stack-Top of vCPU {}: {stack:p}",self.vcpu_id);
			(*stack).guest_vmcb_pa=self.vmcb.phys;
			(*stack).host_vmcb_pa=self.hvmcb.phys;
			(*stack).vcpu=self as *mut Self;
			(*stack).proc_id=self.vcpu_id;
			(*stack).custom_vcpu=null_mut();
			(*stack).nested_vcpu=null_mut();
			(*stack).reserved=0;
			nvc_svm_subvert_processor_a(stack);
		}
		info!("Processor {} completed subversion!",self.vcpu_id);
	}
}

pub type SvmCustomVmLockedList=Arc<RwLock<Vec<Option<Arc<RwLock<SvmCustomVm>>>>>>;

pub struct SvmHypervisor
{
	pub vcpus:Vec<SvmVcpu>,
	pub msrpm:MemoryDescriptor<2,c_void>,
	pub iopm:MemoryDescriptor<3,c_void>,
	pub nptm:SvmNptManager,
	pub host:HostSystem,
	pub iommu_manager:Option<Box<dyn IommuOps>>,
	pub pio_space:IoAddressSpace,
	pub mmio_space:IoAddressSpace,
	pub image_base:*mut c_void,
	pub image_size:u32,
	pub asid_max:u32,
	pub asid_pool:Vec<u8>,
	pub xsave_size:usize,
	pub features:EnabledFeatures,
	pub rmt_root:ReverseMappingTableRoot,
	pub vm_list:SvmCustomVmLockedList,
	pub cvm_iopm:MemoryDescriptor<3,usize,InternalPageAllocator>,
	pub cvm_msrpm:MemoryDescriptor<2,usize,InternalPageAllocator>,
	pub l5_npt:bool
}

impl SvmHypervisor
{
	pub fn is_rip_from_hypervisor(&self,rip:u64)->bool
	{
		let start=self.image_base as u64;
		let end=start+self.image_size as u64;
		(start..end).contains(&rip)
	}

	/// Allocation of ASID must be protected by hypervisor.
	pub fn alloc_asid(&mut self)->Option<u32>
	{
		let bmp:&mut BitmapSlice=unsafe{BitmapSlice::from_raw_parts_mut(self.asid_pool.as_mut_ptr().cast(),self.asid_pool.len())};
		let i=bmp.search_cleared_forward()? as u32;
		if i<self.asid_max
		{
			let _=bmp.set(i as usize);
			Some(i)
		}
		else
		{
			None
		}
	}

	pub fn free_asid(&mut self,asid:u32)
	{
		if asid<self.asid_max
		{
			let bmp:&mut BitmapSlice=unsafe{BitmapSlice::from_raw_parts_mut(self.asid_pool.as_mut_ptr().cast(),self.asid_pool.len())};
			let _=bmp.reset(asid as usize);
		}
	}
}

impl Default for SvmHypervisor
{
	fn default() -> Self
	{
		let svm_feat=SvmFeatureIdentifier::cpuid();
		let mut asid_quotient=(svm_feat.asid() as usize)>>3;
		let asid_remainder=(svm_feat.asid() as usize)&7;
		if asid_remainder!=0
		{
			asid_quotient+=1;
		}
		let mut asid_pool:Vec<u8>=vec![0;asid_quotient];
		let xstate_cpuid=ExtendedStateEnumeration0::cpuid();
		let cvm_iopm=MemoryDescriptor::alloc().unwrap();
		let cvm_msrpm=MemoryDescriptor::alloc().unwrap();
		unsafe
		{
			memset(cvm_msrpm.virt as *mut c_void,0xFF,page_4kb_mult(2));
			memset(cvm_iopm.virt as *mut c_void,0xFF,page_4kb_mult(2)+1);
		}
		// ASID 0 and 1 are reserved.
		asid_pool[0]=3;
		Self
		{
			vcpus:Vec::with_capacity(unsafe{noir_get_processor_count() as usize}),
			msrpm:MemoryDescriptor::null(),
			iopm:MemoryDescriptor::null(),
			nptm:SvmNptManager::default(),
			host:HostSystem::build(),
			iommu_manager:None,
			pio_space:IoAddressSpace{regions:Vec::new()},
			mmio_space:IoAddressSpace{regions:Vec::new()},
			image_base:null_mut(),
			image_size:0,
			asid_max:svm_feat.asid(),
			asid_pool,
			xsave_size:xstate_cpuid.supported_size() as usize,
			features:EnabledFeatures::get(),
			rmt_root:ReverseMappingTableRoot::new(),
			vm_list:Arc::new(RwLock::new(Vec::with_capacity(8))),
			cvm_iopm,
			cvm_msrpm,
			l5_npt:false
		}
	}
}

impl HypervisorCapabilities for SvmHypervisor
{
	fn check_support()->u32
	{
		let proc_feat=ExtendedFeatureIdentifier::cpuid();
		if proc_feat.svm()
		{
			let svm_feat=SvmFeatureIdentifier::cpuid();
			// At least one ASID should be available.
			if svm_feat.asid()>0
			{
				let mut ret:u32=1;
				// Nested Paging
				ret|=if svm_feat.npt() {2} else {0};
				// Accelerated Nested Virtualization
				ret|=if svm_feat.vmsave_virt() && svm_feat.vgif() {4} else {0};
				return ret;
			}
			DECODE_ASSIST_SUPPORT.store(svm_feat.decode_assists(),Ordering::Relaxed);
			NEXT_RIP_SAVING_SUPPORT.store(svm_feat.nrips(),Ordering::Relaxed);
		}
		0
	}

	fn check_enabled()->bool
	{
		let vmcr=VmCr::from_bits(rdmsr(MSR_VMCR));
		!vmcr.svmdis()
	}
}

impl Drop for SvmHypervisor
{
	fn drop(&mut self)
	{
		
	}
}

unsafe impl Send for SvmHypervisor {}
unsafe impl Sync for SvmHypervisor {}

impl HypervisorEssentials for SvmHypervisor
{
	fn subvert_system(&mut self)->Status
	{
		// Use a macro to help cleanup-on-fail and return.
		macro_rules! fail_cleanup
		{
			($($arg:tt)*) =>
			{
				{
					error!("{}\n",format_args!($($arg)*));
					return Status::INSUFFICIENT_RESOURCES;
				}
			};
		}
		info!("Subverting the system with AMD-V...");
		info!("Enabled features: {}",self.features);
		// Allocate various stuff. Note that they are required to be raw-pointer.
		match MemoryDescriptor::alloc()
		{
			Some(md)=>
			{
				self.msrpm=md;
				let msrpm:&mut RefBitmap<65536>=unsafe{RefBitmap::from_raw_mut_ptr(self.msrpm.virt.cast())};
				// Setup basic interceptions to MSRs that may interfere with SVM normal operations.
				// This is also for nested virtualization.
				let mut set_interception=|index:u32,read:bool,write:bool|
				{
					let base=match index
					{
						0..0x2000=>Some((index<<1) as usize),
						0xC0000000..0xC0002000=>Some(((index-0xC0000000)<<1) as usize+0x4000),
						0xC0010000..0xC0012000=>Some(((index-0xC0010000)<<1) as usize+0x8000),
						_=>None
					};
					match base
					{
						Some(i)=>
						{
							let _=msrpm.assign(i,read);
							let _=msrpm.assign(i+1,write);
						}
						None=>warn!("MSR 0x{index:X} is invalid!")
					}
				};
				set_interception(MSR_EFER,true,true);
				set_interception(MSR_TSC_RATIO,true,true);
				set_interception(MSR_VMCR,true,true);
				set_interception(MSR_IGNNE,true,true);
				set_interception(MSR_SMM_CTRL,true,true);
				set_interception(MSR_HSAVE_PA,true,true);
				// There is no need to intercept read because the processor will always return zero on reads.
				set_interception(MSR_SVM_KEY,false,true);
				set_interception(MSR_SMM_KEY,false,true);
			}
			None=>fail_cleanup!("Failed to allocate MSR Permission-Map!")
		}
		match MemoryDescriptor::alloc()
		{
			Some(md)=>self.iopm=md,
			None=>fail_cleanup!("Failed to allocate I/O Permission-Map!")
		}
		debug!("MSRPM: 0x{:016X}, IOPM: 0x{:016X}",self.msrpm.phys,self.iopm.phys);
		let vcpu_count=unsafe{noir_get_processor_count()};
		for i in 0..vcpu_count
		{
			let mut vcpu=SvmVcpu::new();
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.hsave=md,
				None=>fail_cleanup!("Failed to allocate HSAVE Area for processor {}!",i)
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.vmcb=md,
				None=>fail_cleanup!("Failed to allocate VMCB for processor {}!",i)
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.hvmcb=md,
				None=>fail_cleanup!("Failed to allocate Host-VMCB for processor {}!",i)
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.hv_stack=md,
				None=>fail_cleanup!("Failed to allocate hypervisor stack for processor {}!",i)
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.ist[1]=md,
				None=>fail_cleanup!("Failed to allocate host IST1 stack for processor {}!",i)
			}
			vcpu.hypervisor=self as *mut Self as *mut c_void;
			// Allocate XSAVE state size.
			vcpu.host_xsave.init(self.xsave_size);
			vcpu.vcpu_id=i;
			self.vcpus.push(vcpu);
		}
		// Intercept APIC Accesses.
		/*#[cfg(target_os="uefi")]
		{
			use exit::svm_apic_output_handler;
			let apic_bar=rdmsr(MSR_APIC_BASE);
			self.mmio_space.add_region(IoRegion::new("lapic",None,svm_apic_output_handler,page_4kb_base(apic_bar),PAGE_SIZE as u64));
		}*/
		// Initialize NPT.
		self.nptm.build_identity_map();
		if self.features.enable_iommu()
		{
			self.iommu_manager=create_iommu();
			if let Some(iommu)=&mut self.iommu_manager
			{
				iommu.setup_mapping();
				iommu.protect_ci();
				iommu.protect_allocated_pages();
				iommu.subvert();
				// Add all IOMMU BARs to the I/O Filter.
				let mut v=iommu.get_bar_pages();
				while let Some(x)=v.pop()
				{
					self.mmio_space.add_region(x);
				}
			}
		}
		self.nptm.setup_mmio_filter(&self.mmio_space);
		self.nptm.protect_ci();
		self.nptm.protect_allocated_pages();
		extern "C" fn subvert_processor_thunk(context:*mut c_void,processor_id:u32)
		{
			let hv:&mut SvmHypervisor=unsafe{&mut *context.cast()};
			info!("Subverting processor {processor_id} with AMD-V...");
			match hv.vcpus.get_mut(processor_id as usize)
			{
				Some(vcpu)=>vcpu.subvert(),
				None=>panic!("Processor ID ({processor_id}) out of bounds! Check for broadcaster bugs!\n")
			}
		}
		unsafe
		{
			nvc_store_image_info(&raw mut self.image_base,&raw mut self.image_size);
			debug!("Base: {:p}, Size: 0x{:X}",self.image_base,self.image_size);
			noir_generic_call(subvert_processor_thunk,self as *mut Self as *mut c_void);
		}
		info!("System subversion completed!");
		Status::SUCCESS
	}

	fn restore_system(&mut self)->Status
	{
		info!("System restoration feature is removed! Please reboot the system instead.");
		Status::NOT_IMPLEMENTED
	}
}