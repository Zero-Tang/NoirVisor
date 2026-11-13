/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file defines Microsoft TLFS MSR for NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::fmt::Display;

use alloc::{slice, string::String};
use bitfield_struct::bitfield;
use log::{error, info, trace, warn};

use super::hvcall::{MSHV_HYPERCALL_CODE32, MSHV_HYPERCALL_CODE64};
use crate::{
	mshv_core::MshvVcpuContext,
	xpf_core::{
		asm::{
			crdr::{read_cr8, write_cr8},
			msr::{rdmsr, wrmsr},
		},
		nvbdk::{PAGE_4KB_SIZE, noir_find_virt_by_phys, page_4kb_base, page_4kb_offset},
		x86::msr::*,
	},
};

impl MshvVcpuContext
{
	fn handle_unknown(&mut self, _write: bool, _value: &mut u64) -> bool
	{
		false
	}

	fn handle_guest_os_id(&mut self, write: bool, value: &mut u64) -> bool
	{
		if write
		{
			self.guest_os_id = *value;
			let os = GuestOsId::from(*value);
			info!("Guest OS: {os}");
		}
		else
		{
			*value = self.guest_os_id;
		}
		true
	}

	fn handle_hypercall(&mut self, write: bool, value: &mut u64) -> bool
	{
		if write
		{
			let old = HypercallConfig::from_bits(self.hypercall_gpfn);
			if !old.locked()
			{
				// This value is changeable only if it is not locked.
				self.hypercall_gpfn = *value;
				let src = if self.in_long_mode() { MSHV_HYPERCALL_CODE64.as_slice() } else { MSHV_HYPERCALL_CODE32.as_slice() };
				let dest = unsafe {
					let virt: *mut u8 = noir_find_virt_by_phys(page_4kb_base(*value)).cast();
					slice::from_raw_parts_mut(virt, src.len())
				};
				dest.copy_from_slice(src);
			}
		}
		else
		{
			*value = self.hypercall_gpfn;
		}
		true
	}

	fn handle_vp_index(&mut self, write: bool, value: &mut u64) -> bool
	{
		if write
		{
			false
		}
		else
		{
			*value = self.get_vp_index() as u64;
			true
		}
	}

	fn handle_eoi(&mut self, write: bool, value: &mut u64) -> bool
	{
		if write
		{
			// FIXME: Check if it's APIC or x2APIC.
			wrmsr(MSR_X2APIC_EOI, *value);
		}
		// This synthetic MSR is write-only.
		write
	}

	fn handle_icr(&mut self, write: bool, value: &mut u64) -> bool
	{
		if write
		{
			let hi = (*value >> 32) as u32;
			let lo = *value as u32;
			trace!("Issuing IPI with ICR-HI=0x{hi:X}, ICR-LO=0x{lo:X}...");
			self.inform_apic_icr(lo, hi);
		}
		else
		{
			// FIXME: Check if it's APIC or x2APIC
			*value = rdmsr(MSR_X2APIC_ICR);
		}
		true
	}

	fn handle_tpr(&mut self, write: bool, value: &mut u64) -> bool
	{
		// Synthetic TPR has the same structure as CR8, so just simply forward the access to cr8 register.
		if write
		{
			trace!("Raising cr8 to {value}...");
			write_cr8(*value);
		}
		else
		{
			*value = read_cr8();
		}
		true
	}

	fn handle_crash_param<const N: usize>(&mut self, write: bool, value: &mut u64) -> bool
	{
		if write
		{
			self.crash_params[N] = *value;
		}
		else
		{
			*value = self.crash_params[N];
		}
		true
	}

