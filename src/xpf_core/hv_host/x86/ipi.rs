/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file emulates INIT and Startup-IPI for subverted processors of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

//! # INIT and Startup-IPI emulation
//! A subverted processor can't receive INIT and Startup-IPI from the hardware reliably:
//! - AMD-V can't keep a physical INIT signal from resetting the processor, unless `VM_CR.R_INIT` redirects it to #SX.
//!   Some platforms (e.g.: QEMU in both TCG and KVM) don't emulate `R_INIT` and #SX, though.
//! - Intel VT-x blocks INIT signals in VMX root operation, and discards Startup-IPIs unless the processor is in the
//!   wait-for-SIPI state of VMX non-root operation. A Startup-IPI that arrives while the hypervisor is still handling the
//!   INIT signal is lost.
//!
//! NoirVisor intercepts the guest's writes to the ICR instead. INIT and Startup-IPI aimed at subverted processors
//! are not sent. They are posted to the mailbox of the target processor, and NoirVisor kicks the target with an NMI.
//! An NMI is used rather than a fixed interrupt because the target is usually parked with `cli` and `hlt`,
//! and only an NMI can take it out of the guest in such state.
//!
//! As a result, NoirVisor intercepts NMIs. NMIs not sent by NoirVisor are virtualized and injected into the guest.

use core::{
	ffi::c_void,
	hint::spin_loop,
	marker::PhantomData,
	sync::atomic::{AtomicBool, AtomicU32, Ordering},
};

use log::*;
use nvcvm::status::Status;

use crate::{
	drv_core::lapic::*,
	xpf_core::{
		asm::io::{mmio_read, mmio_write},
		ioflt::IoRegionOps,
		nvbdk::PAGE_4KB_SIZE,
		x86::apic::*,
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

/// ## `IpiMailbox` structure
/// INIT and Startup-IPI posted to a subverted processor. Other processors access it, so all fields are atomic.
pub struct IpiMailbox
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

impl IpiMailbox
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
	pub fn has_init(&self) -> bool
	{
		self.state.load(Ordering::Acquire) & 0xFF > IPI_STATE_ACTIVE
	}

	/// ## `set_ldr` method
	/// Track the Logical Destination Register written by the guest in xAPIC mode.
	pub fn set_ldr(&self, ldr: u32)
	{
		self.ldr.store(ldr, Ordering::Relaxed);
	}

	/// ## `set_dfr` method
	/// Track the Destination Format Register written by the guest in xAPIC mode.
	pub fn set_dfr(&self, dfr: u32)
	{
		self.dfr.store(dfr, Ordering::Relaxed);
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
	pub fn post_init(&self) -> bool
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
	pub fn wait_for_sipi(&self) -> u8
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
	pub fn take_kick(&self) -> bool
	{
		self.nmi_kick.swap(false, Ordering::AcqRel)
	}
}

impl Default for IpiMailbox
{
	fn default() -> Self
	{
		Self::new()
	}
}

/// ## `filter_icr` function
/// Filter an IPI sent by the guest on processor `sender`. `icr_hi` is in the format of the current Local APIC mode. \
/// Returns `true` if NoirVisor emulated the IPI, so it must not be sent. Otherwise, forward the IPI to the hardware.
///
/// INIT and Startup-IPI aimed at subverted processors are emulated. Other IPIs are forwarded. \
/// An INIT signal sent to the sender itself is only posted. The caller must handle it at the end of the VM-Exit.
pub fn filter_icr(mailboxes: &[IpiMailbox], sender: usize, icr_lo: u32, icr_hi: u32) -> bool
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
	let x2apic = lapic_is_x2apic_mode();
	let destination = if x2apic { icr_hi } else { icr_hi >> 24 };
	let mut emulated = false;
	let mut unsubverted = false;
	for (i, mb) in mailboxes.iter().enumerate()
	{
		let is_self = i == sender;
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
				debug!("Processor {sender} sent INIT signal to processor {i}!");
				// INIT signal sent to the current processor is handled at the end of this VM-Exit.
				if !is_self
				{
					mb.kick();
				}
			}
		}
		else if mb.post_sipi(lo.vector())
		{
			debug!("Processor {sender} sent Startup-IPI (vector=0x{:02X}) to processor {i}!", lo.vector());
		}
	}
	if emulated && unsubverted
	{
		warn!(
			"Processor {sender} sent an IPI (ICR=0x{icr_hi:08X}_{icr_lo:08X}) to both subverted and unsubverted processors! Unsubverted processors will not receive it!"
		);
	}
	emulated
}

/// ## `IpiVcpu` trait
/// A vCPU of the subverted host that sends IPIs through the xAPIC page.
pub trait IpiVcpu
{
	/// ## `ipi_mailboxes` method
	/// Returns the mailboxes of all processors.
	fn ipi_mailboxes(&self) -> &[IpiMailbox];
	/// ## `ipi_index` method
	/// Returns the index of this processor in the mailboxes.
	fn ipi_index(&self) -> usize;
}

/// ## `LapicRegion` structure
/// Filter the writes to the xAPIC page so that INIT and Startup-IPI sent by the guest can be intercepted. \
/// Reads are not filtered. This filter must be stateless since processors access it simultaneously. \
/// The context passed to the filter must point to the vCPU of type `V`.
pub struct LapicRegion<V: IpiVcpu>
{
	base: u64,
	_vcpu: PhantomData<V>,
}

impl<V: IpiVcpu> LapicRegion<V>
{
	pub fn new(base: u64) -> Self
	{
		Self { base, _vcpu: PhantomData }
	}
}

impl<V: IpiVcpu> IoRegionOps for LapicRegion<V>
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
		let vcpu: &V = unsafe { &*context.cast() };
		let mailboxes = vcpu.ipi_mailboxes();
		let sender = vcpu.ipi_index();
		match offset
		{
			APIC_OFFSET_ICR_LO =>
			{
				// The guest has written the destination to the ICR-High. It's forwarded to the hardware.
				let icr_hi: u32 = unsafe { mmio_read((self.base as usize + APIC_OFFSET_ICR_HI) as *const u32) };
				if filter_icr(mailboxes, sender, data, icr_hi)
				{
					return Status::SUCCESS;
				}
			}
			APIC_OFFSET_LDR =>
			{
				if let Some(mb) = mailboxes.get(sender)
				{
					mb.set_ldr(data);
				}
			}
			APIC_OFFSET_DFR =>
			{
				if let Some(mb) = mailboxes.get(sender)
				{
					mb.set_dfr(data);
				}
			}
			_ => (),
		}
		unsafe { mmio_write(address as usize as *mut u32, data) };
		Status::SUCCESS
	}

	fn base_size(&self) -> (u64, usize)
	{
		(self.base, PAGE_4KB_SIZE)
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
