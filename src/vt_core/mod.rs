/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the Intel VT-x driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::vec::Vec;
use bitfield_struct::bitfield;
use static_collections::bitmap::RefBitmap;
use core::{arch::x86_64::_xgetbv, ffi::c_void, ptr::null_mut, sync::atomic::AtomicBool};

use ia32::msr::*;
use vmcs::*;
use ept::VtEptManager;
use crate::{xpf_core::x86::{apic::APIC_OFFSET_ICR_HI, msr::MSR_X2APIC_ICR}, *};
#[cfg(windows)] use mshv_core::forwarder::MshvCallForwarder;
use mshv_core::{MshvVcpuContext,MshvVcpuOps};
use xpf_core::{asm::{crdr::*, msr::*, seg::*, vt::*}, hv_host::{x86::{HostProcessor, HostSystem, PerCpuGsException}, NOIR_HYPERCALL_CODE_CALLEXIT}, ioflt::IoAddressSpace, nvbdk::*, x86::{apic::*, caching::MEMORY_TYPE_WB, crdr::*, descriptors::SELECTOR_RPLTI_MASK, interrupts::InterruptStackFrameWithErrorCode, msr::{MSR_APIC_BASE,MSR_CSTAR, MSR_KERNEL_GS_BASE, MSR_LSTAR, MSR_SFMASK, MSR_STAR}, rflags::RFLAGS_IF_BIT}};

#[allow(dead_code)] mod ia32;
#[allow(dead_code)] mod vmcs;
#[allow(dead_code)] mod exit;
mod hvcall;
#[allow(dead_code)] mod ept;
#[allow(dead_code)] mod decode;

#[bitfield(u32)] pub struct VtStackContextFlags
{
	/// If set, use vmlaunch upon next VM-Entry. Otherwise, use vmresume.
	pub use_vmlaunch:bool,
	#[bits(28)] rsvd:u32,
	/// If set, the available context is specified in `request_full_context` field. \
	/// If reset, this is phase 1 and only volatile registers are available.
	pub in_phase2:bool,
	/// If Phase I cannot handle this VM-Exit, go to Phase II so that more contexts are available.
	/// - 0: Phase I can handle this VM-Exit. No need to go to Phase II.
	/// - 1: Phase I cannot handle this VM-Exit. It requires all GPRs in Phase II.
	/// - 2: Phase I cannot handle this VM-Exit. In addition to all GPRs, XSAVE-state is also required in Phase II.
	/// - 3: Reserved.
	#[bits(2)] pub request_full_context:usize
}

impl VtStackContextFlags
{
	pub const REQUEST_VOLATILE_REGISTERS:u32=0;
	pub const REQUEST_ALL_GPR:u32=1;
	pub const REQUEST_FULL_CONTEXT:u32=2;
}

#[repr(C,align(16))] pub struct VtStackTop
{
	pub arg_home:[u64;4],
	pub gpr_state:GprState,
	pub guest_frame:InterruptStackFrameWithErrorCode,
	pub xsave_state:*mut c_void,
	pub vcpu:*mut VtVcpu,
	pub custom_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub nested_vcpu:*mut c_void,	// NOT IMPLEMENTED IN RUST
	pub proc_id:u32,
	// This flag indicates whether the assembly code should use vmlaunch or vmresume.
	pub flags:VtStackContextFlags,
	pub guest_xcr0:u64,
	pub host_xcr0:u64
}

enum VtIrqInterruptibilityState
{
	/// Can be immediately delivered.
	Interruptible,
	/// Either masked by RFLAGS.IF or interrupt-shadow. \
	/// Should wait for interrupt window.
	MaskedByRflags,
	/// Masked by TPR. \
	/// Should wait for TPR-below-threshold.
	MaskedByTpr(u8)
}

