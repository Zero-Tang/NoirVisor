/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file emulates INIT and virtualizes NMI in Intel VT-x of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

//! # INIT and Startup-IPI emulation
//! See [`crate::xpf_core::hv_host::x86::ipi`] for the overall design. INIT and Startup-IPI aimed at subverted processors are
//! posted to mailboxes and the target is kicked with an NMI. This file emulates the INIT signal and virtualizes NMIs.
//!
//! Intel VT-x discards a Startup-IPI if it arrives while the processor is in VMX root operation, even if the guest is in
//! the wait-for-SIPI state. Therefore, NoirVisor doesn't use the wait-for-SIPI state. The processor spins in the host
//! until the Startup-IPI is posted to its mailbox.
//!
//! ## NMI virtualization
//! "NMI exiting" and "virtual NMIs" are set, so the processor tracks the NMI-blocking of the guest. NMIs not sent by
//! NoirVisor are held pending and injected once the guest can take them, with the help of "NMI-window exiting".
//!
//! An NMI may also arrive while the processor is in VMX root operation. The host's NMI handler records it and sets
//! "NMI-window exiting" so that the processor leaves the guest right after VM-Entry to handle the NMI.

use core::{arch::global_asm, hint::cold_path, sync::atomic::Ordering};

use log::*;
use pastey::paste;

use super::{VtHypervisor, VtStackTop, VtVcpu, ia32::msr::*, vmcs::*};
use crate::{
	drv_core::lapic::*,
	xpf_core::{
		asm::{crdr::*, msr::rdmsr, vt::*},
		hv_host::x86::ipi::{IpiMailbox, IpiVcpu, filter_icr},
		x86::{
			crdr::*,
			descriptors::SegmentFlags,
			interrupts::{EventType, NMI_INTERRUPT},
		},
	},
};

global_asm!(
	// Unblock NMIs in the host by executing an `iret` instruction that returns to the next instruction.
	// A VM-Exit due to NMI leaves NMIs blocked, and the `iret` instruction in the guest doesn't unblock them.
	".global nvc_vt_unblock_nmi",
	"nvc_vt_unblock_nmi:",
	"	mov rax,rsp",
	"	mov ecx,ss",
	"	push rcx",
	"	push rax",
	"	pushfq",
	"	mov ecx,cs",
	"	push rcx",
	"	lea rcx,[rip+2f]",
	"	push rcx",
	"	iretq",
	"2:",
	"	ret"
);

unsafe extern "C" {
	fn nvc_vt_unblock_nmi();
}

impl IpiVcpu for VtVcpu
{
	fn ipi_mailboxes(&self) -> &[IpiMailbox]
	{
		let hv: &VtHypervisor = unsafe { &*self.hypervisor.cast() };
		&hv.ipi_mailboxes
	}

	fn ipi_index(&self) -> usize
	{
		self.vcpu_id as usize
	}
}

impl VtVcpu
{
	fn ipi_mailbox(&self) -> Option<&IpiMailbox>
	{
		self.ipi_mailboxes().get(self.ipi_index())
	}

	/// ## `filter_icr` method
	/// Filter an IPI sent by the guest. `icr_hi` is in the format of the current Local APIC mode. \
	/// Returns `true` if NoirVisor emulated the IPI, so it must not be sent. Otherwise, forward the IPI to the hardware.
	///
	/// INIT and Startup-IPI aimed at subverted processors are emulated. Other IPIs are forwarded.
	pub(super) fn filter_icr(&mut self, icr_lo: u32, icr_hi: u32) -> bool
	{
		filter_icr(self.ipi_mailboxes(), self.ipi_index(), icr_lo, icr_hi)
	}

	/// ## `post_init_to_self` method
	/// Post an INIT signal to this processor. It is emulated at the end of this VM-Exit.
	pub(super) fn post_init_to_self(&mut self)
	{
		if let Some(mb) = self.ipi_mailbox()
		{
			mb.post_init();
		}
	}

