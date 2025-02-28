/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the Intel VT-x driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::vec::Vec;
use ept::VtEptManager;
use core::{ffi::c_void, ptr::null_mut};

use ia32::{cpuid::CPUID_VMX, msr::*};
use vmcs::*;
use crate::{xpf_core::{asm::{cpuid::cpuid, crdr::*, msr::rdmsr, seg::*, vt::*}, bitmap::*, dlalloc::alloc_contd_pages, hv_host::x86::{HostProcessor, HostSystem}, ioflt::IoAddressSpace, nvbdk::*, nvstatus::*, x86::{caching::MEMORY_TYPE_WB, crdr::*, descriptors::SELECTOR_RPLTI_MASK, msr::{MSR_CSTAR, MSR_KERNEL_GS_BASE, MSR_LSTAR, MSR_SFMASK, MSR_STAR}}}, HypervisorEssentials, *};

#[allow(dead_code)] mod ia32;
#[allow(dead_code)] mod vmcs;
#[allow(dead_code)] mod exit;
#[allow(dead_code)] mod ept;

#[repr(C)] pub struct VtStackTop
{
	pub vcpu:*mut VtVcpu,
	pub custom_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub nested_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub proc_id:u32,
	// This flag indicates whether the assembly code should use vmlaunch or vmresume.
	pub flags:u32
}

#[repr(C)] pub struct VtVcpu
{
	pub vmcs:MemoryDescriptor,
	pub vmxon:MemoryDescriptor,
	pub hv_stack:*mut c_void,
	pub hypervisor:*mut c_void,
	pub ist:[*mut c_void;8],
	pub cpuid_fms:u32,
	pub vcpu_id:u32,
	pub under_hvm:bool,
	pub apic_id:u8,
	pub x2apic_id:u32,
	pub host_cpu:HostProcessor,
	pub msr_auto_host:[VmxMsrAutoItem;5],
	pub msr_auto_guest:[VmxMsrAutoItem;5],
}

impl Default for VtVcpu
{
	fn default() -> Self
	{
		Self
		{
			vmcs:MemoryDescriptor::null(),
			vmxon:MemoryDescriptor::null(),
			hv_stack:null_mut(),
			hypervisor:null_mut(),
			ist:[null_mut();8],
			cpuid_fms:0,
			vcpu_id:0,
			under_hvm:false,
			apic_id:0,
			x2apic_id:0,
			host_cpu:HostProcessor::default(),
			msr_auto_host:[VmxMsrAutoItem::default();5],
			msr_auto_guest:[VmxMsrAutoItem::default();5]
		}
	}
}

unsafe extern "C"
{
	fn nvc_vt_subvert_processor_a(stack:*mut VtVcpu);
	fn nvc_vt_exit_handler_a();
	fn nvc_vt_guest_start();
}

#[unsafe(no_mangle)] unsafe extern "C" fn nvc_vt_subvert_processor_i(vcpu:*mut VtVcpu,gsp:usize)
{
	unsafe
	{
		(*vcpu).subvert_i(gsp)
	}
}

impl VtVcpu
{
	fn setup_msr_auto_list(&mut self,state:&ProcessorState)
	{
		self.msr_auto_guest[0]=VmxMsrAutoItem::new(MSR_STAR,state.star);
		self.msr_auto_guest[1]=VmxMsrAutoItem::new(MSR_LSTAR,state.lstar);
		self.msr_auto_guest[2]=VmxMsrAutoItem::new(MSR_CSTAR,state.cstar);
		self.msr_auto_guest[3]=VmxMsrAutoItem::new(MSR_SFMASK,state.sfmask);
		self.msr_auto_guest[4]=VmxMsrAutoItem::new(MSR_KERNEL_GS_BASE,state.gsswap);
		self.msr_auto_host[0]=VmxMsrAutoItem::new(MSR_STAR,state.star);
		self.msr_auto_host[1]=VmxMsrAutoItem::new(MSR_LSTAR,state.lstar);
		self.msr_auto_host[2]=VmxMsrAutoItem::new(MSR_CSTAR,state.cstar);
		self.msr_auto_host[3]=VmxMsrAutoItem::new(MSR_SFMASK,state.sfmask);
		self.msr_auto_host[4]=VmxMsrAutoItem::new(MSR_KERNEL_GS_BASE,state.gsswap);
		unsafe
		{
			vmwrite32(VMENTRY_MSR_LOAD_COUNT,self.msr_auto_guest.len() as u32);
			vmwrite64(VMENTRY_MSR_LOAD_ADDRESS,noir_get_physical_address(self.msr_auto_guest.as_mut_ptr().cast()));
			vmwrite32(VMEXIT_MSR_STORE_COUNT,self.msr_auto_guest.len() as u32);
			vmwrite64(VMEXIT_MSR_STORE_ADDRESS,noir_get_physical_address(self.msr_auto_guest.as_mut_ptr().cast()));
			vmwrite32(VMEXIT_MSR_LOAD_COUNT,self.msr_auto_host.len() as u32);
			vmwrite64(VMEXIT_MSR_LOAD_ADDRESS,noir_get_physical_address(self.msr_auto_host.as_mut_ptr().cast()));
		}
	}

