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

use core::arch::x86_64::CpuidResult;

use crate::{
	disasm::emulator::EmulatorOps,
	mshv_core::cpuid::*,
	svm_core::amd64::cpuid::CPUID_EXT_PROCESSOR_TOPOLOGY_INFORMATION,
	xpf_core::{
		asm::msr::rdmsr,
		x86::{apic::ApicBar, cpuid::*, msr::MSR_APIC_BASE},
	},
};

// While we don't really need to clone and copy the whole structure,
// This derivation simply helps us to constrain all data be `Copy`.
#[derive(Clone, Copy, Debug, Default)]
pub struct StaticCpuidData
{
	/// eax=0x0
	pub std_vendor: MaxStandardLeafAndVendorString,
	/// eax=0x1
	pub std_cpu_feat_id: StandardProcessorFeatureIdentifiers,
	/// eax=0x5
	pub monitor_mwait: MonitorMwaitIdentifiers,
	/// eax=0x6
	pub power_mgr_feat: PowerManagementFeatures,
	/// eax=0x7, ecx=0
	pub struct_ext_feat0: StructuredExtendedFeatures0,
	/// eax=0x7, ecx=1
	pub struct_ext_feat1: StructuredExtendedFeatures1,
	/// eax=0xB, ecx=0
	pub ext_topo_enum_thread: ExtendedTopologyEnumeration<0>,
	/// eax=0xB, ecx=1
	pub ext_topo_enum_core: ExtendedTopologyEnumeration<1>,
	/// eax=0xD, ecx=0
	pub proc_xstate0: ExtendedStateEnumeration0,
	/// eax=0xD, ecx=1
	pub proc_xstate1: ExtendedStateEnumeration1,
	/// eax=0xD, ecx=2
	pub proc_xstate_avx: ExtendedStateEnumerationN<2>,
	/// eax=0x80000000
	pub ext_vendor: MaxExtendedLeafAndVendorString,
	/// eax=0x80000001
	pub ext_cpu_feat_id: ExtendedFeatureIdentifier,
	/// eax=0x80000002~0x80000004
	pub cpu_brand: ProcessorBrandString,
	/// eax=0x80000005~0x80000006
	pub cache_tlb: CacheAndTlbInformation,
	/// eax=0x80000008
	pub proc_cap_param: ProcessorCapabilityParameters,
	/// eax=0x8000001E
	pub proc_topo_info: ProcessorTopologyInformation,
}

