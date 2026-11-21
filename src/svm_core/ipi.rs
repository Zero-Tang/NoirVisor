/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file emulates INIT and virtualizes NMI in AMD-V of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

//! # INIT and Startup-IPI emulation
//! See [`crate::xpf_core::hv_host::x86::ipi`] for the overall design. INIT and Startup-IPI aimed at subverted processors are
//! posted to mailboxes and the target is kicked with an NMI. This file emulates the INIT signal and virtualizes NMIs.

use core::hint::cold_path;

use super::*;
use crate::{
	drv_core::lapic::*,
	xpf_core::{
		hv_host::x86::ipi::{IpiMailbox, IpiVcpu, filter_icr},
		x86::{crdr::*, descriptors::SegmentFlags, rflags::Rflags},
	},
};

impl IpiVcpu for SvmVcpu
{
	fn ipi_mailboxes(&self) -> &[IpiMailbox]
	{
		let hv: &SvmHypervisor = unsafe { &*self.hypervisor.cast() };
		&hv.ipi_mailboxes
	}

	fn ipi_index(&self) -> usize
	{
		self.vcpu_id as usize
	}
}

const DEBUG_EXCEPTION_BIT: u32 = 1 << DEBUG_FAULT_OR_TRAP;

impl SvmVcpu
{
	/// ## `filter_icr` method
	/// Filter an IPI sent by the guest. `icr_hi` is in the format of the current Local APIC mode. \
	/// Returns `true` if NoirVisor emulated the IPI, so it must not be sent. Otherwise, forward the IPI to the hardware.
	///
	/// INIT and Startup-IPI aimed at subverted processors are emulated. Other IPIs are forwarded.
	pub(super) fn filter_icr(&mut self, icr_lo: u32, icr_hi: u32) -> bool
	{
		filter_icr(self.ipi_mailboxes(), self.ipi_index(), icr_lo, icr_hi)
	}

	/// ## `process_init_signal` method
	/// Emulate the INIT signal posted to this processor, then wait for the Startup-IPI. \
	/// Call this method at the very end of VM-Exit handling, since INIT discards all pending events of the guest.
	#[inline(always)]
	pub(super) fn process_init_signal(&mut self, context: &mut SvmStackTop)
	{
		let hv: &SvmHypervisor = unsafe { &*self.hypervisor.cast() };
		let Some(mb) = hv.ipi_mailboxes.get(self.vcpu_id as usize)
		else
		{
			return;
		};
		if mb.has_init()
		{
			cold_path();
			info!("Processor {} received INIT signal!", self.vcpu_id);
			self.emulate_init(context);
			// Emulate the wait-for-SIPI state by spinning. GIF is cleared, so all events remain pending in the meantime.
			let vector = mb.wait_for_sipi();
			info!("Processor {} received Startup-IPI with vector 0x{vector:02X}!", self.vcpu_id);
			let cs = self.ref_cs_mut();
			cs.selector = (vector as u16) << 8;
			cs.base = (vector as u64) << 12;
			self.write_rip(0);
		}
	}

	/// ## `emulate_init` method
	/// Put the guest into the state right after an INIT signal.
	fn emulate_init(&mut self, context: &mut SvmStackTop)
	{
		// INIT discards all pending events of the guest.
		self.inject_event(0, EventType::ExternalInterrupt, None, false);
		self.write_guest_interrupt(InterruptControl::new());
		self.set_interrupt_window_exiting(false);
		self.held_irqs.clear();
		self.reset_nmi_state();
		let mut avic_ctrl = self.read_avic_control();
		avic_ctrl.set_v_tpr(0);
		self.write_avic_control(avic_ctrl);
		lapic_reset_for_init();
		// General-Purpose Registers
		for i in 0..16usize
		{
			context.gpr_state.write(i, 0);
		}
		context.gpr_state.rdx = self.cpuid_fms as u64;
		context.guest_xcr0 = 1;
		self.write_rsp(0);
		self.write_rip(0xFFF0);
		self.write_rflags(Rflags::from_bits(2));
		self.write_cpl(0);
		// Control Registers. INIT preserves CR0.CD and CR0.NW.
		let old_cr0 = self.read_cr0();
		let cr0 = Cr0::new().with_et(true).with_cd(old_cr0.cd()).with_nw(old_cr0.nw());
		self.write_cr0(cr0);
		self.write_cr2(0);
		self.write_cr3(0);
		// CR4 is cleared. CR4.MCE is virtualized and is always set in the guest.
		self.flags.set_mce(false);
		self.write_cr4(Cr4::new().with_mce(true));
		// INIT clears EFER to 0, but SVME bit must be set as a guest.
		self.flags.set_svme(false);
		self.write_efer(Efer::new().with_svme(true));
		// Segment Registers
		let mut attrib = SvmSegmentFlags::new()
			.with_segment_type(SegmentFlags::CODE_EXECUTE_READ_ACCESSED)
			.with_user_segment(true)
			.with_present(true);
		self.write_cs(SvmSegmentRegister { selector: 0xF000, attrib, limit: 0xFFFF, base: 0xFFFF0000 });
		attrib.set_segment_type(SegmentFlags::DATA_READ_WRITE_ACCESSED);
		let data_seg = SvmSegmentRegister { selector: 0, attrib, limit: 0xFFFF, base: 0 };
		self.write_ds(data_seg);
		self.write_es(data_seg);
		self.write_fs(data_seg);
		self.write_gs(data_seg);
		self.write_ss(data_seg);
		{
			let idtr = self.ref_idtr_mut();
			idtr.limit = 0xFFFF;
			idtr.base = 0;
		}
		{
			let gdtr = self.ref_gdtr_mut();
			gdtr.limit = 0xFFFF;
			gdtr.base = 0;
		}
		attrib.set_user_segment(false);
		attrib.set_segment_type(SegmentFlags::LDT);
		self.write_ldtr(SvmSegmentRegister { selector: 0, attrib, limit: 0xFFFF, base: 0 });
		attrib.set_segment_type(SegmentFlags::BUSY_TSS_16BIT);
		self.write_tr(SvmSegmentRegister { selector: 0, attrib, limit: 0xFFFF, base: 0 });
		// Debug Registers
		self.write_dr6(Dr6::from_bits(0xFFFF0FF0));
		self.write_dr7(Dr7::from_bits(0x400));
		// Because CR0/CR4 are changed, flush the TLBs.
		self.write_tlb_control(TLB_CONTROL_FLUSH_GUEST_TLB);
	}