	fn setup_host_state_area(&mut self,state:&ProcessorState)
	{
		unsafe
		{
			let hv:*mut VtHypervisor=self.hypervisor.cast();
			let stack:*mut VtStackTop=self.hv_stack.byte_add(HYPERVISOR_STACK_SIZE-size_of::<VtStackTop>()).cast();
			// Setup Host State.
			let mut ist:[*mut c_void;8]=self.ist;
			ist[1]=self.ist[1].byte_add(HYPERVISOR_STACK_SIZE);
			HostProcessor::build(&mut self.host_cpu,&ist);
			let idtr=(*hv).host.idt.get_reg();
			let gdtr=self.host_cpu.gdt.get_reg();
			// Load them into VMCS.
			vmwriteptr(HOST_GDTR_BASE,gdtr.base as usize);
			vmwriteptr(HOST_IDTR_BASE,idtr.base as usize);
			vmwrite16(HOST_TR_SELECTOR,self.host_cpu.tr_sel);
			vmwriteptr(HOST_TR_BASE,&raw const self.host_cpu.tss as usize);
			// Also load into host now.
			write_idtr(&raw const idtr);
			write_gdtr(&raw const gdtr);
			write_tr(self.host_cpu.tr_sel);
			write_cr3((*hv).host.paging.cr3.phys);
			// Test IDT.
			// xpf_core::asm::misc::ud2();
			// Host State Area - Segment Selectors
			vmwrite16(HOST_CS_SELECTOR,state.cs.selector&SELECTOR_RPLTI_MASK);
			vmwrite16(HOST_DS_SELECTOR,state.ds.selector&SELECTOR_RPLTI_MASK);
			vmwrite16(HOST_ES_SELECTOR,state.es.selector&SELECTOR_RPLTI_MASK);
			vmwrite16(HOST_FS_SELECTOR,state.fs.selector&SELECTOR_RPLTI_MASK);
			vmwrite16(HOST_GS_SELECTOR,state.gs.selector&SELECTOR_RPLTI_MASK);
			vmwrite16(HOST_SS_SELECTOR,state.ss.selector&SELECTOR_RPLTI_MASK);
			// Host State Area - Segment Bases
			vmwriteptr(HOST_FS_BASE,state.fs.base as usize);
			vmwriteptr(HOST_GS_BASE,state.gs.base as usize);
			// Host State Area - Control Registers
			let cr0=(state.cr0|rdmsr(MSR_VMX_CR0_FIXED0) as usize)&rdmsr(MSR_VMX_CR0_FIXED1) as usize;
			let cr4=(state.cr4|rdmsr(MSR_VMX_CR4_FIXED0) as usize)&rdmsr(MSR_VMX_CR4_FIXED1) as usize;
			vmwriteptr(HOST_CR0,cr0);
			vmwriteptr(HOST_CR3,(*hv).host.paging.cr3.phys as usize);
			vmwriteptr(HOST_CR4,cr4&!(CR4_CET as usize));
			vmwrite64(HOST_MSR_IA32_EFER,state.efer);
			// Host State Area - Stack Pointer, Instruction Pointer
			vmwriteptr(HOST_RSP,stack as usize);
			vmwriteptr(HOST_RIP,nvc_vt_exit_handler_a as usize);
		}
	}