impl StaticCpuidData
{
	/// Creates a `StaticCpuidData` structure which all expected
	/// x86 features would be used in the guest of NoirVisor.
	///
	/// By saying "expected", it means the features that NoirVisor
	/// could grant to its guest.
	pub fn new() -> Self
	{
		// Query what features the current processor supports.
		let mut r = Self {
			std_vendor: MaxStandardLeafAndVendorString::cpuid(),
			std_cpu_feat_id: StandardProcessorFeatureIdentifiers::cpuid(),
			monitor_mwait: MonitorMwaitIdentifiers::cpuid(),
			power_mgr_feat: PowerManagementFeatures::cpuid(),
			struct_ext_feat0: StructuredExtendedFeatures0::cpuid(),
			struct_ext_feat1: StructuredExtendedFeatures1::cpuid(),
			ext_topo_enum_thread: ExtendedTopologyEnumeration::<0>::cpuid(),
			ext_topo_enum_core: ExtendedTopologyEnumeration::<1>::cpuid(),
			proc_xstate0: ExtendedStateEnumeration0::cpuid(),
			proc_xstate1: ExtendedStateEnumeration1::cpuid(),
			proc_xstate_avx: ExtendedStateEnumerationN::<2>::cpuid(),
			ext_vendor: MaxExtendedLeafAndVendorString::cpuid(),
			ext_cpu_feat_id: ExtendedFeatureIdentifier::cpuid(),
			cpu_brand: ProcessorBrandString::cpuid(),
			cache_tlb: CacheAndTlbInformation::cpuid(),
			proc_cap_param: ProcessorCapabilityParameters::cpuid(),
			proc_topo_info: ProcessorTopologyInformation::cpuid(),
		};
		log::trace!("Before Mask CPUID: {r:X?}");
		// Retain known features.
		r.std_vendor.max_leaf = CPUID_STD_PROCESSOR_EXTENDED_STATE_ENUMERATION;
		let supported = StandardProcessorFeatureIdentifiers::new()
			.with_ext_family(u8::MAX)
			.with_ext_model(0xF)
			.with_base_family(0xF)
			.with_base_model(0xF)
			.with_stepping(0xF)
			.with_local_apic_id(u8::MAX)
			.with_logical_cpu_count(u8::MAX)
			.with_clflush_size(u8::MAX)
			.with_brand_id(u8::MAX)
			.with_rdrand(true)
			.with_f16c(true)
			.with_avx(true)
			.with_xsave(true)
			.with_aes(true)
			.with_popcnt(true)
			.with_movbe(true)
			.with_x2apic(false) // Disable x2APIC
			.with_sse42(true)
			.with_sse41(true)
			.with_pcid(true)
			.with_cmpxchg16b(true)
			.with_fma(true)
			.with_ssse3(true)
			.with_pclmulqdq(true)
			.with_sse3(true)
			.with_htt(true)
			.with_sse2(true)
			.with_sse(true)
			.with_fxsr(true)
			.with_mmx(true)
			.with_clflush(true)
			.with_pse36(true)
			.with_pat(true)
			.with_cmov(true)
			.with_mca(true)
			.with_pge(true)
			.with_mtrr(true)
			.with_sysenter_sysexit(true)
			.with_apic(true)
			.with_cmpxchg8b(true)
			.with_mce(true)
			.with_pae(true)
			.with_msr(true)
			.with_tsc(true)
			.with_pse(true)
			.with_de(true)
			.with_vme(true)
			.with_fpu(true);
		r.std_cpu_feat_id &= supported;
		r.std_cpu_feat_id.set_hypervisor(true);
		// MONITOR/MWAIT and their extension leaf are not emulated for CVM guests.
		let supported = MonitorMwaitIdentifiers::new()
			.with_smallest_monitor_line_size(u16::MAX)
			.with_largest_monitor_line_size(u16::MAX)
			.with_emx(true)
			.with_ibe(true);
		r.monitor_mwait &= supported;
		// Power, thermal, and frequency-management capabilities are not virtualized.
		// ARAT can be pass-thru.
		r.power_mgr_feat = PowerManagementFeatures::new().with_arat(true);
		// Keep only known-safe leaf 7 capabilities; this also clears unmodeled flags.
		let supported = StructuredExtendedFeatures0::new()
			.with_max_sub_fn(1)
			.with_fsgsbase(true)
			.with_bmi1(true)
			.with_avx2(true)
			.with_smep(true)
			.with_bmi2(true)
			.with_erms(true)
			.with_invpcid(true)
			.with_rdseed(true)
			.with_adx(true)
			.with_smap(true)
			.with_clflushopt(true)
			.with_clwb(true)
			.with_sha(true)
			.with_umip(true)
			.with_gfni(true)
			.with_vaes(true)
			.with_vpcmulqdq(true)
			.with_la57(true)
			.with_rdpid(true)
			.with_movdiri(true)
			.with_movdir64b(true);
		r.struct_ext_feat0 &= supported;
		let supported = StructuredExtendedFeatures1::new().with_avx_vnni(true);
		r.struct_ext_feat1 &= supported;
		// CVM virtualizes only x87, SSE, and AVX/YMM extended state.
		const MAX_XSAVE_SIZE: u32 = 0x340;
		let xstate_mask = r.proc_xstate0.mask_lo() & 0x7;
		let supported_xstate_size = if xstate_mask & (1 << 2) != 0 { MAX_XSAVE_SIZE } else { 0x240 };
		r.proc_xstate0 = r
			.proc_xstate0
			.with_mask_lo(xstate_mask)
			.with_mask_hi(0)
			.with_enabled_size(r.proc_xstate0.enabled_size().min(supported_xstate_size))
			.with_supported_size(supported_xstate_size);
		let supported = ExtendedStateEnumeration1::new().with_xsaveopt(true).with_xsavec(true).with_fixed_xstate_size(u32::MAX);
		r.proc_xstate1 &= supported;
		r.proc_xstate1 = r.proc_xstate1.with_fixed_xstate_size(r.proc_xstate1.fixed_xstate_size().min(MAX_XSAVE_SIZE));
		if r.proc_xstate0.mask_lo() & (1 << 2) == 0
		{
			r.proc_xstate_avx = ExtendedStateEnumerationN::default();
		}
		// Only native, non-virtualization-dependent AMD features are retained.
		let supported = ExtendedFeatureIdentifier::new()
			.with_stepping(0xF)
			.with_base_model(0xF)
			.with_base_family(0xF)
			.with_ext_model(0xF)
			.with_ext_family(u8::MAX)
			.with_brand_id(u16::MAX)
			.with_pkg_type(0xF)
			.with_lahf_sahf(true)
			.with_cmp_legacy(true)
			.with_ext_apic_space(true)
			.with_alt_mov_cr8(true)
			.with_abm(true)
			.with_sse4a(true)
			.with_misalign_sse(true)
			.with_prefetch_3dnow(true)
			.with_xop(true)
			.with_fma4(true)
			.with_tce(true)
			.with_tbm(true)
			.with_data_bkpt_ext(true)
			.with_addr_mask_ext(true)
			.with_fpu(true)
			.with_vme(true)
			.with_de(true)
			.with_pse(true)
			.with_tsc(true)
			.with_msr(true)
			.with_pae(true)
			.with_mce(true)
			.with_cmpxchg8b(true)
			.with_apic(true)
			.with_syscall_sysret(true)
			.with_mtrr(true)
			.with_pge(true)
			.with_mca(true)
			.with_cmov(true)
			.with_pat(true)
			.with_pse36(true)
			.with_nx(true)
			.with_mmx_ext(true)
			.with_mmx(true)
			.with_fxsr(true)
			.with_fast_fxsr(true)
			.with_page_1gb(true)
			.with_rdtscp(true)
			.with_long_mode(true)
			.with_amd_3dnow_ext(true)
			.with_amd_3dnow(true);
		r.ext_cpu_feat_id &= supported;
		// Preserve address/topology information and CLZERO; suppress unsupported
		// MSR, performance-monitoring, security-control, and cache/TLB features.
		let supported = ProcessorCapabilityParameters::new()
			.with_phys_addr_size(u8::MAX)
			.with_virt_addr_size(u8::MAX)
			.with_guest_phys_addr_size(u8::MAX)
			.with_wbnoinvd(true)
			.with_clzero(true)
			.with_restore_fp_err_ptr(true)
			.with_efer_lmsle_unsupported(true)
			.with_phys_threads(u8::MAX)
			.with_apic_id_size(0xF);
		r.proc_cap_param &= supported;
		r.proc_cap_param.set_efer_lmsle_unsupported(true);
		r.ext_vendor.max_leaf = core::cmp::min(CPUID_EXT_PROCESSOR_TOPOLOGY_INFORMATION, r.ext_vendor.max_leaf);
		log::trace!("After Mask CPUID: {r:X?}");
		r
	}
}

