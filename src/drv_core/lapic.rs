/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file is the driver of the physical Local APIC of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::sync::atomic::{AtomicPtr, AtomicU64, Ordering};

use crate::xpf_core::{
	asm::{
		io::{mmio_read, mmio_write},
		msr::{rdmsr, wrmsr},
	},
	x86::{
		apic::*,
		cpuid::{CpuidLeaf, ProcessorCapabilityParameters, StandardProcessorFeatureIdentifiers},
		msr::{MSR_APIC_BASE, MSR_X2APIC_ICR, MSR_X2APIC_ID, MSR_X2APIC_MSR_START, MSR_X2APIC_SELF_IPI},
	},
};

/// ## `LocalApicOps` structure
/// Register accessors of a Local APIC mode. All registers are addressed by their xAPIC MMIO offsets.
struct LocalApicOps
{
	read: fn(usize) -> u32,
	write: fn(usize, u32),
	self_ipi: fn(u8),
	nmi_self: fn(),
	id: fn() -> Option<u32>,
}

/// The Local APIC is globally disabled. Only the 8259 PIC can deliver interrupts. \
/// Reads return zero and writes are discarded.
static DISABLED_OPS: LocalApicOps = LocalApicOps { read: |_| 0, write: |_, _| {}, self_ipi: |_| {}, nmi_self: || {}, id: || None };
/// The Local APIC is in xAPIC mode. Registers are accessed through MMIO.
static XAPIC_OPS: LocalApicOps =
	LocalApicOps { read: xapic_read, write: xapic_write, self_ipi: xapic_self_ipi, nmi_self: xapic_nmi_self, id: xapic_id };
/// The Local APIC is in x2APIC mode. Registers are accessed through MSRs.
static X2APIC_OPS: LocalApicOps =
	LocalApicOps { read: x2apic_read, write: x2apic_write, self_ipi: x2apic_self_ipi, nmi_self: x2apic_nmi_self, id: x2apic_id };

/// Accessors of the current Local APIC mode.
///
/// The mode is global rather than per-processor. Operating systems switch the BSP to x2APIC mode
/// before they start the APs, so processors don't run in different modes in the meantime.
static LAPIC_OPS: AtomicPtr<LocalApicOps> = AtomicPtr::new((&raw const DISABLED_OPS).cast_mut());
/// The base address of the xAPIC MMIO page. The page is accessed through identity mapping.
static XAPIC_BASE: AtomicU64 = AtomicU64::new(0);

fn xapic_read(offset: usize) -> u32
{
	unsafe { mmio_read((XAPIC_BASE.load(Ordering::Relaxed) as usize + offset) as *const u32) }
}

fn xapic_write(offset: usize, value: u32)
{
	unsafe { mmio_write((XAPIC_BASE.load(Ordering::Relaxed) as usize + offset) as *mut u32, value) }
}

fn xapic_self_ipi(vector: u8)
{
	// Destination is ignored with "Self" shorthand, so ICR-High is left untouched.
	let icr_lo = ApicIcrLo::new().with_vector(vector).with_destination_shorthand(ApicX2Icr::DSH_SELF);
	xapic_write(APIC_OFFSET_ICR_LO, icr_lo.into_bits());
}

fn xapic_nmi_self()
{
	// "Self" shorthand only supports fixed interrupts. Address the current processor by its APIC ID.
	let apic_id = (xapic_read(APIC_OFFSET_ID) >> 24) as u8;
	xapic_write(APIC_OFFSET_ICR_HI, ApicIcrHi::new().with_destination(apic_id).into_bits());
	let icr_lo = ApicIcrLo::new().with_message_type(ApicX2Icr::MESSAGE_TYPE_NMI).with_level(true);
	xapic_write(APIC_OFFSET_ICR_LO, icr_lo.into_bits());
}

fn xapic_id() -> Option<u32>
{
	Some(xapic_read(APIC_OFFSET_ID) >> 24)
}

fn x2apic_read(offset: usize) -> u32
{
	rdmsr(MSR_X2APIC_MSR_START + (offset >> 4) as u32) as u32
}

fn x2apic_write(offset: usize, value: u32)
{
	wrmsr(MSR_X2APIC_MSR_START + (offset >> 4) as u32, value as u64)
}

fn x2apic_self_ipi(vector: u8)
{
	wrmsr(MSR_X2APIC_SELF_IPI, vector as u64)
}

fn x2apic_id() -> Option<u32>
{
	Some(rdmsr(MSR_X2APIC_ID) as u32)
}