	fn setup_guest_state_area(&self,state:&ProcessorState,gsp:usize)
	{
		unsafe
		{
			// Guest State Area - CS Segment
			vmwrite16(GUEST_CS_SELECTOR,state.cs.selector);
			vmwrite32(GUEST_CS_LIMIT,state.cs.limit);
			vmwrite32(GUEST_CS_ACCESS_RIGHTS,vt_attrib(state.cs.selector,state.cs.attrib));
			vmwriteptr(GUEST_CS_BASE,state.cs.base as usize);
			// Guest State Area - DS Segment
			vmwrite16(GUEST_DS_SELECTOR,state.ds.selector);
			vmwrite32(GUEST_DS_LIMIT,state.ds.limit);
			vmwrite32(GUEST_DS_ACCESS_RIGHTS,vt_attrib(state.ds.selector,state.ds.attrib));
			vmwriteptr(GUEST_DS_BASE,state.ds.base as usize);
			// Guest State Area - ES Segment
			vmwrite16(GUEST_ES_SELECTOR,state.es.selector);
			vmwrite32(GUEST_ES_LIMIT,state.es.limit);
			vmwrite32(GUEST_ES_ACCESS_RIGHTS,vt_attrib(state.es.selector,state.es.attrib));
			vmwriteptr(GUEST_ES_BASE,state.es.base as usize);
			// Guest State Area - FS Segment
			vmwrite16(GUEST_FS_SELECTOR,state.fs.selector);
			vmwrite32(GUEST_FS_LIMIT,state.fs.limit);
			vmwrite32(GUEST_FS_ACCESS_RIGHTS,vt_attrib(state.fs.selector,state.fs.attrib));
			vmwriteptr(GUEST_FS_BASE,state.fs.base as usize);
			// Guest State Area - GS Segment
			vmwrite16(GUEST_GS_SELECTOR,state.gs.selector);
			vmwrite32(GUEST_GS_LIMIT,state.gs.limit);
			vmwrite32(GUEST_GS_ACCESS_RIGHTS,vt_attrib(state.gs.selector,state.gs.attrib));
			vmwriteptr(GUEST_GS_BASE,state.gs.base as usize);
			// Guest State Area - SS Segment
			vmwrite16(GUEST_SS_SELECTOR,state.ss.selector);
			vmwrite32(GUEST_SS_LIMIT,state.ss.limit);
			vmwrite32(GUEST_SS_ACCESS_RIGHTS,vt_attrib(state.ss.selector,state.ss.attrib));
			vmwriteptr(GUEST_SS_BASE,state.ss.base as usize);
			// Guest State Area - TR Segment
			vmwrite16(GUEST_TR_SELECTOR,state.tr.selector);
			vmwrite32(GUEST_TR_LIMIT,state.tr.limit);
			let tr_ar=if cfg!(target_os="uefi")
			{
				0x808B
			}
			else
			{
				vt_attrib(state.tr.selector,state.tr.attrib)
			};
			vmwrite32(GUEST_TR_ACCESS_RIGHTS,tr_ar);
			vmwriteptr(GUEST_TR_BASE,state.tr.base as usize);
			// Guest State Area - LDTR Segment
			vmwrite16(GUEST_LDTR_SELECTOR,state.ldtr.selector);
			vmwrite32(GUEST_LDTR_LIMIT,state.ldtr.limit);
			vmwrite32(GUEST_LDTR_ACCESS_RIGHTS,vt_attrib(state.ldtr.selector,state.ldtr.attrib));
			vmwriteptr(GUEST_LDTR_BASE,state.ldtr.base as usize);
			// Guest State Area - IDTR and GDTR
			vmwrite32(GUEST_GDTR_LIMIT,state.gdtr.limit);
			vmwriteptr(GUEST_GDTR_BASE,state.gdtr.base as usize);
			vmwrite32(GUEST_IDTR_LIMIT,state.idtr.limit);
			vmwriteptr(GUEST_IDTR_BASE,state.idtr.base as usize);
			// Save Control Registers.
			vmwriteptr(GUEST_CR0,state.cr0);
			vmwriteptr(GUEST_CR3,state.cr3);
			vmwriteptr(GUEST_CR4,state.cr4);
			vmwriteptr(CR0_READ_SHADOW,state.cr0);
			vmwriteptr(CR4_READ_SHADOW,state.cr4&(!CR4_VMXE as usize));
			// Save Debug Registers.
			vmwriteptr(GUEST_DR7,state.dr7);
			// Save Model-Specific Registers.
			vmwrite64(GUEST_MSR_IA32_DEBUG_CTRL,state.debug_ctrl);
			vmwrite32(GUEST_MSR_IA32_SYSENTER_CS,state.sysenter_cs as u32);
			vmwrite64(GUEST_MSR_IA32_SYSENTER_ESP,state.sysenter_esp);
			vmwrite64(GUEST_MSR_IA32_SYSENTER_EIP,state.sysenter_eip);
			vmwrite64(GUEST_MSR_IA32_EFER,state.efer);
			// Save rflags, rsp and rip.
			vmwriteptr(GUEST_RSP,gsp);
			vmwriteptr(GUEST_RIP,nvc_vt_guest_start as usize);
			vmwriteptr(GUEST_RFLAGS,2);
			// VMCS Link Pointer.
			vmwrite64(VMCS_LINK_POINTER,u64::MAX);
		}
	}

