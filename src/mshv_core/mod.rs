/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines Microsoft TLFS for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, ptr::null_mut};

pub mod cpuid;
pub mod msr;
#[cfg(windows)] pub mod forwarder;
pub mod hvcall;

pub struct MshvVcpuOps
{
	pub inform_apic_icr:unsafe fn(vcpu:*mut c_void,icr_lo:u32,icr_hi:u32),
	pub in_long_mode:fn(vcpu:*const c_void)->bool,
	pub get_vp_index:fn(vcpu:*const c_void)->u32
}
pub struct MshvVcpuContext
{
	pub root:*mut c_void,
	pub hypercall_gpfn:u64,
	pub guest_os_id:u64,
	pub ops:&'static MshvVcpuOps
}

impl MshvVcpuContext
{
	pub const fn new(ops:&'static MshvVcpuOps)->Self
	{
		Self
		{
			root:null_mut(),
			hypercall_gpfn:0,
			guest_os_id:0,
			ops
		}
	}

	pub fn inform_apic_icr(&mut self,icr_lo:u32,icr_hi:u32)
	{
		unsafe
		{
			let f=self.ops.inform_apic_icr;
			f(self.root,icr_lo,icr_hi);
		}
	}

	pub fn in_long_mode(&self)->bool
	{
		(self.ops.in_long_mode)(self.root)
	}

	pub fn get_vp_index(&self)->u32
	{
		(self.ops.get_vp_index)(self.root)
	}
}

#[allow(dead_code)]
pub mod status
{
	pub struct HvStatus(u16);

	macro_rules! define
	{
		($name:tt,$value:literal) =>
		{
			pub const $name:Self=Self($value);
		};
	}

	impl HvStatus
	{
		define!(SUCCESS,0x0);
		define!(INVALID_HYPERCALL_CODE,0x2);
		define!(INVALID_HYPERCALL_INPUT,0x3);
		define!(INVALID_ALIGNMENT,0x4);
		define!(INVALID_PARAMETER,0x5);
		define!(ACCESS_DENIED,0x6);
		define!(INVALID_PARTITION_STATE,0x7);
		define!(OPERATION_DENIED,0x8);
		define!(UNKNOWN_PROPERTY,0x9);
		define!(PROPERTY_VALUE_OUT_OF_RANGE,0xA);
		define!(INSUFFICIENT_MEMORY,0xB);
		define!(PARTITION_TOO_DEEP,0xC);
		define!(INVALID_PARTITION_ID,0xD);
		define!(INVALID_VP_INDEX,0xE);
		define!(INVALID_PORT_ID,0x11);
		define!(INVALID_CONNECTION_ID,0x12);
		define!(NOT_ACKNOWLDGED,0x14);
		define!(INVALID_VP_STATE,0x15);
		define!(ACKNOWLEDGED,0x16);
		define!(INVALID_SAVE_RESTORE_STATE,0x17);
		define!(INVALID_SYNIC_STATE,0x18);
		define!(OBJECT_IN_USE,0x19);
		define!(INVALID_PROXIMITY_DOMAIN_INFO,0x1A);
		define!(NO_DATA,0x1B);
		define!(INACTIVE,0x1C);
		define!(NO_RESOURCES,0x1D);
		define!(FEATURE_UNAVAILABLE,0x1E);
		define!(PARTIAL_PACKET,0x1F);
		define!(PROCESSOR_FEATURE_NOT_SUPPORTED,0x20);
		define!(PROCESSOR_CACHE_LINE_FLUSH_SIZE_INCOMPATIBLE,0x30);
		define!(INSUFFICIENT_BUFFER,0x33);
		define!(INCOMPATIBLE_PROCESSOR,0x37);
		define!(INSUFFICIENT_DEVICE_DOMAINS,0x38);
		define!(CPUID_FEATURE_VALIDATION_ERROR,0x3C);
		define!(CPUID_XSAVE_FEATURE_VALIDATION_ERROR,0x3D);
		define!(PROCESSOR_STARTUP_TIMEOUT,0x3E);
		define!(SMX_ENABLED,0x3F);
		define!(INVALID_LP_INDEX,0x41);
		define!(INVALID_REGISTER_VALUE,0x50);
		define!(NX_NOT_DETECTED,0x55);
		define!(INVALID_DEVICE_ID,0x57);
		define!(INVALID_DEVICE_STATE,0x58);
		define!(PENDING_PAGE_REQUESTS,0x59);
		define!(PAGE_REQUEST_INVALID,0x60);
		define!(OPERFATION_FAILED,0x71);
		define!(NOT_ALLOWED_WITH_NESTED_VIRT_ACTIVE,0x72);
	}
}