fn x2apic_nmi_self()
{
	// "Self" shorthand only supports fixed interrupts. Address the current processor by its x2APIC ID.
	let apic_id = rdmsr(MSR_X2APIC_ID) as u32;
	let icr = ApicX2Icr::new().with_message_type(ApicX2Icr::MESSAGE_TYPE_NMI).with_level(true).with_destination(apic_id);
	wrmsr(MSR_X2APIC_ICR, icr.into_bits())
}

#[inline(always)]
fn lapic_ops() -> &'static LocalApicOps
{
	unsafe { &*LAPIC_OPS.load(Ordering::Relaxed) }
}

/// Test the bit of `vector` in a 256-bit register array (ISR, TMR or IRR).
fn lapic_test_vector(base: usize, vector: u8) -> bool
{
	let reg = (lapic_ops().read)(apic_vector_register_offset(base, vector));
	(reg >> (vector & 31)) & 1 != 0
}

/// ## `lapic_update_mode` function
/// Select the accessors of the Local APIC according to the value of `IA32_APIC_BASE` MSR. \
/// Call this function on initialization and whenever `IA32_APIC_BASE` MSR is changed.
pub fn lapic_update_mode(apic_base: u64)
{
	let bar = ApicBar::from_bits(apic_base);
	let ops = match (bar.enable(), bar.x2apic())
	{
		(true, true) => &raw const X2APIC_OPS,
		(true, false) => &raw const XAPIC_OPS,
		_ => &raw const DISABLED_OPS,
	};
	XAPIC_BASE.store(bar.bar() << 12, Ordering::Relaxed);
	LAPIC_OPS.store(ops.cast_mut(), Ordering::Relaxed);
}

/// ## `lapic_write_base` function
/// Write `value` to `IA32_APIC_BASE` MSR on behalf of the guest, and update the accessors accordingly. \
/// Returns `false` without writing the MSR if the write would cause a #GP.
pub fn lapic_write_base(value: u64) -> bool
{
	let new = ApicBar::from_bits(value);
	let old = ApicBar::from_bits(rdmsr(MSR_APIC_BASE));
	// Bits 0-7, bit 9 and bits beyond MAXPHYADDR are reserved.
	let pa_width = ProcessorCapabilityParameters::cpuid().phys_addr_size() as u32;
	let reserved = 0x2FF | u64::MAX.checked_shl(pa_width).unwrap_or(0);
	if value & reserved != 0
	{
		return false;
	}
	// x2APIC mode can't be enabled if x2APIC isn't supported.
	if new.x2apic() && !StandardProcessorFeatureIdentifiers::cpuid().x2apic()
	{
		return false;
	}
	// See "x2APIC State Transitions" in Intel SDM. These transitions are invalid:
	let invalid = match (old.enable(), old.x2apic(), new.enable(), new.x2apic())
	{
		// Enabling x2APIC while disabling the Local APIC.
		(_, _, false, true) => true,
		// From x2APIC mode to xAPIC mode.
		(true, true, true, false) => true,
		// From disabled state to x2APIC mode.
		(false, _, true, true) => true,
		_ => false,
	};
	if invalid
	{
		return false;
	}
	wrmsr(MSR_APIC_BASE, value);
	lapic_update_mode(value);
	true
}

/// ## `lapic_is_in_service` function
/// Check whether `vector` is in-service.
pub fn lapic_is_in_service(vector: u8) -> bool
{
	lapic_test_vector(APIC_OFFSET_ISR, vector)
}

/// ## `lapic_is_level_triggered` function
/// Check whether `vector` was accepted as level-triggered.
pub fn lapic_is_level_triggered(vector: u8) -> bool
{
	lapic_test_vector(APIC_OFFSET_TMR, vector)
}

/// ## `lapic_spurious_vector` function
/// Get the spurious-interrupt vector.
pub fn lapic_spurious_vector() -> u8
{
	ApicSvr::from_bits((lapic_ops().read)(APIC_OFFSET_SVR)).vector()
}

/// ## `lapic_is_eoi_broadcast_suppressed` function
/// Check whether EOI-broadcast to I/O APICs is suppressed.
pub fn lapic_is_eoi_broadcast_suppressed() -> bool
{
	ApicSvr::from_bits((lapic_ops().read)(APIC_OFFSET_SVR)).suppress_eoi_broadcast()
}

/// ## `lapic_eoi` function
/// Signal End-of-Interrupt. This clears the highest-priority in-service vector.
pub fn lapic_eoi()
{
	(lapic_ops().write)(APIC_OFFSET_EOI, 0)
}

/// ## `lapic_send_nmi_to_self` function
/// Send an NMI to the current processor.
pub fn lapic_send_nmi_to_self()
{
	(lapic_ops().nmi_self)()
}

/// ## `lapic_is_x2apic_mode` function
/// Check whether the Local APIC is in x2APIC mode.
pub fn lapic_is_x2apic_mode() -> bool
{
	LAPIC_OPS.load(Ordering::Relaxed).cast_const() == &raw const X2APIC_OPS
}