	fn handle_crash_ctl(&mut self, write: bool, value: &mut u64) -> bool
	{
		if write
		{
			let ctl = CrashControl::from_bits(*value);
			if ctl.crash_notify()
			{
				let [p0, p1, p2, p3, p4] = self.crash_params;
				error!("Guest reported a crash on vCPU {}!", self.get_vp_index());
				error!("P0=0x{p0:016X}, P1=0x{p1:016X}, P2=0x{p2:016X}, P3=0x{p3:016X}, P4=0x{p4:016X}");
				if ctl.crash_message()
				{
					// P3 is the guest-physical address of the message. P4 is the length of the message.
					self.log_crash_message(p3, p4);
				}
			}
		}
		else
		{
			// Report the supported crash actions.
			*value = CrashControl::new().with_crash_message(true).with_crash_notify(true).into_bits();
		}
		true
	}

	fn log_crash_message(&self, gpa: u64, length: u64)
	{
		if length > MAX_CRASH_MESSAGE_LENGTH as u64
		{
			warn!("Crash message is too long ({length} bytes)!");
			return;
		}
		let mut buffer = [0u8; MAX_CRASH_MESSAGE_LENGTH];
		let message = &mut buffer[..length as usize];
		// The message may span over two pages. Copy it page by page.
		let mut copied = 0;
		while copied < message.len()
		{
			let pa = gpa + copied as u64;
			let size = (PAGE_4KB_SIZE - page_4kb_offset(pa) as usize).min(message.len() - copied);
			// FIXME: Use vcpu's read_gpa method to read memory.
			let virt: *const u8 = unsafe { noir_find_virt_by_phys(pa) }.cast();
			if virt.is_null()
			{
				warn!("Crash message at 0x{pa:X} is not accessible!");
				return;
			}
			message[copied..copied + size].copy_from_slice(unsafe { slice::from_raw_parts(virt, size) });
			copied += size;
		}
		let message = String::from_utf8_lossy(message);
		error!("Crash message: {}", message.trim_end_matches('\0'));
	}
}

/// The maximum length of guest crash message is 4096 bytes.
const MAX_CRASH_MESSAGE_LENGTH: usize = 4096;

/// Contents of the `HV_X64_MSR_CRASH_CTL` synthetic MSR.
#[bitfield(u64)]
pub struct CrashControl
{
	#[bits(62)]
	rsvd: u64,
	/// `HV_X64_MSR_CRASH_P3` is the GPA of the crash message, and `HV_X64_MSR_CRASH_P4` is its length.
	pub crash_message: bool,
	/// The guest has written the crash parameters. The hypervisor should log them.
	pub crash_notify: bool,
}

/// Microsoft Hypervisor Top Level Functional Specification MSR Handler
///
/// This function should be implemented as a method of `MshvVcpuContext`. The `context` argument should be `&mut self`. \
/// The `write` indicates whether the VM-Exit is induced by `wrmsr` or `rdmsr` instruction. \
/// The `value` indicates the result value.
/// - If `write` is true, the content referenced by `value` is undefined.
/// - If `write` is false, `value` is referencing the content to be written into the specified MSR.
///
/// The return value indicates if this MSR-access is successful or not.
/// - If successful, advance the rip and resume the guest.
/// - If failed, inject `#GP(0)` exception and resume the guest.
pub type TlfsMsrHandler = fn(context: &mut MshvVcpuContext, write: bool, value: &mut u64) -> bool;

const MSHV_MSR_HANDLERS_COUNT: usize = 12;

static MSHV_MSR_INDICES: [u32; MSHV_MSR_HANDLERS_COUNT] = [
	HV_X64_MSR_GUEST_OS_ID,
	HV_X64_MSR_HYPERCALL,
	HV_X64_MSR_VP_INDEX,
	HV_X64_MSR_EOI,
	HV_X64_MSR_ICR,
	HV_X64_MSR_TPR,
	HV_X64_MSR_CRASH_P0,
	HV_X64_MSR_CRASH_P1,
	HV_X64_MSR_CRASH_P2,
	HV_X64_MSR_CRASH_P3,
	HV_X64_MSR_CRASH_P4,
	HV_X64_MSR_CRASH_CTL,
];