	fn setup_pinbased_controls(&self,true_msr:bool)
	{
		let mut pin_ctrl=VmxPinBasedControls(0);
		// Setup Pin-Based VM-Execution Controls.
		// Filter unsupported fields.
		let pin_ctrl_msr=VmxPinBasedCtrlMsr::read(true_msr);
		pin_ctrl.0|=pin_ctrl_msr.get_allowed0().0;
		pin_ctrl.0&=pin_ctrl_msr.get_allowed1().0;
		// Write to VMCS.
		unsafe
		{
			vmwrite32(PIN_BASED_VM_EXECUTION_CONTROLS,pin_ctrl.0);
		}
	}

	fn setup_procbased_controls(&self,true_msr:bool)
	{
		// Setup Primary Processor-Based VM-Execution Controls
		let mut proc_ctrl=VmxPrimaryProcessorControls(0);
		proc_ctrl.set_use_io_bitmap(true);
		proc_ctrl.set_use_msr_bitmap(true);
		proc_ctrl.set_activate_secondary_controls(true);
		// Filter unsupported fields.
		let proc_ctrl_msr=VmxPriProcCtrlMsr::read(true_msr);
		proc_ctrl.0|=proc_ctrl_msr.get_allowed0().0;
		proc_ctrl.0&=proc_ctrl_msr.get_allowed1().0;
		// Setup Secondary Processor-Based VM-Execution Controls
		let mut proc_ctrl2=VmxSecondaryProcessorControls(0);
		proc_ctrl2.set_enable_ept(true);
		proc_ctrl2.set_enable_rdtscp(true);
		proc_ctrl2.set_enable_vpid(true);
		proc_ctrl2.set_unrestricted_guest(true);
		proc_ctrl2.set_enable_invpcid(true);
		proc_ctrl2.set_enable_xsaves_xrstors(true);
		proc_ctrl2.set_enable_umwait(true);
		// Filter unsupported fields.
		let proc_ctrl2_msr=VmxSecProcCtrlMsr::read();
		proc_ctrl2.0|=proc_ctrl2_msr.get_allowed0().0;
		proc_ctrl2.0&=proc_ctrl2_msr.get_allowed1().0;
		// Write to VMCS.
		unsafe
		{
			vmwrite32(PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,proc_ctrl.0);
			vmwrite32(SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,proc_ctrl2.0);
		}
	}

	fn setup_vmexit_controls(&self,true_msr:bool)
	{
		// Setup VM-Exit Controls
		let mut exit_ctrl=VmxExitControls(0);
		exit_ctrl.set_save_debug_controls(true);
		exit_ctrl.set_host_address_space_size(cfg!(target_arch="x86_64"));
		exit_ctrl.set_load_efer(true);
		exit_ctrl.set_save_efer(true);
		// Filter unsupported fields.
		let exit_ctrl_msr=VmxExitCtrlMsr::read(true_msr);
		exit_ctrl.0|=exit_ctrl_msr.get_allowed0().0;
		exit_ctrl.0&=exit_ctrl_msr.get_allowed1().0;
		// Write to VMCS.
		unsafe
		{
			vmwrite32(VMEXIT_CONTROLS,exit_ctrl.0);
		}
	}