	/// ## `take_nmi` method
	/// Handle an NMI taken by the host. NMIs not sent by NoirVisor are held pending for the guest.
	pub(super) fn take_nmi(&mut self)
	{
		let hv: &SvmHypervisor = unsafe { &*self.hypervisor.cast() };
		let kicked = hv.ipi_mailboxes.get(self.vcpu_id as usize).is_some_and(|mb| mb.take_kick());
		if !kicked
		{
			self.nmi_pending = true;
		}
	}

	/// ## `deliver_pending_nmi` method
	/// Inject the pending NMI into the guest if the guest can take it.
	///
	/// The processor does not track the NMI-masking of the guest for injected NMIs.
	/// NoirVisor considers NMIs masked from the injection until the `iret` instruction retires.
	pub(super) fn deliver_pending_nmi(&mut self)
	{
		if let Some(rip) = self.nmi_iret_rip
		{
			if self.read_rip() == rip
			{
				// The `iret` instruction hasn't retired. Single-step over it before delivering the pending NMI.
				if self.nmi_pending
				{
					self.enable_nmi_singlestep();
				}
				return;
			}
			// The `iret` instruction retired (or faulted).
			self.nmi_iret_rip = None;
			self.disable_nmi_singlestep();
		}
		if !self.nmi_pending || self.nmi_masked
		{
			return;
		}
		if self.read_event_injection().valid() || self.read_exit_int_info().valid()
		{
			// Another event is being delivered. Only one event can be injected per VMRUN.
			// Hold the NMI pending in the Local APIC. It will be intercepted right after that event is delivered.
			lapic_send_nmi_to_self();
			return;
		}
		self.inject_event(NMI_INTERRUPT, EventType::NonMaskableInterrupt, None, true);
		self.nmi_pending = false;
		self.nmi_masked = true;
		// Intercept the `iret` instruction to know when the guest's NMI handler finishes.
		let mut iv1 = self.read_intercept_vector1();
		iv1.set_iret(true);
		self.write_intercept_vector1(iv1);
	}

	/// ## `enable_nmi_singlestep` method
	/// Single-step the guest by setting `RFLAGS.TF` and intercepting #DB.
	fn enable_nmi_singlestep(&mut self)
	{
		if self.nmi_singlestep.is_some()
		{
			return;
		}
		let rflags = self.read_rflags();
		self.nmi_singlestep = Some(rflags.tf());
		self.write_rflags(rflags.with_tf(true).with_rf(true));
		let exceptions = self.read_intercept_exception();
		self.write_intercept_exception(exceptions | DEBUG_EXCEPTION_BIT);
	}

	/// ## `disable_nmi_singlestep` method
	/// Stop intercepting #DB. Returns `RFLAGS.TF` of the guest before single-stepping, if single-stepping was enabled.
	pub(super) fn disable_nmi_singlestep(&mut self) -> Option<bool>
	{
		let guest_tf = self.nmi_singlestep.take()?;
		let exceptions = self.read_intercept_exception();
		self.write_intercept_exception(exceptions & !DEBUG_EXCEPTION_BIT);
		Some(guest_tf)
	}

	/// ## `reset_nmi_state` method
	/// Forget all pending NMIs and the NMI-masking of the guest.
	fn reset_nmi_state(&mut self)
	{
		self.nmi_pending = false;
		self.nmi_masked = false;
		self.nmi_iret_rip = None;
		self.disable_nmi_singlestep();
		let mut iv1 = self.read_intercept_vector1();
		iv1.set_iret(false);
		self.write_intercept_vector1(iv1);
	}
}
