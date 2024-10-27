/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file defines Microsoft TLFS CPUID for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

fn nvc_mshv_cpuid_hypervisor_system_id_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	let hv_vendor="NoirVisor ZT".as_bytes();
	let a=0x40000001u32;
	let b=u32::from_le_bytes(hv_vendor[..4].try_into().unwrap());
	let c=u32::from_le_bytes(hv_vendor[4..8].try_into().unwrap());
	let d=u32::from_le_bytes(hv_vendor[8..].try_into().unwrap());
	(a,b,c,d)
}

fn nvc_mshv_cpuid_hypervisor_feature_id_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	let signature="Hv#0".as_bytes();
	let a=u32::from_le_bytes(signature.try_into().unwrap());
	let b=0;
	let c=0;
	let d=0;
	(a,b,c,d)
}

type TlfsCpuidHandler=fn(u32,u32)->(u32,u32,u32,u32);

pub const MSHV_CPUID_HANDLERS_COUNT:usize=2;
pub const MSHV_CPUID_HANDLERS:[TlfsCpuidHandler;MSHV_CPUID_HANDLERS_COUNT]=
[
	nvc_mshv_cpuid_hypervisor_system_id_handler,
	nvc_mshv_cpuid_hypervisor_feature_id_handler
];

pub const CPUID_LEAF_RANGE_AND_VENDOR_STRING:u32=0x40000000;
pub const CPUID_VENDOR_NEUTRAL_INTERFACE_ID:u32=0x40000001;
pub const CPUID_HYPERVISOR_SYSTEM_ID:u32=0x40000002;
pub const CPUID_HYPERVISOR_FEATURE_ID:u32=0x40000003;
pub const CPUID_IMPLEMENTATION_RECOMMENDATIONS:u32=0x40000004;
pub const CPUID_IMPLEMENTATION_LIMITS:u32=0x40000005;
pub const CPUID_IMPLEMENTATION_HARDWARE_FEATURES:u32=0x40000006;
pub const CPUID_CPU_MANAGEMENT_FEATURES:u32=0x40000007;
pub const CPUID_SHARED_VIRTUAL_MEMORY_FEATURES:u32=0x40000008;
pub const CPUID_NESTED_HYPERVISOR_FEATURE_ID:u32=0x40000009;
pub const CPUID_NESTED_VIRTUALIZATION_FEATURES:u32=0x4000000A;