	fn setup_vmentry_controls(&self,true_msr:bool)
	{
		// Setup VM-Entry Controls
		let mut entry_ctrl=VmxEntryControls(0);
		entry_ctrl.set_load_debug_controls(true);
		entry_ctrl.set_ia32e_mode_guest(cfg!(target_arch="x86_64"));
		entry_ctrl.set_load_efer(true);
		// Filter unsupported fields.
		let entry_ctrl_msr=VmxEntryCtrlMsr::read(true_msr);
		entry_ctrl.0|=entry_ctrl_msr.get_allowed0().0;
		entry_ctrl.0&=entry_ctrl_msr.get_allowed1().0;
		unsafe
		{
			vmwrite32(VMENTRY_CONTROLS,entry_ctrl.0);
		}
	}

	fn setup_memory_virtualization(&self)
	{
		let mut eptp=VmxEptPointer(0);
		eptp.set_page_walk_length(3);
		eptp.set_ept_memory_type(MEMORY_TYPE_WB as u64);
		eptp.set_enable_ad_flags(true);
		unsafe
		{
			let hv:*const VtHypervisor=self.hypervisor.cast();
			eptp.set_eptp_pa((*hv).eptm.pml4e.phys>>PAGE_4KB_SHIFT);
			vmwrite16(GUEST_VPID,1);
			vmwrite64(EPT_POINTER,eptp.0);
		}
	}

	fn setup_control_area(&self)
	{
		let true_msr=VmxBasicMsr::read().get_use_true_msr();
		self.setup_pinbased_controls(true_msr);
		self.setup_procbased_controls(true_msr);
		self.setup_vmexit_controls(true_msr);
		self.setup_vmentry_controls(true_msr);
		self.setup_memory_virtualization();
		unsafe
		{
			let hv:*const VtHypervisor=self.hypervisor.cast();
			vmwriteptr(CR0_GUEST_HOST_MASK,0);
			vmwriteptr(CR4_GUEST_HOST_MASK,CR4_VMXE as usize);
			vmwrite64(ADDRESS_OF_MSR_BITMAP,(*hv).msr_bitmap.phys);
			vmwrite64(ADDRESS_OF_IO_BITMAP_A,(*hv).io_bitmap_a.phys);
			vmwrite64(ADDRESS_OF_IO_BITMAP_B,(*hv).io_bitmap_b.phys);
		}
	}

	fn subvert_i(&mut self,gsp:usize)
	{
		let mut state=ProcessorState::default();
		unsafe{noir_save_processor_state(&raw mut state)};
		self.setup_guest_state_area(&state,gsp);
		self.setup_msr_auto_list(&state);
		self.setup_host_state_area(&state);
		self.setup_control_area();
		println!("Processor {} completed setting up VMCS!",self.vcpu_id);
		// xpf_core::asm::misc::int3();
		let r=unsafe{vmlaunch()};
		panic!("Failed to launch VM! {r}");
	}

	fn subvert(&mut self)
	{
		println!("Processor {} entered subversion routine!",self.vcpu_id);
		let vt_basic=VmxBasicMsr::read();
		// Setup Revision Identifier.
		unsafe
		{
			self.vmxon.virt.cast::<u32>().write(vt_basic.get_revision_id());
			self.vmcs.virt.cast::<u32>().write(vt_basic.get_revision_id());
		}
		// Enable VMX in CR0 and CR4.
		let mut cr0=read_cr0();
		cr0|=rdmsr(MSR_VMX_CR0_FIXED0);
		cr0&=rdmsr(MSR_VMX_CR0_FIXED1);
		write_cr0(cr0);
		// In addition to CR4.VMXE, we also add following bits.
		// CR4.OSXSAVE bit is required to execute xsetbv instruction.
		let mut cr4=read_cr4();
		cr4|=CR4_OSFXSR|CR4_OSXMMEXCEPT|CR4_OSXSAVE;
		cr4|=rdmsr(MSR_VMX_CR4_FIXED0);
		cr4&=rdmsr(MSR_VMX_CR4_FIXED1);
		write_cr4(cr4);
		match unsafe{vmxon(&raw const self.vmxon.phys)}
		{
			VmxResult::Ok(_)=>
			{
				match unsafe{vmclear(&raw const self.vmcs.phys)}
				{
					VmxResult::Ok(_)=>
					{
						match unsafe{vmptrld(&raw const self.vmcs.phys)}
						{
							VmxResult::Ok(_)=>
							{
								println!("VMCS has been loaded to CPU successfully!");
								unsafe
								{
									nvc_vt_subvert_processor_a(self as *mut Self);
									println!("Processor {} completed subversion!",self.vcpu_id);
								}
							}
							r=>panic!("Failed to execute vmptrld! Reason: {r}")
						}
					}
					r=>panic!("Failed to execute vmclear! Reason: {r}")
				}
			}
			r=>panic!("Failed to execute VMXON! Reason: {r}")
		}
	}
}

