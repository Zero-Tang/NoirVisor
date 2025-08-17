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
use iced_x86::MasmFormatter;
use core::{ffi::c_void, ptr::null_mut};

use ia32::{cpuid::CPUID_VMX, msr::*};
use vmcs::*;
use ept::VtEptManager;
use crate::*;
#[cfg(windows)] use mshv_core::forwarder::MshvCallForwarder;
use xpf_core::{asm::{cpuid::cpuid, crdr::*, msr::*, seg::*, vt::*}, bitmap::*, allocator::alloc_contd_pages, hv_host::{x86::{HostProcessor, HostSystem, PerCpuGsException}, NOIR_HYPERCALL_CODE_CALLEXIT}, ioflt::IoAddressSpace, nvbdk::*, nvstatus::*, x86::{caching::MEMORY_TYPE_WB, crdr::*, descriptors::SELECTOR_RPLTI_MASK, interrupts::InterruptStackFrameWithErrorCode, msr::{MSR_CSTAR, MSR_KERNEL_GS_BASE, MSR_LSTAR, MSR_SFMASK, MSR_STAR}}};

#[allow(dead_code)] mod ia32;
#[allow(dead_code)] mod vmcs;
#[allow(dead_code)] mod exit;
#[allow(dead_code)] mod ept;
#[allow(dead_code)] mod decode;

#[repr(C)] pub struct VtStackTop
{
	pub arg_home:[u64;4],
	pub volatile_xmms:VolatileXmmState,
	pub gpr_state:GprState,
	pub guest_frame:InterruptStackFrameWithErrorCode,
	pub vcpu:*mut VtVcpu,
	pub custom_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub nested_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub proc_id:u32,
	// This flag indicates whether the assembly code should use vmlaunch or vmresume.
	pub flags:u32
}

pub struct VtVcpu
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
	// Always use this member to format the mnemonic of an instruction.
	// Do not use `MasmFormatter::new()` on your own because it will cause runtime allocation!
	pub disasm_fmter:MasmFormatter,
	// This context handles exceptions.
	pub gs_context:PerCpuGsException
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
			msr_auto_guest:[VmxMsrAutoItem::default();5],
			disasm_fmter:MasmFormatter::new(),
			gs_context:PerCpuGsException::default()
		}
	}
}

