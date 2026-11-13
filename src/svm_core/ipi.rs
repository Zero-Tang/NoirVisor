/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file emulates INIT and Startup-IPI, and virtualizes NMI in AMD-V of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

//! # INIT and Startup-IPI emulation
//! AMD-V can't keep a physical INIT signal from resetting the processor, unless `VM_CR.R_INIT` redirects it to #SX.
//! Some platforms (e.g.: QEMU in both TCG and KVM) don't emulate `R_INIT` and #SX, though.
//!
//! NoirVisor intercepts the guest's writes to the ICR instead. INIT and Startup-IPI aimed at subverted processors
//! are not sent. They are posted to the mailbox of the target processor, and NoirVisor kicks the target with an NMI.
//! An NMI is used rather than a fixed interrupt because the target is usually parked with `cli` and `hlt`,
//! and only an NMI can take it out of the guest in such state.
//!
//! As a result, NoirVisor intercepts NMIs. NMIs not sent by NoirVisor are virtualized and injected into the guest.

use core::{
	hint::{cold_path, spin_loop},
	sync::atomic::{AtomicBool, AtomicU32, Ordering},
};

use super::*;
use crate::{
	drv_core::lapic::*,
	xpf_core::{
		asm::io::{mmio_read, mmio_write},
		ioflt::IoRegionOps,
		x86::{apic::*, crdr::*, descriptors::SegmentFlags, rflags::Rflags},
	},
};

/// The processor is not subverted yet. INIT and Startup-IPI are delivered by the hardware.
const IPI_STATE_OFFLINE: u32 = 0;
/// The processor is subverted and running the guest.
const IPI_STATE_ACTIVE: u32 = 1;
/// An INIT signal is posted to the processor.
const IPI_STATE_INIT_PENDING: u32 = 2;
/// The processor has emulated the INIT signal and is waiting for a Startup-IPI.
const IPI_STATE_WAIT_FOR_SIPI: u32 = 3;
/// A Startup-IPI is posted to the processor. Its vector is located at bits 8-15 of the state.
const IPI_STATE_SIPI_RECEIVED: u32 = 4;

/// ## `SvmIpiMailbox` structure
/// INIT and Startup-IPI posted to a subverted processor. Other processors access it, so all fields are atomic.
pub struct SvmIpiMailbox
{
	/// Bits 0-7 are the state. Bits 8-15 are the vector of the received Startup-IPI.
	state: AtomicU32,
	/// Set if NoirVisor sent an NMI to this processor in order to deliver the INIT signal.
	nmi_kick: AtomicBool,
	xapic_id: AtomicU32,
	x2apic_id: AtomicU32,
	/// The Logical Destination Register in xAPIC mode.
	ldr: AtomicU32,
	/// The Destination Format Register in xAPIC mode.
	dfr: AtomicU32,
}

impl SvmIpiMailbox
{
	pub const fn new() -> Self
	{
		Self {
			state: AtomicU32::new(IPI_STATE_OFFLINE),
			nmi_kick: AtomicBool::new(false),
			xapic_id: AtomicU32::new(0),
			x2apic_id: AtomicU32::new(0),
			ldr: AtomicU32::new(0),
			dfr: AtomicU32::new(u32::MAX),
		}
	}

	/// ## `go_online` method
	/// Record the APIC IDs of the processor, and start accepting INIT and Startup-IPI. \
	/// Call this method on the processor itself once it is subverted.
	pub fn go_online(&self, xapic_id: u8, x2apic_id: u32)
	{
		self.xapic_id.store(xapic_id as u32, Ordering::Relaxed);
		self.x2apic_id.store(x2apic_id, Ordering::Relaxed);
		if let Some((ldr, dfr)) = lapic_logical_destination()
		{
			self.ldr.store(ldr, Ordering::Relaxed);
			self.dfr.store(dfr, Ordering::Relaxed);
		}
		self.state.store(IPI_STATE_ACTIVE, Ordering::Release);
	}

	fn is_online(&self) -> bool
	{
		self.state.load(Ordering::Acquire) & 0xFF != IPI_STATE_OFFLINE
	}

	/// ## `has_init` method
	/// Check whether an INIT signal is posted to this processor and not yet completed.
	#[inline(always)]
	fn has_init(&self) -> bool
	{
		self.state.load(Ordering::Acquire) & 0xFF > IPI_STATE_ACTIVE
	}