#[repr(C)] pub struct VtHypervisor
{
	pub vcpus:Vec<VtVcpu>,
	pub msr_bitmap:MemoryDescriptor,
	pub io_bitmap_a:MemoryDescriptor,
	pub io_bitmap_b:MemoryDescriptor,
	pub eptm:VtEptManager,
	pub host:HostSystem,
	pub pio_space:IoAddressSpace<u16>,
	pub mmio_space:IoAddressSpace<u64>,
	pub image_base:*mut c_void,
	pub image_size:u32
}

impl Default for VtHypervisor
{
	fn default() -> Self
	{
		Self
		{
			vcpus:Vec::with_capacity(unsafe{noir_get_processor_count() as usize}),
			msr_bitmap:MemoryDescriptor::null(),
			io_bitmap_a:MemoryDescriptor::null(),
			io_bitmap_b:MemoryDescriptor::null(),
			eptm:VtEptManager::default(),
			host:HostSystem::build(),
			pio_space:IoAddressSpace{regions:Vec::new()},
			mmio_space:IoAddressSpace{regions:Vec::new()},
			image_base:null_mut(),
			image_size:0
		}
	}
}

impl HypervisorCapabilities for VtHypervisor
{
	fn check_support()->u32
	{
		let mut c:u32=0;
		let mut supportability:u32=0;
		cpuid(CPUID_STD_PROCESSOR_FEATURE,0,None,None,Some(&mut c),None);
		if c&CPUID_VMX==CPUID_VMX
		{
			let mut basic_requirement:bool=true;
			let vt_basic=VmxBasicMsr::read();
			let use_true_msr=vt_basic.get_use_true_msr();
			let pri_proc_sup=VmxPriProcCtrlMsr::read(use_true_msr);
			basic_requirement&=pri_proc_sup.get_allowed1().get_use_msr_bitmap();
			if pri_proc_sup.get_allowed1().get_activate_secondary_controls()
			{
				let sec_proc_sup=VmxSecProcCtrlMsr::read();
				if sec_proc_sup.get_allowed1().get_enable_ept()
				{
					let ept_sup=VmxEptVpidCapMsr::read();
					let mut ept_requirement:bool=true;
					// We have a series of EPT feature requirements.
					ept_requirement&=ept_sup.get_support_wb_ept();
					ept_requirement&=ept_sup.get_support_2mb_paging();
					ept_requirement&=ept_sup.get_support_invept();
					ept_requirement&=ept_sup.get_support_single_context_invept();
					ept_requirement&=ept_sup.get_support_global_context_invept();
					ept_requirement&=ept_sup.get_support_invvpid();
					ept_requirement&=ept_sup.get_support_sc_invvpid();
					ept_requirement&=ept_sup.get_support_ac_invvpid();
					if ept_requirement {supportability|=2;}
				}
				basic_requirement&=sec_proc_sup.get_allowed1().get_enable_vpid();
				basic_requirement&=sec_proc_sup.get_allowed1().get_unrestricted_guest();
				if sec_proc_sup.get_allowed1().get_vmcs_shadowing()
				{
					supportability|=4;
				}
			}
			if basic_requirement {supportability|=1;}
		}
		supportability
	}

	fn check_enabled()->bool
	{
		let feat_ctrl=rdmsr(MSR_FEATURE_CONTROL);
		feat_ctrl&MSR_FEATURE_CONTROL_VMXON_OUT_SMX==MSR_FEATURE_CONTROL_VMXON_OUT_SMX
	}
}