unsafe extern "C"
{
	#[allow(improper_ctypes)]
	fn nvc_vt_subvert_processor_a(stack:*mut VtVcpu);
	fn nvc_vt_exit_handler_a();
	fn nvc_vt_guest_start();
	fn nvc_vt_resume_without_entry(gpr_state:*const GprState)->!;
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
			let hv:*const VtHypervisor=self.hypervisor.cast();
			let stack:*mut VtStackTop=self.hv_stack.byte_add(HYPERVISOR_STACK_SIZE-size_of::<VtStackTop>()).cast();
			// Setup Host State.
			let mut ist:[*mut c_void;8]=self.ist;
			ist[1]=self.ist[1].byte_add(HYPERVISOR_STACK_SIZE);
			HostProcessor::build(&mut self.host_cpu,&ist);
			let idtr=(*hv).host.idt.get_reg();
			let gdtr=self.host_cpu.gdt.get_reg();
			// Setup Host Stack
			(*stack).vcpu=self as *mut Self;
			(*stack).custom_vcpu=null_mut();
			(*stack).nested_vcpu=null_mut();
			(*stack).proc_id=self.vcpu_id;
			(*stack).flags=0;
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
			vmwriteptr(HOST_GS_BASE,&raw mut self.gs_context as usize);
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
			vmwrite32(GUEST_CS_ACCESS_RIGHTS,SegmentAccessRights::from_raw(state.cs.selector,state.cs.attrib).into_bits());
			vmwriteptr(GUEST_CS_BASE,state.cs.base as usize);
			// Guest State Area - DS Segment
			vmwrite16(GUEST_DS_SELECTOR,state.ds.selector);
			vmwrite32(GUEST_DS_LIMIT,state.ds.limit);
			vmwrite32(GUEST_DS_ACCESS_RIGHTS,SegmentAccessRights::from_raw(state.ds.selector,state.ds.attrib).into_bits());
			vmwriteptr(GUEST_DS_BASE,state.ds.base as usize);
			// Guest State Area - ES Segment
			vmwrite16(GUEST_ES_SELECTOR,state.es.selector);
			vmwrite32(GUEST_ES_LIMIT,state.es.limit);
			vmwrite32(GUEST_ES_ACCESS_RIGHTS,SegmentAccessRights::from_raw(state.es.selector,state.es.attrib).into_bits());
			vmwriteptr(GUEST_ES_BASE,state.es.base as usize);
			// Guest State Area - FS Segment
			vmwrite16(GUEST_FS_SELECTOR,state.fs.selector);
			vmwrite32(GUEST_FS_LIMIT,state.fs.limit);
			vmwrite32(GUEST_FS_ACCESS_RIGHTS,SegmentAccessRights::from_raw(state.fs.selector,state.fs.attrib).into_bits());
			vmwriteptr(GUEST_FS_BASE,state.fs.base as usize);
			// Guest State Area - GS Segment
			vmwrite16(GUEST_GS_SELECTOR,state.gs.selector);
			vmwrite32(GUEST_GS_LIMIT,state.gs.limit);
			vmwrite32(GUEST_GS_ACCESS_RIGHTS,SegmentAccessRights::from_raw(state.gs.selector,state.gs.attrib).into_bits());
			vmwriteptr(GUEST_GS_BASE,state.gs.base as usize);
			// Guest State Area - SS Segment
			vmwrite16(GUEST_SS_SELECTOR,state.ss.selector);
			vmwrite32(GUEST_SS_LIMIT,state.ss.limit);
			vmwrite32(GUEST_SS_ACCESS_RIGHTS,SegmentAccessRights::from_raw(state.ss.selector,state.ss.attrib).into_bits());
			vmwriteptr(GUEST_SS_BASE,state.ss.base as usize);
			// Guest State Area - TR Segment
			vmwrite16(GUEST_TR_SELECTOR,state.tr.selector);
			vmwrite32(GUEST_TR_LIMIT,state.tr.limit);
			let tr_ar=if cfg!(target_os="uefi")
			{
				0x8B
			}
			else
			{
				SegmentAccessRights::from_raw(state.tr.selector,state.tr.attrib).into_bits()
			};
			vmwrite32(GUEST_TR_ACCESS_RIGHTS,tr_ar);
			vmwriteptr(GUEST_TR_BASE,state.tr.base as usize);
			// Guest State Area - LDTR Segment
			vmwrite16(GUEST_LDTR_SELECTOR,state.ldtr.selector);
			vmwrite32(GUEST_LDTR_LIMIT,state.ldtr.limit);
			vmwrite32(GUEST_LDTR_ACCESS_RIGHTS,SegmentAccessRights::from_raw(state.ldtr.selector,state.ldtr.attrib).into_bits());
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
			vmwrite64(GUEST_MSR_IA32_PAT,state.pat);
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
		let mut pin_ctrl=VmxPinBasedControls::from_bits(0);
		// Setup Pin-Based VM-Execution Controls.
		// Filter unsupported fields.
		let pin_ctrl_msr=VmxPinBasedCtrlMsr::read(true_msr);
		pin_ctrl|=pin_ctrl_msr.get_allowed0();
		pin_ctrl&=pin_ctrl_msr.get_allowed1();
		// Write to VMCS.
		unsafe
		{
			vmwrite32(PIN_BASED_VM_EXECUTION_CONTROLS,pin_ctrl.into_bits());
		}
	}

	fn setup_procbased_controls(&self,true_msr:bool)
	{
		// Setup Primary Processor-Based VM-Execution Controls
		let mut proc_ctrl=VmxPrimaryProcessorControls::from_bits(0);
		proc_ctrl.set_use_io_bitmap(true);
		proc_ctrl.set_use_msr_bitmap(true);
		proc_ctrl.set_activate_secondary_controls(true);
		// Filter unsupported fields.
		let proc_ctrl_msr=VmxPriProcCtrlMsr::read(true_msr);
		proc_ctrl|=proc_ctrl_msr.get_allowed0();
		proc_ctrl&=proc_ctrl_msr.get_allowed1();
		// Setup Secondary Processor-Based VM-Execution Controls
		let mut proc_ctrl2=VmxSecondaryProcessorControls::from_bits(0);
		proc_ctrl2.set_enable_ept(true);
		proc_ctrl2.set_enable_rdtscp(true);
		proc_ctrl2.set_enable_vpid(true);
		proc_ctrl2.set_unrestricted_guest(true);
		proc_ctrl2.set_enable_invpcid(true);
		proc_ctrl2.set_enable_xsaves_xrstors(true);
		proc_ctrl2.set_enable_umwait(true);
		// Filter unsupported fields.
		let proc_ctrl2_msr=VmxSecProcCtrlMsr::read();
		proc_ctrl2|=proc_ctrl2_msr.get_allowed0();
		proc_ctrl2&=proc_ctrl2_msr.get_allowed1();
		// Write to VMCS.
		unsafe
		{
			vmwrite32(PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,proc_ctrl.into_bits());
			vmwrite32(SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,proc_ctrl2.into_bits());
		}
	}

	fn setup_vmexit_controls(&self,true_msr:bool)
	{
		// Setup VM-Exit Controls
		let mut exit_ctrl=VmxExitControls::from_bits(0);
		exit_ctrl.set_save_debug_controls(true);
		exit_ctrl.set_host_address_space_size(cfg!(target_arch="x86_64"));
		exit_ctrl.set_load_efer(true);
		exit_ctrl.set_save_efer(true);
		exit_ctrl.set_load_pat(true);
		exit_ctrl.set_save_pat(true);
		// Filter unsupported fields.
		let exit_ctrl_msr=VmxExitCtrlMsr::read(true_msr);
		exit_ctrl|=exit_ctrl_msr.get_allowed0();
		exit_ctrl&=exit_ctrl_msr.get_allowed1();
		// Write to VMCS.
		unsafe
		{
			vmwrite32(VMEXIT_CONTROLS,exit_ctrl.into_bits());
		}
	}

	fn setup_vmentry_controls(&self,true_msr:bool)
	{
		// Setup VM-Entry Controls
		let mut entry_ctrl=VmxEntryControls::from_bits(0);
		entry_ctrl.set_load_debug_controls(true);
		entry_ctrl.set_ia32e_mode_guest(cfg!(target_arch="x86_64"));
		entry_ctrl.set_load_efer(true);
		entry_ctrl.set_load_pat(true);
		// Filter unsupported fields.
		let entry_ctrl_msr=VmxEntryCtrlMsr::read(true_msr);
		entry_ctrl|=entry_ctrl_msr.get_allowed0();
		entry_ctrl&=entry_ctrl_msr.get_allowed1();
		unsafe
		{
			vmwrite32(VMENTRY_CONTROLS,entry_ctrl.into_bits());
		}
	}

	fn setup_memory_virtualization(&self)
	{
		let mut eptp=VmxEptPointer::from(0);
		eptp.set_page_walk_length(3);
		eptp.set_ept_memory_type(MEMORY_TYPE_WB as u64);
		eptp.set_enable_ad_flags(true);
		unsafe
		{
			let hv:*const VtHypervisor=self.hypervisor.cast();
			eptp.set_eptp_pa((*hv).eptm.pml4e.phys>>PAGE_4KB_SHIFT);
			vmwrite16(GUEST_VPID,1);
			vmwrite64(EPT_POINTER,eptp.into_bits());
		}
	}

	fn setup_control_area(&self)
	{
		let true_msr=VmxBasicMsr::read().use_true_msr();
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
		let state=ProcessorState::new();
		self.setup_guest_state_area(&state,gsp);
		self.setup_msr_auto_list(&state);
		self.setup_host_state_area(&state);
		self.setup_control_area();
		info!("Processor {} completed setting up VMCS!",self.vcpu_id);
		// xpf_core::asm::misc::int3();
		let r=unsafe{vmlaunch()};
		panic!("Failed to launch VM! {r}");
	}

	fn subvert(&mut self)
	{
		info!("Processor {} entered subversion routine!",self.vcpu_id);
		let vt_basic=VmxBasicMsr::read();
		// Setup Revision Identifier.
		unsafe
		{
			self.vmxon.virt.cast::<u32>().write(vt_basic.revision_id());
			self.vmcs.virt.cast::<u32>().write(vt_basic.revision_id());
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
								info!("VMCS has been loaded to CPU {} successfully!",self.vcpu_id);
								unsafe
								{
									nvc_vt_subvert_processor_a(self as *mut Self);
									info!("Processor {} completed subversion!",self.vcpu_id);
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

	fn restore(&mut self)
	{
		unsafe
		{
			// Leave VMX Non-Root Operation by vmcall.
			vmcall(NOIR_HYPERCALL_CODE_CALLEXIT,self as *mut Self as usize);
			// Turn off VMX.
			vmxoff();
			// Clear CR4.VMXE bit.
			let cr4=read_cr4()&!CR4_VMXE;
			write_cr4(cr4);
			info!("Processor {} completed restoration!",self.vcpu_id);
		}
	}
}

pub struct VtHypervisor
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
	pub image_size:u32,
	pub features:EnabledFeatures,
	#[cfg(windows)] pub mshvcall_forwarder:Option<MshvCallForwarder>
}

impl VtHypervisor
{
	pub fn is_rip_from_hypervisor(&self,rip:usize)->bool
	{
		let start=self.image_base as usize;
		let end=start+self.image_size as usize;
		(start..end).contains(&rip)
	}
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
			image_size:0,
			features:EnabledFeatures::get(),
			#[cfg(windows)] mshvcall_forwarder:MshvCallForwarder::new()
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
			let use_true_msr=vt_basic.use_true_msr();
			let pri_proc_sup=VmxPriProcCtrlMsr::read(use_true_msr);
			basic_requirement&=pri_proc_sup.get_allowed1().use_msr_bitmap();
			let vt_misc=VmxMiscMsr::read();
			if pri_proc_sup.get_allowed1().activate_secondary_controls()
			{
				let sec_proc_sup=VmxSecProcCtrlMsr::read();
				if sec_proc_sup.get_allowed1().enable_ept()
				{
					let ept_sup=VmxEptVpidCapMsr::read();
					let mut ept_requirement:bool=true;
					// We have a series of EPT feature requirements.
					ept_requirement&=ept_sup.support_wb_ept();
					// 2MiB-paging is NoirVisor's minimum requirement.
					// 1GiB-paging is preferred, but some processors don't support it (e.g.: vCPU in VMware).
					ept_requirement&=ept_sup.support_2mb_paging();
					ept_requirement&=ept_sup.support_invept();
					ept_requirement&=ept_sup.support_single_context_invept();
					ept_requirement&=ept_sup.support_global_context_invept();
					ept_requirement&=ept_sup.support_invvpid();
					ept_requirement&=ept_sup.support_ia_invvpid();
					ept_requirement&=ept_sup.support_sc_invvpid();
					ept_requirement&=ept_sup.support_ac_invvpid();
					if ept_requirement {supportability|=2;}
				}
				basic_requirement&=sec_proc_sup.get_allowed1().enable_vpid();
				basic_requirement&=sec_proc_sup.get_allowed1().unrestricted_guest();
				let mut accel_nvirt_requirement:bool=true;
				accel_nvirt_requirement&=sec_proc_sup.get_allowed1().vmcs_shadowing();
				accel_nvirt_requirement&=vt_misc.allow_vmcs_write_anywhere();
				if accel_nvirt_requirement
				{
					supportability|=4;
				}
			}
			#[cfg(target_os="uefi")]
			{
				basic_requirement&=vt_misc.support_wait_for_sipi_state();
			}
			if basic_requirement {supportability|=1;}
		}
		supportability
	}

	fn check_enabled()->bool
	{
		let mut feat_ctrl=rdmsr(MSR_FEATURE_CONTROL);
		sysdprintln!("IA32_FEATURE_CONTROL= 0x{feat_ctrl:X}");
		if (feat_ctrl&MSR_FEATURE_CONTROL_LOCK)==0
		{
			// In Bochs, VMX is disabled by default, but it's not locked.
			sysdprintln!("Enabling VMX since it's not locked...");
			wrmsr(MSR_FEATURE_CONTROL,MSR_FEATURE_CONTROL_LOCK|MSR_FEATURE_CONTROL_VMXON_OUT_SMX);
			feat_ctrl=rdmsr(MSR_FEATURE_CONTROL);
		}
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
					error!("{}",format_args!($($arg)*));
					return NOIR_INSUFFICIENT_RESOURCES;
				}
			};
		}
		info!("Subverting the system with Intel VT-x...");
		// Allocate various stuff. Note that they are required to be raw-pointer.
		match alloc_contd_pages(PAGE_SIZE)
		{
			Some(md)=>
			{
				self.msr_bitmap=md;
				let set_interception=|index:u32,read:bool,write:bool|
				{
					// Get bitmap position.
					let i=if (0..0x2000).contains(&index) {index}
						else if (0xC0000000..0xC0002000).contains(&index) {index-0xC0000000}
						else
						{
							warn!("MSR (0x{index:X}) can't be intercepted via bitmap!");
							return;
						} as usize;
					let bmp_r:&mut Bitmap<8192>=unsafe{Bitmap::from_raw_parts_mut(self.msr_bitmap.virt.byte_add(if index>=0xC0000000 {0x400} else {0}))};
					let bmp_w:&mut Bitmap<8192>=unsafe{Bitmap::from_raw_parts_mut(self.msr_bitmap.virt.byte_add(if index>=0xC0000000 {0xC00} else {0x800}))};
					bmp_r.assign(i,read);
					bmp_w.assign(i,write);
				};
				// Intercept accesses to the microcode updater.
				set_interception(MSR_BIOS_UPDATE_TRIGGER,true,true);
				// Intercept accesses to VMX MSRs.
				set_interception(MSR_VMX_BASIC,true,false);
				set_interception(MSR_VMX_PIN_BASED_CTLS,true,false);
				set_interception(MSR_VMX_PROC_BASED_CTLS,true,false);
				set_interception(MSR_VMX_EXIT_CTLS,true,false);
				set_interception(MSR_VMX_ENTRY_CTLS,true,false);
				set_interception(MSR_VMX_MISC,true,false);
				set_interception(MSR_VMX_CR0_FIXED0,true,false);
				set_interception(MSR_VMX_CR0_FIXED1,true,false);
				set_interception(MSR_VMX_CR4_FIXED0,true,false);
				set_interception(MSR_VMX_CR4_FIXED1,true,false);
				set_interception(MSR_VMX_VMCS_ENUM,true,false);
				set_interception(MSR_VMX_PROC_BASED_CTLS2,true,false);
				set_interception(MSR_VMX_EPT_VPID_CAP,true,false);
				set_interception(MSR_VMX_TRUE_PIN_BASED_CTLS,true,false);
				set_interception(MSR_VMX_TRUE_PROC_BASED_CTLS,true,false);
				set_interception(MSR_VMX_TRUE_EXIT_CTLS,true,false);
				set_interception(MSR_VMX_TRUE_ENTRY_CTLS,true,false);
				set_interception(MSR_VMX_VMFUNC,true,false);
				set_interception(MSR_VMX_PROC_BASED_CTLS3,true,false);
				set_interception(MSR_VMX_EXIT_CTLS2,true,false);
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
		self.eptm.protect_allocated_pages();
		self.eptm.protect_ci();
		unsafe
		{
			nvc_store_image_info(&raw mut self.image_base,&raw mut self.image_size);
			debug!("Base: {:p}, Size: 0x{:X}",self.image_base,self.image_size);
			noir_generic_call(nvc_vt_subvert_processor_thunk,self as *mut Self as *mut c_void);
		}
		info!("System Subversion Completed!");
		NOIR_SUCCESS
	}

	fn restore_system(&mut self)->Status
	{
		unsafe
		{
			noir_generic_call(nvc_vt_restore_processor_thunk,self as *mut Self as *mut c_void);
		}
		info!("System Restoration Completed!");
		NOIR_SUCCESS
	}
}

extern "C" fn nvc_vt_subvert_processor_thunk(context:*mut c_void,processor_id:u32)
{
	let hv:&mut VtHypervisor=unsafe{&mut *context.cast()};
	let vp=hv.vcpus.get_mut(processor_id as usize);
	info!("Subverting processor {processor_id} with Intel VT-x...");
	match vp
	{
		Some(vcpu)=>vcpu.subvert(),
		None=>panic!("WTF? Processor ID out of bounds!\n")
	}

}

extern "C" fn nvc_vt_restore_processor_thunk(context:*mut c_void,processor_id:u32)
{
	let hv:&mut VtHypervisor=unsafe{&mut *context.cast()};
	let vp=hv.vcpus.get_mut(processor_id as usize);
	info!("Processor {processor_id} entered restoration routine...");
	match vp
	{
		Some(vcpu)=>vcpu.restore(),
		None=>panic!("WTF? Processor ID out of bounds!\n")
	}
}