static MSHV_MSR_HANDLERS: [TlfsMsrHandler; MSHV_MSR_HANDLERS_COUNT] = [
	MshvVcpuContext::handle_guest_os_id,
	MshvVcpuContext::handle_hypercall,
	MshvVcpuContext::handle_vp_index,
	MshvVcpuContext::handle_eoi,
	MshvVcpuContext::handle_icr,
	MshvVcpuContext::handle_tpr,
	MshvVcpuContext::handle_crash_param::<0>,
	MshvVcpuContext::handle_crash_param::<1>,
	MshvVcpuContext::handle_crash_param::<2>,
	MshvVcpuContext::handle_crash_param::<3>,
	MshvVcpuContext::handle_crash_param::<4>,
	MshvVcpuContext::handle_crash_ctl,
];

pub fn dispatch_mshv_msr_handler(index: u32) -> TlfsMsrHandler
{
	// Use binary-search to look for handler.
	match MSHV_MSR_INDICES.binary_search(&index)
	{
		Ok(i) => MSHV_MSR_HANDLERS[i],
		Err(_) => MshvVcpuContext::handle_unknown,
	}
}

#[bitfield(u64)]
pub struct ProprietaryGuestOsId
{
	pub build_number: u16,
	pub service_version: u8,
	pub minor_version: u8,
	pub major_version: u8,
	pub os_id: u8,
	#[bits(15)]
	pub vendor_id: u16,
	pub open_source: bool,
}

impl ProprietaryGuestOsId
{
	pub const VENDOR_MICROSOFT: u16 = 0x0001;
	pub const VENDOR_HPE: u16 = 0x0002;
	pub const VENDOR_LANCOM: u16 = 0x0200;

	pub const MICROSOFT_OS_MS_DOS: u8 = 1;
	pub const MICROSOFT_OS_WINDOWS_3X: u8 = 2;
	pub const MICROSOFT_OS_WINDOWS_9X: u8 = 3;
	pub const MICROSOFT_OS_WINDOWS_NT: u8 = 4;
	pub const MICROSOFT_OS_WINDOWS_CE: u8 = 5;
}

#[bitfield(u64)]
pub struct OpenSourceGuestOsId
{
	pub build_number: u16,
	pub version: u32,
	pub os_id: u8,
	#[bits(7)]
	pub os_type: u8,
	pub open_source: bool,
}

impl OpenSourceGuestOsId
{
	pub const OS_LINUX: u8 = 1;
	pub const OS_FREEBSD: u8 = 2;
	pub const OS_XEN: u8 = 3;
	pub const OS_ILLUMOS: u8 = 4;
}

pub union GuestOsId
{
	pub proprietary: ProprietaryGuestOsId,
	pub open_source: OpenSourceGuestOsId,
}

impl From<u64> for GuestOsId
{
	fn from(value: u64) -> Self
	{
		Self { proprietary: ProprietaryGuestOsId(value) }
	}
}

impl Display for GuestOsId
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
	{
		let o = unsafe { self.open_source };
		if o.open_source()
		{
			// This is an open-source system.
			let os_name = match o.os_type()
			{
				OpenSourceGuestOsId::OS_LINUX => "Linux",
				OpenSourceGuestOsId::OS_FREEBSD => "FreeBSD",
				OpenSourceGuestOsId::OS_XEN => "Xen",
				OpenSourceGuestOsId::OS_ILLUMOS => "Illumos",
				_ => "Unknown Open-Source OS",
			};
			write!(f, "{os_name} Distributed by {}, Version 0x{:X}, Build {}", o.os_id(), o.version(), o.build_number())
		}
		else
		{
			// This is a proprietary system.
			let p = unsafe { self.proprietary };
			match p.vendor_id()
			{
				ProprietaryGuestOsId::VENDOR_MICROSOFT =>
				{
					let os_name = match p.os_id()
					{
						ProprietaryGuestOsId::MICROSOFT_OS_MS_DOS => "MS-DOS",
						ProprietaryGuestOsId::MICROSOFT_OS_WINDOWS_3X => "Windows 3.x",
						ProprietaryGuestOsId::MICROSOFT_OS_WINDOWS_9X => "Windows 9x",
						ProprietaryGuestOsId::MICROSOFT_OS_WINDOWS_NT => "Windows NT",
						ProprietaryGuestOsId::MICROSOFT_OS_WINDOWS_CE => "Windows CE",
						_ => "Undefined",
					};
					write!(
						f,
						"Microsoft {os_name} {}.{} Service Pack {} Build {}",
						p.major_version(),
						p.minor_version(),
						p.service_version(),
						p.build_number()
					)
				}
				ProprietaryGuestOsId::VENDOR_HPE => write!(f, "HPE OS"),
				ProprietaryGuestOsId::VENDOR_LANCOM => write!(f, "Lancom OS"),
				_ => write!(f, "Unknown Vendor OS"),
			}
		}
	}
}