	/// ## `process_init_signal` method
	/// Emulate the INIT signal posted to this processor, then wait for the Startup-IPI. \
	/// Call this method at the very end of VM-Exit handling, since INIT discards all pending events of the guest.
	#[inline(always)]
	pub(super) fn process_init_signal(&mut self, context: &mut VtStackTop)
	{
		// The mailboxes belong to the hypervisor, not to this vCPU.
		let hv: &VtHypervisor = unsafe { &*self.hypervisor.cast() };
		let Some(mb) = hv.ipi_mailboxes.get(self.vcpu_id as usize)
		else
		{
			return;
		};
		if !mb.has_init()
		{
			return;
		}
		cold_path();
		info!("Processor {} received INIT signal! rip=0x{:X}", self.vcpu_id, self.cached_ctxt.rip);
		self.emulate_init(context);
		// Emulate the wait-for-SIPI state by spinning. Interrupts are disabled in the host, so they remain pending.
		let vector = mb.wait_for_sipi();
		info!("Processor {} received Startup-IPI with vector 0x{vector:02X}!", self.vcpu_id);
		// NMIs taken while waiting for the Startup-IPI are discarded.
		mb.take_kick();
		self.host_nmi.store(false, Ordering::Relaxed);
		self.nmi_pending = false;
		vmwrite16(GUEST_CS_SELECTOR, (vector as u16) << 8);
		vmwriteptr(GUEST_CS_BASE, (vector as usize) << 12);
		vmwriteptr(GUEST_RIP, 0);
		vmwrite32(GUEST_ACTIVITY_STATE, ActivityState::ACTIVE);
		// Invalidate the TLBs since paging is switched off.
		let ivc = InvvpidContext::Single(vmread16(GUEST_VPID).unwrap());
		unsafe {
			invvpid(&ivc);
		}
	}