/// ## `lapic_write_icr` function
/// Write the Interrupt Command Register in the format of the current Local APIC mode. \
/// In xAPIC mode, the destination is located at bits 24-31 of `icr_hi`. In x2APIC mode, `icr_hi` is the destination.
pub fn lapic_write_icr(icr_lo: u32, icr_hi: u32)
{
	if lapic_is_x2apic_mode()
	{
		wrmsr(MSR_X2APIC_ICR, ((icr_hi as u64) << 32) | icr_lo as u64)
	}
	else
	{
		let ops = lapic_ops();
		(ops.write)(APIC_OFFSET_ICR_HI, icr_hi);
		(ops.write)(APIC_OFFSET_ICR_LO, icr_lo);
	}
}

/// ## `lapic_send_nmi` function
/// Send an NMI to the processor whose APIC ID (or x2APIC ID in x2APIC mode) is `destination`.
pub fn lapic_send_nmi(destination: u32)
{
	let icr_lo = ApicIcrLo::new().with_message_type(ApicX2Icr::MESSAGE_TYPE_NMI).with_level(true).into_bits();
	if lapic_is_x2apic_mode()
	{
		lapic_write_icr(icr_lo, destination)
	}
	else
	{
		// The ICR-High may hold a destination written by the guest. Preserve it.
		let ops = lapic_ops();
		let old_hi = (ops.read)(APIC_OFFSET_ICR_HI);
		lapic_write_icr(icr_lo, ApicIcrHi::new().with_destination(destination as u8).into_bits());
		(ops.write)(APIC_OFFSET_ICR_HI, old_hi);
	}
}

/// ## `lapic_logical_destination` function
/// Get the Logical Destination Register and the Destination Format Register in xAPIC mode. \
/// Returns `None` if the Local APIC isn't in xAPIC mode.
pub fn lapic_logical_destination() -> Option<(u32, u32)>
{
	if LAPIC_OPS.load(Ordering::Relaxed).cast_const() == &raw const XAPIC_OPS
	{
		Some((xapic_read(APIC_OFFSET_LDR), xapic_read(APIC_OFFSET_DFR)))
	}
	else
	{
		None
	}
}

/// ## `lapic_reset_for_init` function
/// Put the Local APIC of the current processor into the state right after an INIT signal. \
/// The APIC ID, the mode and the logical destination are preserved.
///
/// Interrupts held in-service are dismissed by EOI. Level-triggered interrupts may be resent by the I/O APIC.
pub fn lapic_reset_for_init()
{
	let ops = lapic_ops();
	(ops.write)(APIC_OFFSET_TPR, 0);
	// EOI always dismisses the highest in-service vector. There are 256 vectors at most.
	for _ in 0..256
	{
		let in_service = (0..8).any(|i| (ops.read)(APIC_OFFSET_ISR + (i << 4)) != 0);
		if !in_service
		{
			break;
		}
		(ops.write)(APIC_OFFSET_EOI, 0);
	}
	// Mask all LVT entries and stop the timer.
	// The Thermal and Performance Counter LVT entries are implemented only if the version register reports them.
	const LVT_MASKED: u32 = 1 << 16;
	let max_lvt = ((ops.read)(APIC_OFFSET_VERSION) >> 16) & 0xFF;
	for offset in [APIC_OFFSET_LVT_TIMER, APIC_OFFSET_LVT_LINT0, APIC_OFFSET_LVT_LINT1, APIC_OFFSET_LVT_ERROR]
	{
		(ops.write)(offset, LVT_MASKED);
	}
	if max_lvt >= 4
	{
		(ops.write)(APIC_OFFSET_LVT_PERF, LVT_MASKED);
	}
	if max_lvt >= 5
	{
		(ops.write)(APIC_OFFSET_LVT_THERMAL, LVT_MASKED);
	}
	(ops.write)(APIC_OFFSET_TIMER_INITIAL_COUNT, 0);
	// Software-disable the Local APIC. INIT, SIPI and NMI are still accepted.
	(ops.write)(APIC_OFFSET_SVR, ApicSvr::new().with_vector(0xFF).into_bits());
}

/// ## `lapic_id` function
/// Get the APIC ID (or x2APIC ID in x2APIC mode) of the current processor. \
/// Returns `None` if the Local APIC is globally disabled.
pub fn lapic_id() -> Option<u32>
{
	(lapic_ops().id)()
}

/// ## `lapic_self_ipi` function
/// Send a fixed, edge-triggered interrupt with `vector` to the current processor.
pub fn lapic_self_ipi(vector: u8)
{
	(lapic_ops().self_ipi)(vector)
}