	/// ## `matches` method
	/// Check whether this processor is the destination of an IPI.
	fn matches(&self, destination: u32, logical: bool, x2apic: bool) -> bool
	{
		if x2apic
		{
			let id = self.x2apic_id.load(Ordering::Relaxed);
			if destination == u32::MAX
			{
				true
			}
			else if logical
			{
				// The x2APIC logical ID is derived from the x2APIC ID: cluster ID at bits 16-31 and a bit at bits 0-15.
				(destination >> 16) == (id >> 4) && destination & (1 << (id & 0xF)) != 0
			}
			else
			{
				destination == id
			}
		}
		else
		{
			let destination = destination & 0xFF;
			if destination == 0xFF
			{
				true
			}
			else if logical
			{
				let logical_id = self.ldr.load(Ordering::Relaxed) >> 24;
				if (self.dfr.load(Ordering::Relaxed) >> 28) == 0xF
				{
					// Flat Model
					destination & logical_id != 0
				}
				else
				{
					// Cluster Model
					(destination >> 4) == (logical_id >> 4) && destination & logical_id & 0xF != 0
				}
			}
			else
			{
				destination == self.xapic_id.load(Ordering::Relaxed)
			}
		}
	}

	/// ## `post_init` method
	/// Post an INIT signal. Returns `false` if the processor is not subverted.
	pub(super) fn post_init(&self) -> bool
	{
		self.state
			.try_update(Ordering::AcqRel, Ordering::Acquire, |s| {
				if s & 0xFF == IPI_STATE_OFFLINE { None } else { Some(IPI_STATE_INIT_PENDING) }
			})
			.is_ok()
	}

	/// ## `post_sipi` method
	/// Post a Startup-IPI. Returns `false` if the processor is not waiting for one. Such Startup-IPI is discarded.
	fn post_sipi(&self, vector: u8) -> bool
	{
		self.state
			.try_update(Ordering::AcqRel, Ordering::Acquire, |s| match s & 0xFF
			{
				IPI_STATE_INIT_PENDING | IPI_STATE_WAIT_FOR_SIPI => Some(IPI_STATE_SIPI_RECEIVED | (vector as u32) << 8),
				_ => None,
			})
			.is_ok()
	}

	/// ## `wait_for_sipi` method
	/// Spin until a Startup-IPI is received, and return its vector.
	fn wait_for_sipi(&self) -> u8
	{
		loop
		{
			let s = self.state.load(Ordering::Acquire);
			let next = match s & 0xFF
			{
				IPI_STATE_SIPI_RECEIVED => IPI_STATE_ACTIVE,
				// Another INIT signal. The processor is already in the state right after INIT.
				IPI_STATE_INIT_PENDING => IPI_STATE_WAIT_FOR_SIPI,
				_ =>
				{
					spin_loop();
					continue;
				}
			};
			if self.state.compare_exchange(s, next, Ordering::AcqRel, Ordering::Acquire).is_ok() && next == IPI_STATE_ACTIVE
			{
				return (s >> 8) as u8;
			}
		}
	}

	/// ## `kick` method
	/// Send an NMI to this processor so that it leaves the guest and handles the posted INIT signal.
	fn kick(&self)
	{
		self.nmi_kick.store(true, Ordering::Release);
		let destination =
			if lapic_is_x2apic_mode() { self.x2apic_id.load(Ordering::Relaxed) } else { self.xapic_id.load(Ordering::Relaxed) };
		lapic_send_nmi(destination);
	}

	/// ## `take_kick` method
	/// Check whether an NMI taken by this processor is sent by NoirVisor.
	///
	/// Note: NMIs are not queued. If an NMI from a device arrives while the kick is pending, they merge into one NMI,
	/// and the NMI of the device is lost. This is very unlikely.
	fn take_kick(&self) -> bool
	{
		self.nmi_kick.swap(false, Ordering::AcqRel)
	}
}

impl Default for SvmIpiMailbox
{
	fn default() -> Self
	{
		Self::new()
	}
}

/// ## `SvmLapicRegion` structure
/// Filter the writes to the xAPIC page so that INIT and Startup-IPI sent by the guest can be intercepted. \
/// Reads are not filtered. This filter must be stateless since processors access it simultaneously.
pub struct SvmLapicRegion
{
	base: u64,
}

impl SvmLapicRegion
{
	pub fn new(base: u64) -> Self
	{
		Self { base }
	}
}

impl IoRegionOps for SvmLapicRegion
{
	fn name(&self) -> &str
	{
		"Local APIC"
	}