#[bitfield(u64)]
pub struct HypercallConfig
{
	pub enable: bool,
	pub locked: bool,
	#[bits(10)]
	rsvd: u64,
	#[bits(52)]
	pub gpfn: u64,
}

pub const HV_X64_MSR_GUEST_OS_ID: u32 = 0x40000000;
pub const HV_X64_MSR_HYPERCALL: u32 = 0x40000001;
pub const HV_X64_MSR_VP_INDEX: u32 = 0x40000002;
pub const HV_X64_MSR_RESET: u32 = 0x40000003;
pub const HV_X64_MSR_VP_RUNTIME: u32 = 0x40000010;
pub const HV_X64_MSR_TIME_REF_COUNT: u32 = 0x40000020;
pub const HV_X64_MSR_REFERENCE_TSC: u32 = 0x40000021;
pub const HV_X64_MSR_TSC_FREQUENCY: u32 = 0x40000022;
pub const HV_X64_MSR_APIC_FREQUENCY: u32 = 0x40000023;
pub const HV_X64_MSR_NPIEP_CONFIG: u32 = 0x40000040;
pub const HV_X64_MSR_EOI: u32 = 0x40000070;
pub const HV_X64_MSR_ICR: u32 = 0x40000071;
pub const HV_X64_MSR_TPR: u32 = 0x40000072;
pub const HV_X64_MSR_VP_ASSIST_PAGE: u32 = 0x40000073;
pub const HV_X64_MSR_SCONTROL: u32 = 0x40000080;
pub const HV_X64_MSR_SVERSION: u32 = 0x40000081;
pub const HV_X64_MSR_SIEFP: u32 = 0x40000082;
pub const HV_X64_MSR_SIMP: u32 = 0x40000083;
pub const HV_X64_MSR_EOM: u32 = 0x40000084;
pub const HV_X64_MSR_SINT0: u32 = 0x40000090;
pub const HV_X64_MSR_SINT1: u32 = 0x40000091;
pub const HV_X64_MSR_SINT2: u32 = 0x40000092;
pub const HV_X64_MSR_SINT3: u32 = 0x40000093;
pub const HV_X64_MSR_SINT4: u32 = 0x40000094;
pub const HV_X64_MSR_SINT5: u32 = 0x40000095;
pub const HV_X64_MSR_SINT6: u32 = 0x40000096;
pub const HV_X64_MSR_SINT7: u32 = 0x40000097;
pub const HV_X64_MSR_SINT8: u32 = 0x40000098;
pub const HV_X64_MSR_SINT9: u32 = 0x40000099;
pub const HV_X64_MSR_SINT10: u32 = 0x4000009A;
pub const HV_X64_MSR_SINT11: u32 = 0x4000009B;
pub const HV_X64_MSR_SINT12: u32 = 0x4000009C;
pub const HV_X64_MSR_SINT13: u32 = 0x4000009D;
pub const HV_X64_MSR_SINT14: u32 = 0x4000009E;
pub const HV_X64_MSR_SINT15: u32 = 0x4000009F;
pub const HV_X64_MSR_STIMER0_CONFIG: u32 = 0x400000B0;
pub const HV_X64_MSR_STIMER0_COUNT: u32 = 0x400000B1;
pub const HV_X64_MSR_STIMER1_CONFIG: u32 = 0x400000B2;
pub const HV_X64_MSR_STIMER1_COUNT: u32 = 0x400000B3;
pub const HV_X64_MSR_STIMER2_CONFIG: u32 = 0x400000B4;
pub const HV_X64_MSR_STIMER2_COUNT: u32 = 0x400000B5;
pub const HV_X64_MSR_STIMER3_CONFIG: u32 = 0x400000B6;
pub const HV_X64_MSR_STIMER3_COUNT: u32 = 0x400000B7;
pub const HV_X64_MSR_GUEST_IDLE: u32 = 0x400000F0;
pub const HV_X64_MSR_CRASH_P0: u32 = 0x40000100;
pub const HV_X64_MSR_CRASH_P1: u32 = 0x40000101;
pub const HV_X64_MSR_CRASH_P2: u32 = 0x40000102;
pub const HV_X64_MSR_CRASH_P3: u32 = 0x40000103;
pub const HV_X64_MSR_CRASH_P4: u32 = 0x40000104;
pub const HV_X64_MSR_CRASH_CTL: u32 = 0x40000105;
pub const HV_X64_MSR_REENLIGHTENMENT_CONTROL: u32 = 0x40000106;
pub const HV_X64_MSR_TSC_EMULATION_CONTROL: u32 = 0x40000107;
pub const HV_X64_MSR_TSC_EMULATION_STATUS: u32 = 0x40000108;
pub const HV_X64_MSR_STIME_UNHALTED_TIMER_CONFIG: u32 = 0x40000114;
pub const HV_X64_MSR_STIME_UNHALTED_TIMER_COUNT: u32 = 0x40000115;
pub const HV_X64_MSR_NESTED_VP_INDEX: u32 = 0x40001002;
pub const HV_X64_MSR_NESTED_SCONTROL: u32 = 0x40001080;
pub const HV_X64_MSR_NESTED_SVERSION: u32 = 0x40001081;
pub const HV_X64_MSR_NESTED_SIEFP: u32 = 0x40001082;
pub const HV_X64_MSR_NESTED_SIMP: u32 = 0x40001083;
pub const HV_X64_MSR_NESTED_EOM: u32 = 0x40001084;
pub const HV_X64_MSR_NESTED_SINT0: u32 = 0x40001090;
pub const HV_X64_MSR_NESTED_SINT1: u32 = 0x40001091;
pub const HV_X64_MSR_NESTED_SINT2: u32 = 0x40001092;
pub const HV_X64_MSR_NESTED_SINT3: u32 = 0x40001093;
pub const HV_X64_MSR_NESTED_SINT4: u32 = 0x40001094;
pub const HV_X64_MSR_NESTED_SINT5: u32 = 0x40001095;
pub const HV_X64_MSR_NESTED_SINT6: u32 = 0x40001096;
pub const HV_X64_MSR_NESTED_SINT7: u32 = 0x40001097;
pub const HV_X64_MSR_NESTED_SINT8: u32 = 0x40001098;
pub const HV_X64_MSR_NESTED_SINT9: u32 = 0x40001099;
pub const HV_X64_MSR_NESTED_SINT10: u32 = 0x4000109A;
pub const HV_X64_MSR_NESTED_SINT11: u32 = 0x4000109B;
pub const HV_X64_MSR_NESTED_SINT12: u32 = 0x4000109C;
pub const HV_X64_MSR_NESTED_SINT13: u32 = 0x4000109D;
pub const HV_X64_MSR_NESTED_SINT14: u32 = 0x4000109E;
pub const HV_X64_MSR_NESTED_SINT15: u32 = 0x4000109F;