pub struct VtVcpu
{
	pub vmcs:MemoryDescriptor<1,c_void>,
	pub vmxon:MemoryDescriptor<1,c_void>,
	pub vapic:MemoryDescriptor<1,c_void>,
	pub hv_stack:MemoryDescriptor<HYPERVISOR_STACK_PAGE_COUNT,c_void>,
	pub hypervisor:*mut c_void,
	pub ist:[MemoryDescriptor<HYPERVISOR_STACK_PAGE_COUNT,c_void>;8],
	pub host_xsave:Vec<u8>,
	pub cpuid_fms:u32,
	pub vcpu_id:u32,
	pub under_hvm:bool,
	pub apic_id:u8,
	pub x2apic_id:u32,
	pub host_cpu:HostProcessor,
	pub mshv_ctxt:MshvVcpuContext,
	pub msr_auto_host:[VmxMsrAutoItem;5],
	pub msr_auto_guest:[VmxMsrAutoItem;5],
	pub cached_ctxt:CachedExitContext,
	pub irq_bmp:[u64;4],
	pub special_icr_completed:AtomicBool,
	pub waiting_for_sipi:AtomicBool,
	// This context handles exceptions.
	pub gs_context:PerCpuGsException
}

impl Default for VtVcpu
{
	fn default() -> Self
	{
		let std_leaf=StandardProcessorFeatureIdentifiers::cpuid();
		Self
		{
			vmcs:MemoryDescriptor::null(),
			vmxon:MemoryDescriptor::null(),
			vapic:MemoryDescriptor::null(),
			hv_stack:MemoryDescriptor::null(),
			hypervisor:null_mut(),
			ist:[const{MemoryDescriptor::null()};8],
			host_xsave:Vec::new(),
			cpuid_fms:(std_leaf.ext_model()<<16)|0x600,
			vcpu_id:0,
			under_hvm:false,
			apic_id:0,
			x2apic_id:0,
			host_cpu:HostProcessor::default(),
			mshv_ctxt:MshvVcpuContext::new(&VT_MSHV_VCPU_OPS),
			msr_auto_host:[VmxMsrAutoItem::default();5],
			msr_auto_guest:[VmxMsrAutoItem::default();5],
			cached_ctxt:CachedExitContext::default(),
			irq_bmp:[0;4],
			special_icr_completed:AtomicBool::new(false),
			waiting_for_sipi:AtomicBool::new(false),
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

#[unsafe(no_mangle)] unsafe extern "C" fn nvc_vt_subvert_processor_i(vcpu:*mut VtVcpu,gsp:usize,gssp:usize)
{
	unsafe
	{
		(*vcpu).subvert_i(gsp,gssp)
	}
}

static VT_MSHV_VCPU_OPS:MshvVcpuOps=MshvVcpuOps
{
	inform_apic_icr:VtVcpu::inform_apic_icr,
	in_long_mode:VtVcpu::in_long_mode,
	get_vp_index:VtVcpu::get_vp_index
};

impl VtVcpu
{
	unsafe fn inform_apic_icr(_vcpu:*mut c_void,icr_lo:u32,icr_hi:u32)
	{
		// Forward this to APIC.
		// FIXME: Generalize APIC BAR
		let bar=0xfee00000 as *mut u32;
		unsafe
		{
			// TODO: Filter INIT/SIPI ICR writes.
			bar.byte_add(APIC_OFFSET_ICR_HI).write_volatile(icr_hi);
			bar.byte_add(APIC_OFFSET_ICR_LO).write_volatile(icr_lo);
		}
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

	#[inline(always)] pub fn get_stack_top(&self)->&VtStackTop
	{
		unsafe
		{
			&*self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<VtStackTop>()).cast()
		}
	}

	#[inline(always)] pub fn get_stack_top_mut<'a,'b>(&'a mut self)->&'b mut VtStackTop
	{
		unsafe
		{
			&mut *self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<VtStackTop>()).cast()
		}
	}

	fn is_interruptible(&mut self,irq:u8)->VtIrqInterruptibilityState
	{
		let rflags=self.cached_ctxt.rflags();
		if (rflags&(1<<RFLAGS_IF_BIT))==0
		{
			// Not interruptible because it's masked in rflags.
			// trace!("Current rflags: 0x{rflags:X} and IRQ-Vector 0x{irq:X} will be masked.");
			VtIrqInterruptibilityState::MaskedByRflags
		}
		else
		{
			// Check interruptibility.
			let interruptibility=self.cached_ctxt.interruptibility();
			if interruptibility.blocking_by_sti() || interruptibility.blocking_by_mov_ss()
			{
				// Not interruptible due to interrupt-shadow.
				trace!("Current Interruptibility-State: 0x{:X} and IRQ-Vector 0x{irq:X} will be masked due to interrupt-shadow.",interruptibility.into_bits());
				VtIrqInterruptibilityState::MaskedByRflags
			}
			else
			{
				// Check TPR.
				let required_priority=irq>>4;
				let current_tpr=unsafe{*self.vapic.virt.byte_add(0x80).cast::<u8>()}>>4;
				// Interruptible if required priority is higher than current TPR.
				if required_priority>current_tpr
				{
					VtIrqInterruptibilityState::Interruptible
				}
				else
				{
					// trace!("Current TPR: 0x{current_tpr:X} and IRQ-Vector 0x{irq:X} will be masked.");
					VtIrqInterruptibilityState::MaskedByTpr(required_priority)
				}
			}
		}
	}

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
		let hv:*const VtHypervisor=self.hypervisor.cast();
		let (gdt_base,idt_base,stack)=unsafe
		{
			let stack:*mut VtStackTop=self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<VtStackTop>()).cast();
			// Setup Host State.
			let mut ist:[*mut c_void;8]=[null_mut();8];
			ist[1]=self.ist[1].virt.byte_add(HYPERVISOR_STACK_SIZE);
			HostProcessor::build(&mut self.host_cpu,&ist);
			let idtr=(*hv).host.idt.get_reg();
			let gdtr=self.host_cpu.gdt.get_reg();
			// Setup Host Stack
			(*stack).vcpu=self as *mut Self;
			(*stack).custom_vcpu=null_mut();
			(*stack).nested_vcpu=null_mut();
			(*stack).proc_id=self.vcpu_id;
			(*stack).flags=VtStackContextFlags::from_bits(0);
			// Also load into host now.
			write_idtr(&raw const idtr);
			write_gdtr(&raw const gdtr);
			write_tr(self.host_cpu.tr_sel);
			write_cr3((*hv).host.paging.cr3.phys);
			// Test IDT.
			// xpf_core::asm::misc::ud2();
			(idtr.base,gdtr.base,stack)
		};
		// Load them into VMCS.
		vmwriteptr(HOST_GDTR_BASE,gdt_base as usize);
		vmwriteptr(HOST_IDTR_BASE,idt_base as usize);
		vmwrite16(HOST_TR_SELECTOR,self.host_cpu.tr_sel);
		vmwriteptr(HOST_TR_BASE,&raw const self.host_cpu.tss as usize);
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
		vmwriteptr(HOST_CR3,unsafe{(*hv).host.paging.cr3.phys} as usize);
		vmwriteptr(HOST_CR4,cr4&!(CR4_CET as usize));
		vmwrite64(HOST_MSR_IA32_EFER,state.efer);
		// Host State Area - Stack Pointer, Instruction Pointer
		vmwriteptr(HOST_RSP,stack as usize);
		vmwriteptr(HOST_RIP,nvc_vt_exit_handler_a as *const c_void as usize);
	}

