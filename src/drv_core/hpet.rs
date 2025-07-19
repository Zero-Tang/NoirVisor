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

use log::*;

use crate::xpf_core::{asm::io::mmio_read, nvstatus::*};
use super::acpi::{search_acpi_table,tables::{AcpiSystemDescriptorSignature,HighPrecisionEventTimerTable}};

const HPET_GENRERAL_COUNTER_CLOCK_PERIOD:usize=0x4;
const HPET_MAIN_COUNTER_VALUE:usize=0xF0;

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
		NOIR_ACPI_NO_SUCH_TABLE
	}
	else
	{
		HPET_BASE_ADDRESS.store(unsafe{(*hpet_acpi_ptr).block.address},Ordering::Relaxed);
		HPET_PERIOD.store(hpet_read_register(HPET_GENRERAL_COUNTER_CLOCK_PERIOD),Ordering::Relaxed);
		NOIR_SUCCESS
	}
}