impl HypervisorEssentials for VtHypervisor
{
	fn subvert_system(&mut self)->Status
	{
		// Use a macro to help cleanup-on-fail and return.
		macro_rules! fail_cleanup
		{
			($($arg:tt)*) =>
			{
				{
					print!("{}\n",format_args!($($arg)*));
					return NOIR_INSUFFICIENT_RESOURCES;
				}
			};
		}
		println!("Subverting the system with Intel VT-x...");
		// Allocate various stuff. Note that they are required to be raw-pointer.
		match alloc_contd_pages(PAGE_SIZE)
		{
			Some(md)=>
			{
				self.msr_bitmap=md;
				unsafe
				{
					let set_interception=|index:u32,write:bool|
					{
						let bmp=self.msr_bitmap.virt
							.byte_add(if write {0x800} else {0})
							.byte_add(if index>=0xC0000000 {0x400} else {0});
						let i=if (0..0x2000).contains(&index) {index}
							else if (0xC0000000..0xC0002000).contains(&index) {index-0xC0000000}
							else {panic!("MSR (0x{index:X}) can't be intercepted via bitmap!")} as usize;
						set_bitmap(bmp,0x400,i);
					};
					set_interception(MSR_BIOS_UPDATE_TRIGGER,false);
					set_interception(MSR_BIOS_UPDATE_TRIGGER,true);
				}
			}
			None=>fail_cleanup!("Failed to alloate MSR-Bitmap!")
		}
		match alloc_contd_pages(PAGE_SIZE*2)
		{
			Some(md)=>
			{
				self.io_bitmap_a=md;
				self.io_bitmap_b=md.add(PAGE_SIZE);
			}
			None=>fail_cleanup!("Failed to allocate I/O-Bitmap!")
		}
		let vcpu_count=unsafe{noir_get_processor_count()};
		for i in 0..vcpu_count
		{
			let mut vcpu=VtVcpu::default();
			let vmxon=alloc_contd_pages(PAGE_SIZE);
			let vmcs=alloc_contd_pages(PAGE_SIZE);
			let stack=alloc_contd_pages(HYPERVISOR_STACK_SIZE);
			let ist1=alloc_contd_pages(HYPERVISOR_STACK_SIZE);
			match vmxon
			{
				Some(md)=>vcpu.vmxon=md,
				None=>fail_cleanup!("Failed to allocate VMXON region for processor {i}!")
			}
			match vmcs
			{
				Some(md)=>vcpu.vmcs=md,
				None=>fail_cleanup!("Failed to allocate VMCS for processor {i}!")
			}
			match stack
			{
				Some(md)=>vcpu.hv_stack=md.virt,
				None=>fail_cleanup!("Failed to allocate hypervisor stack for processor {i}!")
			}
			match ist1
			{
				Some(md)=>vcpu.ist[1]=md.virt,
				None=>fail_cleanup!("Failed to allocate host IST1 stack for processor {i}!")
			}
			vcpu.hypervisor=self as *mut Self as *mut c_void;
			vcpu.vcpu_id=i;
			self.vcpus.push(vcpu);
		}
		// Initialize EPT.
		self.eptm.build_identity_map();
		unsafe
		{
			nvc_store_image_info(&raw mut self.image_base,&raw mut self.image_size);
			noir_generic_call(nvc_vt_subvert_processor_thunk,self as *mut Self as *mut c_void);
		}
		println!("System Subversion Completed!");
		NOIR_SUCCESS
	}

	fn restore_system(&mut self)->Status
	{
		sysdprintln!("System restoration is not implemented!");
		NOIR_NOT_IMPLEMENTED
	}
}

#[unsafe(no_mangle)] extern "C" fn nvc_vt_subvert_processor_thunk(context:*mut c_void,processor_id:u32)
{
	let hv=context as *mut VtHypervisor;
	let vp=unsafe{(*hv).vcpus.get_mut(processor_id as usize)};
	sysdprintln!("Subverting processor {} with Intel VT-x...",processor_id);
	match vp
	{
		Some(vcpu)=>vcpu.subvert(),
		None=>panic!("WTF? Processor ID out of bounds!\n")
	}

}