	fn setup_guest_state_area(&self,state:&ProcessorState,gsp:usize,gssp:usize)
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
		// vmwriteptr(CR0_READ_SHADOW,state.cr0);
		vmwriteptr(CR4_READ_SHADOW,state.cr4&(!CR4_VMXE as usize));
		// CR8 is special. It's located in Virtual APIC Page.
		unsafe
		{
			*self.vapic.virt.byte_add(APIC_OFFSET_TPR).cast::<u64>()=state.cr8<<4;
		}
		// Clear CR8 to 0 in host so that External-Interrupt Exiting can work properly.
		write_cr8(0);
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
		vmwriteptr(GUEST_SSP,gssp);
		vmwriteptr(GUEST_RSP,gsp);
		vmwriteptr(GUEST_RIP,nvc_vt_guest_start as *const c_void as usize);
		vmwriteptr(GUEST_RFLAGS,2);
		// VMCS Link Pointer.
		vmwrite64(VMCS_LINK_POINTER,u64::MAX);
	}

	fn setup_pinbased_controls(&self,true_msr:bool)
	{
		let mut pin_ctrl=VmxPinBasedControls::from_bits(0);
		// Setup Pin-Based VM-Execution Controls.
		// Filter unsupported fields.
		let pin_ctrl_msr=VmxPinBasedCtrlMsr::read(true_msr);
		// In future implementation, we may want to enable external interrupt exiting
		// in order to enable interrupts delivered by Intel VT-d page faults.
		// pin_ctrl.set_external_interrupt_exiting(true);
		pin_ctrl|=pin_ctrl_msr.get_allowed0();
		pin_ctrl&=pin_ctrl_msr.get_allowed1();
		// Write to VMCS.
		vmwrite32(PIN_BASED_VM_EXECUTION_CONTROLS,pin_ctrl.into_bits());
	}

	fn setup_procbased_controls(&mut self,true_msr:bool)
	{
		// Setup Primary Processor-Based VM-Execution Controls
		let mut proc_ctrl=VmxPrimaryProcessorControls::from_bits(0);
		// proc_ctrl.set_use_tpr_shadow(true);
		proc_ctrl.set_use_io_bitmap(true);
		proc_ctrl.set_use_msr_bitmap(true);
		proc_ctrl.set_activate_secondary_controls(true);
		// Filter unsupported fields.
		let proc_ctrl_msr=VmxPriProcCtrlMsr::read(true_msr);
		proc_ctrl|=proc_ctrl_msr.get_allowed0();
		proc_ctrl&=proc_ctrl_msr.get_allowed1();
		self.cached_ctxt.write_proc_ctrl1(proc_ctrl);
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
		vmwrite32(PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,proc_ctrl.into_bits());
		vmwrite32(SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,proc_ctrl2.into_bits());
	}

	fn setup_vmexit_controls(&self,true_msr:bool)
	{
		// Setup VM-Exit Controls
		let mut exit_ctrl=VmxExitControls::from_bits(0);
		exit_ctrl.set_save_debug_controls(true);
		// Acknowledging interrupts on exit may reduce our effort in handling interrupts.
		// exit_ctrl.set_acknowledge_interrupt_on_exit(true);
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
		vmwrite32(VMEXIT_CONTROLS,exit_ctrl.into_bits());
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
		vmwrite32(VMENTRY_CONTROLS,entry_ctrl.into_bits());
	}

	fn setup_memory_virtualization(&self)
	{
		let mut eptp=VmxEptPointer::from(0);
		eptp.set_page_walk_length(3);
		eptp.set_ept_memory_type(MEMORY_TYPE_WB as u64);
		// Disable a/d flags because not all CPUs necessarily support it.
		eptp.set_enable_ad_flags(false);
		unsafe
		{
			let hv:*const VtHypervisor=self.hypervisor.cast();
			eptp.set_eptp_pa((*hv).eptm.pml4e.phys>>PAGE_4KB_SHIFT);
		}
		vmwrite16(GUEST_VPID,1);
		vmwrite64(EPT_POINTER,eptp.into_bits());
	}

	fn setup_control_area(&mut self)
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
			let apic_base=page_base(rdmsr(MSR_APIC_BASE));
			// vmwriteptr(CR0_GUEST_HOST_MASK,CR0_PG as usize);
			vmwriteptr(CR4_GUEST_HOST_MASK,CR4_VMXE as usize);
			vmwrite64(ADDRESS_OF_MSR_BITMAP,(*hv).msr_bitmap.phys);
			vmwrite64(ADDRESS_OF_IO_BITMAP_A,(*hv).io_bitmap_a.phys);
			vmwrite64(ADDRESS_OF_IO_BITMAP_B,(*hv).io_bitmap_b.phys);
			vmwrite64(VIRTUAL_APIC_ADDRESS,self.vapic.phys);
			vmwrite64(APIC_ACCESS_ADDRESS,apic_base);
			vmwrite32(TPR_THRESHOLD,0);
		}
	}

	fn subvert_i(&mut self,gsp:usize,gssp:usize)
	{
		let state=ProcessorState::new();
		self.mshv_ctxt.root=(&raw mut *self).cast();
		self.setup_guest_state_area(&state,gsp,gssp);
		self.setup_msr_auto_list(&state);
		self.setup_host_state_area(&state);
		self.setup_control_area();
		self.cached_ctxt.flush();
		{
			let cpu_feat_id=StandardProcessorFeatureIdentifiers::cpuid();
			let stack=self.get_stack_top_mut();
			stack.xsave_state=self.host_xsave.as_mut_ptr().cast();
			// Set to the maximum XCR0.
			// Current implementation would only support up to AVX, AVX512 excluded.
			stack.host_xcr0=1;
			stack.host_xcr0|=(cpu_feat_id.sse() as u64)<<1;
			stack.host_xcr0|=(cpu_feat_id.avx() as u64)<<2;
			stack.guest_xcr0=unsafe{_xgetbv(0)};
			trace!("Using Guest XCR0 as 0x{:X}, Host XCR0 as 0x{:X}...",stack.guest_xcr0,stack.host_xcr0);
		}
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
									sysdprintln!("Processor {} completed subversion!",self.vcpu_id);
									// Test INIT
									// let apic_base=page_base(rdmsr(crate::xpf_core::x86::msr::MSR_APIC_BASE));
									// (apic_base as *mut u32).byte_add(crate::xpf_core::x86::apic::APIC_OFFSET_ICR_HI).write(0);
									// (apic_base as *mut u32).byte_add(crate::xpf_core::x86::apic::APIC_OFFSET_ICR_LO).write(0x44500);
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
			sysdprintln!("Processor {} completed restoration!",self.vcpu_id);
		}
	}
}

