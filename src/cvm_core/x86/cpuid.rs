/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file handles cpuid instructions for NoirVisor CVM Guests.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::disasm::emulator::EmulatorOps;

fn handle_unknown(vcpu:&mut dyn EmulatorOps)
{
	vcpu.set_gpr(0,0);
	vcpu.set_gpr(1,0);
	vcpu.set_gpr(2,0);
	vcpu.set_gpr(3,0);
}

fn handle_cpuid_std_vendor(vcpu:&mut dyn EmulatorOps)
{
	vcpu.set_gpr(0,0);
}

fn handle_cpuid_ext_vendor(vcpu:&mut dyn EmulatorOps)
{
	vcpu.set_gpr(0,0);
}

pub type CpuidHandler=fn(vcpu:&mut dyn EmulatorOps);

static CPUID_STD_LEAF_HANDLERS:[CpuidHandler;1]=
[
	handle_cpuid_std_vendor
];

static CPUID_EXT_LEAF_HANDLERS:[CpuidHandler;1]=
[
	handle_cpuid_ext_vendor
];

static CPUID_HANDLER_GROUPS:[&[CpuidHandler];4]=
[
	// Standard leaves
	&CPUID_STD_LEAF_HANDLERS,
	// Hypervisor leaves
	&[],
	// Extended leaves
	&CPUID_EXT_LEAF_HANDLERS,
	// Unknown leaves
	&[]
];

pub fn dispatch_cpuid_handler(leaf:u32)->CpuidHandler
{
	// Handling CPUID can be optimized into O(1) time complexity.
	let class=(leaf>>30) as usize;
	let index=(leaf&0x3FFFFFFF) as usize;
	CPUID_HANDLER_GROUPS[class].get(index).copied().unwrap_or(handle_unknown)
}