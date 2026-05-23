/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines Microsoft TLFS CPUID for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{arch::x86_64::CpuidResult, slice};

use bitfield_struct::bitfield;

use crate::xpf_core::x86::cpuid::CpuidLeaf;

static HYPERVISOR_VENDOR_STRING:&str="NoirVisor ZT";

fn nvc_mshv_cpuid_hypervisor_vendor_id_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	let hv_vendor=HYPERVISOR_VENDOR_STRING.as_bytes();
	let a=0x3FFFFFFF+MSHV_CPUID_HANDLERS_COUNT as u32;
	let b=u32::from_le_bytes(hv_vendor[..4].try_into().unwrap());
	let c=u32::from_le_bytes(hv_vendor[4..8].try_into().unwrap());
	let d=u32::from_le_bytes(hv_vendor[8..].try_into().unwrap());
	(a,b,c,d)
}

fn nvc_mshv_cpuid_hypervisor_interface_id_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	let a=u32::from_le_bytes(*b"Hv#1");
	let b=0;
	let c=0;
	let d=0;
	(a,b,c,d)
}

fn nvc_mshv_cpuid_hypervisor_system_id_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	(0,0,0,0)
}

fn nvc_mshv_cpuid_hypervisor_feature_id_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	let mut r=HypervisorFeatureId::new();
	// Requirements for Minimal Hv#1 interface.
	r.set_hypercall_msr(true);
	r.set_vp_index(true);
	let a:&[u32;4]=unsafe{&*(&raw const r.0).cast()};
	(a[0],a[1],a[2],a[3])
}

fn nvc_mshv_cpuid_implementation_recommendation_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	let mut r=HypervisorImplementationRecommendation::new();
	// It seems we can't recommend MSR accesses for EOI/ICR/TPR alone.
	// Windows 10 LTSC 2021 (build 19044) may BSoD during boot while issuing EOI.
	// r.set_msr_eoi_icr_tpr(true);
	r.set_relaxed_timing(true);
	let a:&[u32;4]=unsafe{&*(&raw const r.0).cast()};
	(a[0],a[1],a[2],a[3])
}

fn nvc_mshv_cpuid_implementation_limit_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	// Nothing to limit
	(0,0,0,0)
}

fn nvc_mshv_cpuid_implementation_hardware_handler(_ia:u32,_ic:u32)->(u32,u32,u32,u32)
{
	let mut r=HypervisorImplementationHardwareFeatures::new();
	r.set_slat(true);
	r.set_msr_bitmaps(true);
	let a:&[u32;4]=unsafe{&*(&raw const r.0).cast()};
	(a[0],a[1],a[2],a[3])
}

type TlfsCpuidHandler=fn(u32,u32)->(u32,u32,u32,u32);