pub struct VtHypervisor
{
	pub vcpus:Vec<VtVcpu>,
	pub msr_bitmap:MemoryDescriptor<1,usize>,
	pub io_bitmap_a:MemoryDescriptor<1,usize>,
	pub io_bitmap_b:MemoryDescriptor<1,usize>,
	pub eptm:VtEptManager,
	pub host:HostSystem,
	pub pio_space:IoAddressSpace<u16>,
	pub mmio_space:IoAddressSpace<u64>,
	pub image_base:*mut c_void,
	pub image_size:u32,
	pub xsave_size:usize,
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
		let xsave_cpuid=ExtendedStateEnumeration0::cpuid();
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
			xsave_size:xsave_cpuid.supported_size() as usize,
			features:EnabledFeatures::get(),
			#[cfg(windows)] mshvcall_forwarder:MshvCallForwarder::new()
		}
	}
}

impl HypervisorCapabilities for VtHypervisor
{
	fn check_support()->u32
	{
		let cpu_feat_id=StandardProcessorFeatureIdentifiers::cpuid();
		let mut supportability:u32=0;
		if cpu_feat_id.vmx()
		{
			let mut basic_requirement:bool=true;
			let vt_basic=VmxBasicMsr::read();
			let use_true_msr=vt_basic.use_true_msr();
			let pin_sup=VmxPinBasedCtrlMsr::read(use_true_msr);
			let pri_proc_sup=VmxPriProcCtrlMsr::read(use_true_msr);
			let exit_sup=VmxExitCtrlMsr::read(use_true_msr);
			let entry_sup=VmxEntryCtrlMsr::read(use_true_msr);
			// Most of the following requirements are necessary for NoirVisor CVM feature.
			// External-Interrupt Exiting and Interrupt-Acknowledging on Exit are required
			// for filtering IOMMU-fault interrupts.
			basic_requirement&=pin_sup.get_allowed1().external_interrupt_exiting();
			basic_requirement&=pin_sup.get_allowed1().nmi_exiting();
			basic_requirement&=pin_sup.get_allowed1().virtual_nmi();
			basic_requirement&=pri_proc_sup.get_allowed1().interrupt_window_exiting();
			basic_requirement&=pri_proc_sup.get_allowed1().use_tsc_offsetting();
			basic_requirement&=pri_proc_sup.get_allowed1().hlt_exiting();
			basic_requirement&=pri_proc_sup.get_allowed1().nmi_window_exiting();
			basic_requirement&=pri_proc_sup.get_allowed1().unconditional_io_exiting();
			basic_requirement&=pri_proc_sup.get_allowed1().monitor_trap_flag();
			basic_requirement&=pri_proc_sup.get_allowed1().use_msr_bitmap();
			basic_requirement&=exit_sup.get_allowed1().acknowledge_interrupt_on_exit();
			basic_requirement&=exit_sup.get_allowed1().load_efer();
			basic_requirement&=exit_sup.get_allowed1().save_efer();
			basic_requirement&=exit_sup.get_allowed1().load_pat();
			basic_requirement&=exit_sup.get_allowed1().save_pat();
			basic_requirement&=entry_sup.get_allowed1().load_efer();
			basic_requirement&=entry_sup.get_allowed1().load_pat();
			// While NoirVisor doesn't really support being a 32-bit hypervisor,
			// let's still put it in conditional-compilation block
			#[cfg(target_arch="x86_64")]
			{
				basic_requirement&=exit_sup.get_allowed1().host_address_space_size();
				basic_requirement&=entry_sup.get_allowed1().ia32e_mode_guest();
			}
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

impl Drop for VtHypervisor
{
	fn drop(&mut self)
	{
		
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
					return Status::INSUFFICIENT_RESOURCES;
				}
			};
		}
		info!("Subverting the system with Intel VT-x...");
		// Allocate various stuff. Note that they are required to be raw-pointer.
		match MemoryDescriptor::alloc()
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
					let bmp_r:&mut RefBitmap<8192>=unsafe{RefBitmap::from_raw_mut_ptr(self.msr_bitmap.virt.byte_add(if index>=0xC0000000 {0x400} else {0}).cast())};
					let bmp_w:&mut RefBitmap<8192>=unsafe{RefBitmap::from_raw_mut_ptr(self.msr_bitmap.virt.byte_add(if index>=0xC0000000 {0xC00} else {0x800}).cast())};
					let _=bmp_r.assign(i,read);
					let _=bmp_w.assign(i,write);
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
				// Intercept accesses to x2APIC ICR
				set_interception(MSR_X2APIC_ICR,false,true);
			}
			None=>fail_cleanup!("Failed to alloate MSR-Bitmap!")
		}
		match MemoryDescriptor::alloc()
		{
			Some(md)=>self.io_bitmap_a=md,
			None=>fail_cleanup!("Failed to allocate I/O-Bitmap A!")
		}
		match MemoryDescriptor::alloc()
		{
			Some(md)=>self.io_bitmap_b=md,
			None=>fail_cleanup!("Failed to allocate I/O-Bitmap B!")
		}
		let vcpu_count=unsafe{noir_get_processor_count()};
		for i in 0..vcpu_count
		{
			let mut vcpu=VtVcpu::default();
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.vmxon=md,
				None=>fail_cleanup!("Failed to allocate VMXON region for processor {i}!")
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.vmcs=md,
				None=>fail_cleanup!("Failed to allocate VMCS for processor {i}!")
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.vapic=md,
				None=>fail_cleanup!("Failed to allocate Virtual-APIC Page for processor {i}!")
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.hv_stack=md,
				None=>fail_cleanup!("Failed to allocate hypervisor stack for processor {i}!")
			}
			match MemoryDescriptor::alloc()
			{
				Some(md)=>vcpu.ist[1]=md,
				None=>fail_cleanup!("Failed to allocate host IST1 stack for processor {i}!")
			}
			vcpu.hypervisor=self as *mut Self as *mut c_void;
			vcpu.host_xsave.reserve_exact(self.xsave_size);
			unsafe
			{
				vcpu.host_xsave.set_len(self.xsave_size);
			}
			vcpu.vcpu_id=i;
			self.vcpus.push(vcpu);
		}
		// Initialize EPT.
		self.eptm.build_identity_map();
		self.eptm.protect_allocated_pages();
		self.eptm.protect_ci();
		extern "C" fn subvert_processor_thunk(context:*mut c_void,processor_id:u32)
		{
			let hv:&mut VtHypervisor=unsafe{&mut *context.cast()};
			info!("Subverting processor {processor_id} with Intel VT-x...");
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
		sysdprintln!("System Subversion Completed!");
		Status::SUCCESS
	}

	fn restore_system(&mut self)->Status
	{
		extern "C" fn restore_processor_thunk(context:*mut c_void,processor_id:u32)
		{
			let hv:&mut VtHypervisor=unsafe{&mut *context.cast()};
			info!("Processor {processor_id} entered restoration routine...");
			match hv.vcpus.get_mut(processor_id as usize)
			{
				Some(vcpu)=>vcpu.restore(),
				None=>panic!("Processor ID ({processor_id}) out of bounds! Check for broadcaster bugs!\n")
			}
		}
		unsafe
		{
			noir_generic_call(restore_processor_thunk,self as *mut Self as *mut c_void);
		}
		info!("System Restoration Completed!");
		Status::SUCCESS
	}
}