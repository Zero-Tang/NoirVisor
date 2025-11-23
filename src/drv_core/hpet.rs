/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the High-Precision Event Timer Driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ptr::null_mut,sync::atomic::{AtomicU64, AtomicU32, Ordering}};

use bitfield_struct::bitfield;
use log::*;

use nvcvm::status::Status;

use crate::xpf_core::asm::io::mmio_read;
use super::acpi::{search_acpi_table,tables::{AcpiSystemDescriptorSignature,HighPrecisionEventTimerTable}};

#[allow(dead_code)]
const fn hpet_timer_config_capability_offset(n:usize)->usize
{
	0x100+(n<<5)
}

#[allow(dead_code)]
const fn hpet_timer_comparator_value_offset(n:usize)->usize
{
	0x100+(n<<5)+0x8
}

#[allow(dead_code)]
const fn hpet_timer_fsb_interrupt_value_offset(n:usize)->usize
{
	0x100+(n<<5)+0x10
}

#[allow(dead_code)]
const fn hpet_timer_fsb_interrupt_addressvalue_offset(n:usize)->usize
{
	0x100+(n<<5)+0x14
}

#[allow(dead_code)]
const HPET_GENERAL_CAPABILITY_ID:usize=0x0;
const HPET_GENERAL_COUNTER_CLOCK_PERIOD:usize=0x4;
#[allow(dead_code)]
const HPET_GENERAL_CONFIGURATION:usize=0x10;
#[allow(dead_code)]
const HPET_GENREAL_INTERRUPT_STATUS:usize=0x20;
const HPET_MAIN_COUNTER_VALUE:usize=0xF0;

#[bitfield(u32)] pub struct HpetGeneralCapabilityIdRegister
{
	pub revision_id:u8,
	#[bits(5)] pub timer_count:usize,
	pub counter_size:bool,
	reserved:bool,
	pub legacy_replacement_route:bool,
	pub vendor_id:u16
}

#[bitfield(u64)] pub struct HpetGeneralConfigRegister
{
	pub enable:bool,
	pub legacy_replacement_route:bool,
	#[bits(6)] rsvd0:u64,
	pub reserved_non_os:u8,
	#[bits(48)] rsvd1:u64
}

#[bitfield(u64)] pub struct HpetTimerConfigRegister
{
	rsvd0:bool,
	pub int_type:bool,
	pub int_enable:bool,
	pub timer_type:bool,
	pub can_be_periodic:bool,
	pub timer_size:bool,
	pub timer_value_set:bool,
	rsvd1:bool,
	pub timer_32bit_mode:bool,
	#[bits(5)] pub int_route:usize,
	pub fsb_enable:bool,
	pub fsb_delivery:bool,
	rsvd2:u16,
	pub route_cap:u32
}

static HPET_BASE_ADDRESS:AtomicU64=AtomicU64::new(0);
static HPET_PERIOD:AtomicU32=AtomicU32::new(0);

#[inline(always)] fn hpet_read_register<T:Sized>(offset:usize)->T
{
	unsafe{mmio_read((HPET_BASE_ADDRESS.load(Ordering::Relaxed)+offset as u64) as *const T)}
}

pub fn hpet_read_counter()->u64
{
	hpet_read_register(HPET_MAIN_COUNTER_VALUE)
}

#[unsafe(no_mangle)] extern "C" fn nvc_hpet_initialize()->Status
{
	let mut hpet_acpi_ptr:*mut HighPrecisionEventTimerTable=null_mut();
	search_acpi_table(AcpiSystemDescriptorSignature::HIGH_PRECISION_EVENT_TIMER_TABLE,|x| { hpet_acpi_ptr=x.cast(); false});
	if hpet_acpi_ptr.is_null()
	{
		warn!("No HPET Hardware is detected!");
		Status::ACPI_NO_SUCH_TABLE
	}
	else
	{
		HPET_BASE_ADDRESS.store(unsafe{(*hpet_acpi_ptr).block.address},Ordering::Relaxed);
		HPET_PERIOD.store(hpet_read_register(HPET_GENERAL_COUNTER_CLOCK_PERIOD),Ordering::Relaxed);
		Status::SUCCESS
	}
}