	fn input(&mut self, address: u64, value: &mut [u8], _context: *mut c_void) -> Status
	{
		for (i, v) in value.iter_mut().enumerate()
		{
			*v = unsafe { mmio_read((address as usize + i) as *const u8) };
		}
		Status::SUCCESS
	}

	fn output(&mut self, address: u64, value: &[u8], context: *mut c_void) -> Status
	{
		let offset = (address - self.base) as usize;
		let Ok(data) = <[u8; 4]>::try_from(value)
		else
		{
			// The Local APIC only accepts aligned 32-bit accesses. Forward the others anyway.
			for (i, &v) in value.iter().enumerate()
			{
				unsafe { mmio_write((address as usize + i) as *mut u8, v) };
			}
			return Status::SUCCESS;
		};
		let data = u32::from_le_bytes(data);
		let vcpu: &mut SvmVcpu = unsafe { &mut *context.cast() };
		let hv: &SvmHypervisor = unsafe { &*vcpu.hypervisor.cast() };
		let mailbox = hv.ipi_mailboxes.get(vcpu.vcpu_id as usize);
		match offset
		{
			APIC_OFFSET_ICR_LO =>
			{
				// The guest has written the destination to the ICR-High. It's forwarded to the hardware.
				let icr_hi: u32 = unsafe { mmio_read((self.base as usize + APIC_OFFSET_ICR_HI) as *const u32) };
				if vcpu.filter_icr(data, icr_hi)
				{
					return Status::SUCCESS;
				}
			}
			APIC_OFFSET_LDR =>
			{
				if let Some(mb) = mailbox
				{
					mb.ldr.store(data, Ordering::Relaxed);
				}
			}
			APIC_OFFSET_DFR =>
			{
				if let Some(mb) = mailbox
				{
					mb.dfr.store(data, Ordering::Relaxed);
				}
			}
			_ => (),
		}
		unsafe { mmio_write(address as usize as *mut u32, data) };
		Status::SUCCESS
	}

	fn base_size(&self) -> (u64, usize)
	{
		(self.base, PAGE_SIZE)
	}

	fn forward_input(&self) -> bool
	{
		true
	}

	fn forward_output(&self) -> bool
	{
		false
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
		let lo = ApicIcrLo::from_bits(icr_lo);
		let message_type = lo.message_type();
		if message_type != ApicX2Icr::MESSAGE_TYPE_INIT && message_type != ApicX2Icr::MESSAGE_TYPE_SIPI
		{
			return false;
		}
		if message_type == ApicX2Icr::MESSAGE_TYPE_INIT && lo.trigger_mode() && !lo.level()
		{
			// INIT Level De-assert is only meaningful to ancient processors. Discard it.
			return true;
		}
		let hv: &SvmHypervisor = unsafe { &*self.hypervisor.cast() };
		let x2apic = lapic_is_x2apic_mode();
		let destination = if x2apic { icr_hi } else { icr_hi >> 24 };
		let mut emulated = false;
		let mut unsubverted = false;
		for (i, mb) in hv.ipi_mailboxes.iter().enumerate()
		{
			let is_self = i == self.vcpu_id as usize;
			let targeted = match lo.destination_shorthand()
			{
				ApicX2Icr::DSH_SELF => is_self,
				ApicX2Icr::DSH_ALL_INCLUSIVE => true,
				ApicX2Icr::DSH_ALL_EXCLUSIVE => !is_self,
				_ => mb.matches(destination, lo.destination_mode(), x2apic),
			};
			if !targeted
			{
				continue;
			}
			if !mb.is_online()
			{
				unsubverted = true;
				continue;
			}
			emulated = true;
			if message_type == ApicX2Icr::MESSAGE_TYPE_INIT
			{
				if mb.post_init()
				{
					debug!("Processor {} sent INIT signal to processor {i}!", self.vcpu_id);
					// INIT signal sent to the current processor is handled at the end of this VM-Exit.
					if !is_self
					{
						mb.kick();
					}
				}
			}
			else if mb.post_sipi(lo.vector())
			{
				debug!("Processor {} sent Startup-IPI (vector=0x{:02X}) to processor {i}!", self.vcpu_id, lo.vector());
			}
		}
		if emulated && unsubverted
		{
			warn!(
				"Processor {} sent an IPI (ICR=0x{icr_hi:08X}_{icr_lo:08X}) to both subverted and unsubverted processors! Unsubverted processors will not receive it!",
				self.vcpu_id
			);
		}
		emulated
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