	/// ## `emulate_init` method
	/// Put the guest into the state right after an INIT signal.
	fn emulate_init(&mut self, context: &mut VtStackTop)
	{
		// INIT discards all pending events of the guest.
		vmwrite32(VMENTRY_INTERRUPTION_INFORMATION_FIELD, 0);
		vmwriteptr(GUEST_PENDING_DEBUG_EXCEPTIONS, 0);
		self.cached_ctxt.write_interruptibility(InterruptibilityState::new());
		{
			let proc_ctrl = self.cached_ctxt.get_proc_ctrl1_mut();
			proc_ctrl.set_interrupt_window_exiting(false);
			proc_ctrl.set_nmi_window_exiting(false);
		}
		if self.extint_exiting_suspended
		{
			let mut pin_ctrl = VmxPinBasedControls::from_bits(vmread32(PIN_BASED_VM_EXECUTION_CONTROLS).unwrap());
			pin_ctrl.set_external_interrupt_exiting(true);
			vmwrite32(PIN_BASED_VM_EXECUTION_CONTROLS, pin_ctrl.into_bits());
			self.extint_exiting_suspended = false;
		}
		self.held_irqs.clear();
		self.nmi_pending = false;
		lapic_reset_for_init();
		// General-Purpose Registers
		let gpr_state = &mut context.gpr_state;
		for i in 0..16usize
		{
			gpr_state.write(i, 0);
		}
		gpr_state.rdx = self.cpuid_fms as u64;
		context.guest_xcr0 = 1;
		unsafe {
			vmwrite_unchecked(GUEST_RSP, 0);
			vmwrite_unchecked(GUEST_RIP, 0xFFF0);
			vmwrite_unchecked(GUEST_RFLAGS, 2);
			// Control Registers. INIT preserves CR0.CD and CR0.NW.
			let old_cr0 = Cr0::from_bits(vmreadptr(GUEST_CR0).unwrap() as u64);
			let mut cr0 = Cr0::new().with_et(true).with_cd(old_cr0.cd()).with_nw(old_cr0.nw());
			// Fix CR0 bits
			cr0 |= rdmsr(MSR_VMX_CR0_FIXED1);
			cr0 &= rdmsr(MSR_VMX_CR0_FIXED0);
			// CR0.PE and CR0.PG are cleared by INIT. Unrestricted guest allows them to be cleared.
			cr0.set_pe(false);
			cr0.set_pg(false);
			vmwrite_unchecked(GUEST_CR0, cr0.into_bits() as usize);
			vmwrite_unchecked(CR0_READ_SHADOW, cr0.into_bits() as usize);
			write_cr2(0);
			vmwrite_unchecked(GUEST_CR3, 0);
			// CR4 is cleared to 0 upon INIT. But as a guest, CR4.VMXE must be set.
			let mut cr4 = 0;
			cr4 |= rdmsr(MSR_VMX_CR4_FIXED0) as usize;
			cr4 &= rdmsr(MSR_VMX_CR4_FIXED1) as usize;
			vmwrite_unchecked(GUEST_CR4, cr4);
			vmwrite_unchecked(CR4_READ_SHADOW, 0);
			// EFER is cleared to 0 upon INIT.
			vmwrite_unchecked(GUEST_MSR_IA32_EFER, 0);
			// Debug Registers
			write_dr0(0);
			write_dr1(0);
			write_dr2(0);
			write_dr3(0);
			write_dr6(0xffff0ff0);
			vmwrite_unchecked(GUEST_DR7, 0x400);
			// Segment Registers
			macro_rules! vmcs_write_seg {
				($name:tt,$sel:literal,$ar:expr,$lim:literal,$base:literal) => {
					paste! {
						vmwrite_unchecked([<GUEST_ $name:upper _SELECTOR>],$sel as usize);
						vmwrite_unchecked([<GUEST_ $name:upper _ACCESS_RIGHTS>],$ar as usize);
						vmwrite_unchecked([<GUEST_ $name:upper _LIMIT>],$lim as usize);
						vmwrite_unchecked([<GUEST_ $name:upper _BASE>],$base);
					}
				};
			}
			let mut ar = SegmentAccessRights::new();
			ar.set_segment_type(SegmentFlags::CODE_EXECUTE_READ_ACCESSED as u32);
			ar.set_descriptor_type(true);
			ar.set_present(true);
			vmcs_write_seg!(cs, 0xF000, ar.into_bits(), 0xFFFF, 0xFFFF0000);
			ar.set_segment_type(SegmentFlags::DATA_READ_WRITE_ACCESSED as u32);
			vmcs_write_seg!(ds, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(es, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(fs, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(gs, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(ss, 0, ar.into_bits(), 0xFFFF, 0);
			// LDTR & TR
			ar.set_segment_type(SegmentFlags::LDT as u32);
			ar.set_descriptor_type(false);
			vmcs_write_seg!(ldtr, 0, ar.into_bits(), 0xFFFF, 0);
			ar.set_segment_type(SegmentFlags::BUSY_TSS_16BIT as u32);
			vmcs_write_seg!(tr, 0, ar.into_bits(), 0xFFFF, 0);
			// IDTR & GDTR
			vmwrite_unchecked(GUEST_GDTR_BASE, 0);
			vmwrite_unchecked(GUEST_IDTR_BASE, 0);
			vmwrite_unchecked(GUEST_GDTR_LIMIT, 0xFFFF);
			vmwrite_unchecked(GUEST_IDTR_LIMIT, 0xFFFF);
			// VM-Entry Controls: Guest is definitely not in IA-32e mode.
			let mut entry_ctrl = VmxEntryControls::from_bits(vmread_unchecked(VMENTRY_CONTROLS) as u32);
			entry_ctrl.set_ia32e_mode_guest(false);
			vmwrite_unchecked(VMENTRY_CONTROLS, entry_ctrl.into_bits() as usize);
		}
	}

	/// ## `take_nmi` method
	/// Handle an NMI taken by this processor. NMIs not sent by NoirVisor are held pending for the guest.
	pub(super) fn take_nmi(&mut self)
	{
		let kicked = self.ipi_mailbox().is_some_and(|mb| mb.take_kick());
		if !kicked
		{
			self.nmi_pending = true;
		}
	}

	/// ## `handle_nmi_exit` method
	/// Handle the VM-Exit due to an NMI.
	pub(super) fn handle_nmi_exit(&mut self, int_info: VmxExitInterruptionInformation)
	{
		if int_info.nmi_unblocking_due_to_iret()
		{
			// The NMI arrived while the guest's `iret` instruction unblocked virtual NMIs. Keep them blocked.
			self.cached_ctxt.get_interruptibility_mut().set_blocking_by_nmi(true);
		}
		// The NMI might arrive during event delivery. Re-inject that event.
		self.handle_idt_vectoring();
		// NMIs remain blocked after a VM-Exit due to NMI. Unblock them, or NoirVisor would never receive NMIs again.
		// If another NMI is pending, the host's NMI handler takes it right here.
		unsafe {
			nvc_vt_unblock_nmi();
		}
		self.take_nmi();
	}

	/// ## `accept_nmi` method
	/// Record an NMI taken by the host. This method is called from the host's NMI handler.
	///
	/// The NMI might interrupt the code right before VM-Entry, where nobody would check the record. Therefore, set
	/// "NMI-window exiting" in the VMCS so that the processor leaves the guest right after VM-Entry. \
	/// The cached controls are not touched since the NMI might interrupt an update of them.
	pub(super) fn accept_nmi(vcpu: *mut core::ffi::c_void)
	{
		let vcpu: &Self = unsafe { &*vcpu.cast() };
		vcpu.host_nmi.store(true, Ordering::Release);
		let mut proc_ctrl = VmxPrimaryProcessorControls::from_bits(vmread32(PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS).unwrap());
		proc_ctrl.set_nmi_window_exiting(true);
		vmwrite32(PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, proc_ctrl.into_bits());
	}

	/// ## `take_host_nmi` method
	/// Take the NMI recorded by the host's NMI handler, if any. Call this method before flushing the cached context.
	pub(super) fn take_host_nmi(&mut self)
	{
		if self.host_nmi.swap(false, Ordering::Acquire)
		{
			self.take_nmi();
		}
	}

	/// ## `recheck_host_nmi` method
	/// Call this method after flushing the cached context, which may have overwritten the "NMI-window exiting" set by
	/// the host's NMI handler. If an NMI arrived after `take_host_nmi`, restore "NMI-window exiting" so that the NMI is
	/// handled right after VM-Entry. NMIs arriving after this method set "NMI-window exiting" by themselves.
	pub(super) fn recheck_host_nmi(&mut self)
	{
		if self.host_nmi.load(Ordering::Acquire)
		{
			self.cached_ctxt.request_nmi_window_now();
		}
	}

	/// ## `handle_nmi_window` method
	/// Handles the NMI-window exit. The pending NMI is delivered at the end of the VM-Exit.
	pub(super) fn handle_nmi_window(&mut self, _context: &mut VtStackTop)
	{
		self.cached_ctxt.get_proc_ctrl1_mut().set_nmi_window_exiting(false);
	}

	/// ## `deliver_pending_nmi` method
	/// Inject the pending NMI into the guest if the guest can take it. Otherwise, wait for the NMI-window.
	pub(super) fn deliver_pending_nmi(&mut self)
	{
		if !self.nmi_pending
		{
			return;
		}
		// Only one event can be injected per VM-Entry.
		let injecting = VmxEntryInterruptionInformation::from_bits(vmread32(VMENTRY_INTERRUPTION_INFORMATION_FIELD).unwrap()).valid();
		let interruptibility = self.cached_ctxt.interruptibility();
		if injecting
			|| interruptibility.blocking_by_nmi()
			|| interruptibility.blocking_by_sti()
			|| interruptibility.blocking_by_mov_ss()
			|| vmread32(GUEST_ACTIVITY_STATE).unwrap() > ActivityState::HLT
		{
			self.cached_ctxt.get_proc_ctrl1_mut().set_nmi_window_exiting(true);
			return;
		}
		// The injected NMI wakes the guest from HLT state.
		vmwrite32(GUEST_ACTIVITY_STATE, ActivityState::ACTIVE);
		unsafe {
			inject_event(NMI_INTERRUPT, EventType::NonMaskableInterrupt, None, true, 0);
		}
		self.nmi_pending = false;
	}
}