pub trait CpuidHandlerOps: EmulatorOps
{
	fn static_cpuid_data(&self) -> &StaticCpuidData;
}

fn emulate_cpuid_raw(vcpu: &mut dyn CpuidHandlerOps, result: &CpuidResult)
{
	vcpu.set_gpr(0, result.eax as u64);
	vcpu.set_gpr(1, result.ecx as u64);
	vcpu.set_gpr(2, result.edx as u64);
	vcpu.set_gpr(3, result.ebx as u64);
}

fn emulate_cpuid(vcpu: &mut dyn CpuidHandlerOps, data: &impl CpuidLeaf)
{
	let r = data.as_result();
	emulate_cpuid_raw(vcpu, &r);
}

fn handle_unknown(vcpu: &mut dyn CpuidHandlerOps)
{
	emulate_cpuid_raw(vcpu, &CpuidResult { eax: 0, ebx: 0, ecx: 0, edx: 0 });
}

fn handle_cpuid_std_vendor(vcpu: &mut dyn CpuidHandlerOps)
{
	let leaf = vcpu.static_cpuid_data().std_vendor;
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_std_cpu_feat_id(vcpu: &mut dyn CpuidHandlerOps)
{
	let mut leaf = vcpu.static_cpuid_data().std_cpu_feat_id;
	leaf.set_osxsave(vcpu.get_cr4().osxsave());
	// TODO: virtualize APIC MSR.
	let apic = ApicBar::from_bits(rdmsr(MSR_APIC_BASE));
	leaf.set_apic(apic.enable());
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_power_mgr_feat(vcpu: &mut dyn CpuidHandlerOps)
{
	let leaf = vcpu.static_cpuid_data().power_mgr_feat;
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_monitor_mwait(vcpu: &mut dyn CpuidHandlerOps)
{
	let leaf = vcpu.static_cpuid_data().monitor_mwait;
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_struct_ext_feat(vcpu: &mut dyn CpuidHandlerOps)
{
	match vcpu.get_gpr(1) as u32
	{
		0 =>
		{
			let leaf = vcpu.static_cpuid_data().struct_ext_feat0;
			emulate_cpuid(vcpu, &leaf);
		}
		1 =>
		{
			let leaf = vcpu.static_cpuid_data().struct_ext_feat1;
			emulate_cpuid(vcpu, &leaf);
		}
		_ => handle_unknown(vcpu),
	}
}

fn handle_cpuid_ext_topo_enum(vcpu: &mut dyn CpuidHandlerOps)
{
	match vcpu.get_gpr(1) as u32
	{
		0 =>
		{
			let leaf = vcpu.static_cpuid_data().ext_topo_enum_thread;
			emulate_cpuid(vcpu, &leaf);
		}
		1 =>
		{
			let leaf = vcpu.static_cpuid_data().ext_topo_enum_core;
			emulate_cpuid(vcpu, &leaf);
		}
		_ => handle_unknown(vcpu),
	}
}

fn handle_cpuid_proc_xstate(vcpu: &mut dyn CpuidHandlerOps)
{
	match vcpu.get_gpr(1) as u32
	{
		0 =>
		{
			let mut leaf = vcpu.static_cpuid_data().proc_xstate0;
			let mut size = 0x240;
			let xcr0 = vcpu.get_xcr0();
			size += if xcr0.avx() { 0x100 } else { 0 };
			leaf.set_enabled_size(size);
			emulate_cpuid(vcpu, &leaf);
		}
		1 =>
		{
			let mut leaf = vcpu.static_cpuid_data().proc_xstate1;
			let mut size = 0x240;
			let xcr0 = vcpu.get_xcr0();
			size += if xcr0.avx() { 0x100 } else { 0 };
			leaf.set_fixed_xstate_size(size);
			emulate_cpuid(vcpu, &leaf);
		}
		2 =>
		{
			let leaf = vcpu.static_cpuid_data().proc_xstate_avx;
			emulate_cpuid(vcpu, &leaf);
		}
		_ => handle_unknown(vcpu),
	}
}

fn handle_cpuid_hv_leaf_generic<T: MshvCpuidType>(vcpu: &mut dyn CpuidHandlerOps)
{
	let leaf = T::guest();
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_ext_vendor(vcpu: &mut dyn CpuidHandlerOps)
{
	let leaf = vcpu.static_cpuid_data().ext_vendor;
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_ext_cpu_feat_id(vcpu: &mut dyn CpuidHandlerOps)
{
	let mut leaf = vcpu.static_cpuid_data().ext_cpu_feat_id;
	// TODO: virtualize APIC MSR.
	let apic = ApicBar::from_bits(rdmsr(MSR_APIC_BASE));
	leaf.set_apic(apic.enable());
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_cpu_brand(vcpu: &mut dyn CpuidHandlerOps)
{
	let index = (vcpu.get_gpr(0) as u32 - CPUID_EXT_BRAND_STRING_P1) as usize;
	let result = vcpu.static_cpuid_data().cpu_brand.as_results()[index];
	vcpu.set_gpr(0, result.eax as u64);
	vcpu.set_gpr(1, result.ecx as u64);
	vcpu.set_gpr(2, result.edx as u64);
	vcpu.set_gpr(3, result.ebx as u64);
}

fn handle_cpuid_cache_tlb(vcpu: &mut dyn CpuidHandlerOps)
{
	let index = (vcpu.get_gpr(0) as u32 - CPUID_EXT_L1_CACHE_TLBS) as usize;
	let result = vcpu.static_cpuid_data().cache_tlb.as_results()[index];
	vcpu.set_gpr(0, result.eax as u64);
	vcpu.set_gpr(1, result.ecx as u64);
	vcpu.set_gpr(2, result.edx as u64);
	vcpu.set_gpr(3, result.ebx as u64);
}

fn handle_cpuid_proc_cap_param(vcpu: &mut dyn CpuidHandlerOps)
{
	let leaf = vcpu.static_cpuid_data().proc_cap_param;
	emulate_cpuid(vcpu, &leaf);
}

fn handle_cpuid_proc_topo_info(vcpu: &mut dyn CpuidHandlerOps)
{
	let leaf = vcpu.static_cpuid_data().proc_topo_info;
	emulate_cpuid(vcpu, &leaf);
}

pub type CpuidHandler = fn(vcpu: &mut dyn CpuidHandlerOps);

static CPUID_STD_LEAF_HANDLERS: [CpuidHandler; 0xE] = {
	let mut x: [CpuidHandler; _] = [handle_unknown; _];
	x[CPUID_STD_MAX_NUMBER_VENDOR_STRING as usize] = handle_cpuid_std_vendor;
	x[CPUID_STD_PROCESSOR_FEATURE as usize] = handle_cpuid_std_cpu_feat_id;
	x[CPUID_STD_MONITOR_FEATURE as usize] = handle_cpuid_monitor_mwait;
	x[CPUID_STD_THERMAL_FEATURE as usize] = handle_cpuid_power_mgr_feat;
	x[CPUID_STD_STRUCTURED_EXTENDED_FEATURE_ID as usize] = handle_cpuid_struct_ext_feat;
	x[CPUID_STD_EXTENDED_TOPOLOGY_INFORMATION as usize] = handle_cpuid_ext_topo_enum;
	x[CPUID_STD_PROCESSOR_EXTENDED_STATE_ENUMERATION as usize] = handle_cpuid_proc_xstate;
	x
};

static CPUID_HV_LEAF_HANDLERS: [CpuidHandler; 7] = [
	handle_cpuid_hv_leaf_generic::<MaxHypervisorLeafAndVendorString>,
	handle_cpuid_hv_leaf_generic::<HypervisorVendorNeutralInterface>,
	handle_cpuid_hv_leaf_generic::<HypervisorSystemIdentity>,
	handle_cpuid_hv_leaf_generic::<HypervisorFeatureId>,
	handle_cpuid_hv_leaf_generic::<HypervisorImplementationRecommendation>,
	handle_cpuid_hv_leaf_generic::<HypervisorImplementationLimits>,
	handle_cpuid_hv_leaf_generic::<HypervisorImplementationHardwareFeatures>,
];

static CPUID_EXT_LEAF_HANDLERS: [CpuidHandler; 0x27] = {
	let mut x: [CpuidHandler; _] = [handle_unknown; _];
	x[0] = handle_cpuid_ext_vendor;
	x[(CPUID_EXT_PROCESSOR_FEATURE - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] = handle_cpuid_ext_cpu_feat_id;
	x[(CPUID_EXT_BRAND_STRING_P1 - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] = handle_cpuid_cpu_brand;
	x[(CPUID_EXT_BRAND_STRING_P2 - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] = handle_cpuid_cpu_brand;
	x[(CPUID_EXT_BRAND_STRING_P3 - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] = handle_cpuid_cpu_brand;
	x[(CPUID_EXT_L1_CACHE_TLBS - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] = handle_cpuid_cache_tlb;
	x[(CPUID_EXT_L2_L3_CACHE_TLBS - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] = handle_cpuid_cache_tlb;
	x[(CPUID_EXT_PROCESSOR_CAPABILITY_PARAMETERS_EXTENDED_ID - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] =
		handle_cpuid_proc_cap_param;
	x[(CPUID_EXT_PROCESSOR_TOPOLOGY_INFORMATION - CPUID_EXT_MAX_NUMBER_VENDOR_STRING) as usize] = handle_cpuid_proc_topo_info;
	x
};

static CPUID_HANDLER_GROUPS: [&[CpuidHandler]; 4] = [
	// Standard leaves
	&CPUID_STD_LEAF_HANDLERS,
	// Hypervisor leaves
	&CPUID_HV_LEAF_HANDLERS,
	// Extended leaves
	&CPUID_EXT_LEAF_HANDLERS,
	// Unknown leaves
	&[],
];

pub fn dispatch_cpuid_handler(leaf: u32) -> CpuidHandler
{
	// Handling CPUID can be optimized into O(1) time complexity.
	let class = (leaf >> 30) as usize;
	let index = (leaf & 0x3FFFFFFF) as usize;
	CPUID_HANDLER_GROUPS[class].get(index).copied().unwrap_or(handle_unknown)
}