const MSHV_CPUID_HANDLERS_COUNT:usize=7;
pub const MSHV_CPUID_HANDLERS:[TlfsCpuidHandler;MSHV_CPUID_HANDLERS_COUNT]=
[
	nvc_mshv_cpuid_hypervisor_vendor_id_handler,
	nvc_mshv_cpuid_hypervisor_interface_id_handler,
	nvc_mshv_cpuid_hypervisor_system_id_handler,
	nvc_mshv_cpuid_hypervisor_feature_id_handler,
	nvc_mshv_cpuid_implementation_recommendation_handler,
	nvc_mshv_cpuid_implementation_limit_handler,
	nvc_mshv_cpuid_implementation_hardware_handler
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

macro_rules! derive_bitfield_cpuid_leaf_impl
{
	($name:ty,$index:literal)=>
	{
		impl CpuidLeaf for $name
		{
			const LEAF_INDEX:u32=$index;
			const SUBLEAF_INDEX:Option<u32>=None;

			fn init(&mut self,result:&CpuidResult)
			{
				let v:&mut [u32;4]=unsafe{&mut *(&raw mut self.0).cast()};
				v[0]=result.eax;
				v[1]=result.ebx;
				v[2]=result.ecx;
				v[3]=result.edx;
			}

			fn as_result(&self)->CpuidResult
			{
				let v:&[u32;4]=unsafe{&*(&raw const self.0).cast()};
				CpuidResult
				{
					eax:v[0],
					ebx:v[1],
					ecx:v[2],
					edx:v[3]
				}
			}
		}
	};
}

pub struct MaxHypervisorLeafAndVendorString
{
	pub max_leaf:u32,
	vendor_string:[u8;12]
}

impl CpuidLeaf for MaxHypervisorLeafAndVendorString
{
	const LEAF_INDEX:u32 = 0x40000000;
	const SUBLEAF_INDEX:Option<u32> = None;

	fn init(&mut self,result:&CpuidResult)
	{
		self.max_leaf=result.eax;
		self.vendor_string[0..0x4].copy_from_slice(&result.ebx.to_le_bytes());
		self.vendor_string[4..0x8].copy_from_slice(&result.edx.to_le_bytes());
		self.vendor_string[8..0xC].copy_from_slice(&result.ecx.to_le_bytes());
	}

	fn as_result(&self)->CpuidResult
	{
		let s:&[u32]=unsafe{slice::from_raw_parts(self.vendor_string.as_ptr().cast(),3)};
		CpuidResult
		{
			eax:self.max_leaf,
			ebx:s[0],
			ecx:s[1],
			edx:s[2]
		}
	}
}

impl MaxHypervisorLeafAndVendorString
{
	pub fn vendor_name(&self)->&str
	{
		unsafe
		{
			str::from_utf8_unchecked(&self.vendor_string)
		}
	}
}

pub struct HypervisorVendorNeutralInterface
{
	pub interface_id:u32,
	rsvd:[u32;3]
}

impl CpuidLeaf for HypervisorVendorNeutralInterface
{
	const LEAF_INDEX:u32 = 0x40000001;
	const SUBLEAF_INDEX:Option<u32> = None;

	fn init(&mut self,result:&CpuidResult)
	{
		self.interface_id=result.eax;
		self.rsvd[0]=result.ebx;
		self.rsvd[1]=result.ecx;
		self.rsvd[2]=result.edx;
	}

	fn as_result(&self)->CpuidResult
	{
		CpuidResult
		{
			eax:self.interface_id,
			ebx:self.rsvd[0],
			ecx:self.rsvd[1],
			edx:self.rsvd[2]
		}
	}
}

impl HypervisorVendorNeutralInterface
{
	pub fn interface_name(&self)->&str
	{
		unsafe
		{
			let s:&[u8]=slice::from_raw_parts((&raw const self.interface_id).cast(),4);
			str::from_utf8_unchecked(s)
		}
	}
}

#[bitfield(u128)] pub struct HypervisorFeatureId
{
	pub vp_runtime_msr:bool,
	pub partition_ref_counter:bool,
	pub synic_msr:bool,
	pub synthetic_timer_msr:bool,
	pub apic_msr:bool,
	pub hypercall_msr:bool,
	pub vp_index:bool,
	pub reset_msr:bool,
	pub stats_msr:bool,
	pub partition_ref_tsc:bool,
	pub guest_idle_msr:bool,
	pub frequency_msr:bool,
	#[bits(20)] rsvd0:u32,
	pub create_partition:bool,
	pub access_partition_id:bool,
	pub access_memory_pool:bool,
	pub adjust_message_buffers:bool,
	pub port_messages:bool,
	pub signal_events:bool,
	pub create_port:bool,
	pub connect_port:bool,
	pub access_stats:bool,
	#[bits(2)] rsvd1:u32,
	pub debugging:bool,
	pub cpu_management:bool,
	pub configure_profiler:bool,
	#[bits(18)] rsvd2:u32,
	pub power_management:u32,
	pub mwait_avail:bool,
	pub guest_debug:bool,
	pub performance_monitor:bool,
	pub phys_cpu_dynp_event:bool,
	pub hypercall_xmm_support:bool,
	pub virt_guest_idle_state:bool,
	pub hv_sleep_state:bool,
	pub query_numa_distance:bool,
	pub determine_timer_freq:bool,
	pub inject_synthetic_mc:bool,
	pub guest_crash_msr:bool,
	pub guest_debug_msr:bool,
	pub npiep:bool,
	pub disable_hypervisor:bool,
	pub ext_gvar_flush_va_list:bool,
	pub ret_hypercall_by_xmm:bool,
	rsvd3:bool,
	pub sint_polling_mode:bool,
	pub hypercall_msr_lock:bool,
	pub direct_synthetic_timer:bool,
	pub avail_pat_for_vsm:bool,
	pub avail_bndcfgs_vsm:bool,
	rsvd4:bool,
	pub synthetic_unhalted_timer:bool,
	#[bits(2)] rsvd5:u32,
	pub use_lbr_support:bool,
	#[bits(5)] rsvd6:u32
}

derive_bitfield_cpuid_leaf_impl!(HypervisorFeatureId,0x40000003);

#[bitfield(u128)] pub struct HypervisorImplementationRecommendation
{
	pub hvcall_switch_cr3:bool,
	pub hvcall_invlpg:bool,
	pub hvcall_remote_tlb_flush:bool,
	pub msr_eoi_icr_tpr:bool,
	pub msr_reset:bool,
	pub relaxed_timing:bool,
	pub dma_remapping:bool,
	pub intr_remapping:bool,
	rsvd0:bool,
	pub deprecate_auto_eoi:bool,
	pub hvcall_synthetic_clustered_ipi:bool,
	pub newer_processor_masks:bool,
	pub nested_in_hyperv:bool,
	pub int_mbec:bool,
	pub enlightened_vmcs:bool,
	pub use_synced_timeline:bool,
	rsvd1:bool,
	pub use_direct_local_flush_entire:bool,
	pub no_non_arch_core_sharing:bool,
	#[bits(13)] rsvd2:u32,
	pub long_spinlock_attempts:u32,
	#[bits(7)] pub impl_phys_bits:u8,
	#[bits(25)] rsvd3:u32,
	pub rsvd_edx:u32
}

derive_bitfield_cpuid_leaf_impl!(HypervisorImplementationRecommendation,0x40000004);

#[derive(Clone, Copy)]
#[repr(C)] pub struct HypervisorImplementationLimits
{
	pub max_vcpu_supported:u32,
	pub max_lcpu_supported:u32,
	pub max_phys_intr_vectors:u32,
	rsvd:u32
}

impl CpuidLeaf for HypervisorImplementationLimits
{
	const LEAF_INDEX:u32 = 0x40000005;
	const SUBLEAF_INDEX:Option<u32> = None;

	fn init(&mut self,result:&CpuidResult)
	{
		self.max_vcpu_supported=result.eax;
		self.max_lcpu_supported=result.ebx;
		self.max_phys_intr_vectors=result.ecx;
		self.rsvd=result.edx;
	}

	fn as_result(&self)->CpuidResult
	{
		CpuidResult
		{
			eax:self.max_vcpu_supported,
			ebx:self.max_lcpu_supported,
			ecx:self.max_phys_intr_vectors,
			edx:self.rsvd
		}
	}
}

#[bitfield(u128)] pub struct HypervisorImplementationHardwareFeatures
{
	pub apic_overlay:bool,
	pub msr_bitmaps:bool,
	pub arch_perf_counter:bool,
	pub slat:bool,
	pub dma_remap:bool,
	pub intr_remap:bool,
	pub memory_patrol_scrub:bool,
	pub dma_protection:bool,
	pub hpet_requested:bool,
	pub volatile_synthetic_timer:bool,
	#[bits(118)] rsvd:u128
}

derive_bitfield_cpuid_leaf_impl!(HypervisorImplementationHardwareFeatures,0x40000006);