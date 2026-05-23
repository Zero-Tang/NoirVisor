/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines IA32-specific constants for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

pub mod cpuid
{
	// Do not exhaustively enumerate CPUID bit fields. Only list what we are interested in.
	// CPUID Flags for Extended Processor and Feature Identifiers.
	/// Use this flag for CPUID[EAX=0x1].ECX
	pub const CPUID_VMX:u32=0x20;
}

pub mod msr
{
	use bitfield_struct::bitfield;

	use crate::*;
	use vt_core::vmcs::*;
	use xpf_core::asm::msr::rdmsr;

	pub const MSR_FEATURE_CONTROL:u32=0x3A;
	pub const MSR_BIOS_UPDATE_TRIGGER:u32=0x79;

	pub const MSR_VMX_BASIC:u32=0x480;
	pub const MSR_VMX_PIN_BASED_CTLS:u32=0x481;
	pub const MSR_VMX_PROC_BASED_CTLS:u32=0x482;
	pub const MSR_VMX_EXIT_CTLS:u32=0x483;
	pub const MSR_VMX_ENTRY_CTLS:u32=0x484;
	pub const MSR_VMX_MISC:u32=0x485;
	pub const MSR_VMX_CR0_FIXED0:u32=0x486;
	pub const MSR_VMX_CR0_FIXED1:u32=0x487;
	pub const MSR_VMX_CR4_FIXED0:u32=0x488;
	pub const MSR_VMX_CR4_FIXED1:u32=0x489;
	pub const MSR_VMX_VMCS_ENUM:u32=0x48A;
	pub const MSR_VMX_PROC_BASED_CTLS2:u32=0x48B;
	pub const MSR_VMX_EPT_VPID_CAP:u32=0x48C;
	pub const MSR_VMX_TRUE_PIN_BASED_CTLS:u32=0x48D;
	pub const MSR_VMX_TRUE_PROC_BASED_CTLS:u32=0x48E;
	pub const MSR_VMX_TRUE_EXIT_CTLS:u32=0x48F;
	pub const MSR_VMX_TRUE_ENTRY_CTLS:u32=0x490;
	pub const MSR_VMX_VMFUNC:u32=0x491;
	pub const MSR_VMX_PROC_BASED_CTLS3:u32=0x492;
	pub const MSR_VMX_EXIT_CTLS2:u32=0x493;

	macro_rules! build_rdmsr_method
	{
		($const:expr) =>
		{
			#[inline] pub fn read()->Self
			{
				Self(rdmsr($const))
			}
		};

		($const:expr,$true_const:tt) =>
		{
			#[inline] pub fn read(use_true_msr:bool)->Self
			{
				Self(rdmsr(if use_true_msr {$true_const} else {$const}))
			}
		}
	}

	#[bitfield(u64)] pub struct Ia32FeatureControl
	{
		pub lock:bool,
		pub vmx_in_smx:bool,
		pub vmx_out_smx:bool,
		#[bits(5)] rsvd0:u8,
		#[bits(7)] pub senter_local:u8,
		pub senter_global:bool,
		rsvd1:bool,
		pub sgx_launch_control_enable:bool,
		pub sgx_global_enable:bool,
		rsvd2:bool,
		pub lmce_on:bool,
		#[bits(43)] rsvd3:u64
	}

	impl Ia32FeatureControl
	{
		build_rdmsr_method!(MSR_FEATURE_CONTROL);
	}

	#[bitfield(u64)] pub struct VmxBasicMsr
	{
		#[bits(31)] pub revision_id:u32,
		rsvd0:bool,
		#[bits(12)] pub region_size:u64,
		#[bits(4)] rsvd1:u64,
		pub pa_width:bool,
		pub dual_monitor:bool,
		#[bits(4)] rsvd2:u64,
		pub report_io_on_exit:bool,
		pub use_true_msr:bool,
		rsvd3:u8
	}

	impl VmxBasicMsr
	{
		build_rdmsr_method!(MSR_VMX_BASIC);
	}

	macro_rules! build_allowed_fields
	{
		($type:tt) =>
		{
			#[inline] pub fn get_allowed0(&self)->$type
			{
				$type::from_bits(self.0 as u32)
			}

			#[inline] pub fn get_allowed1(&self)->$type
			{
				$type::from_bits((self.0>>32) as u32)
			}
		};
	}

	macro_rules! build_allowed_fields64
	{
		($type:tt) =>
		{
			#[inline] pub fn get_allowed(&self)->$type
			{
				$type::from_bits(self.0)
			}
		};
	}

	pub struct VmxPinBasedCtrlMsr(pub u64);
	impl VmxPinBasedCtrlMsr
	{
		build_rdmsr_method!(MSR_VMX_PIN_BASED_CTLS,MSR_VMX_TRUE_PIN_BASED_CTLS);
		build_allowed_fields!(VmxPinBasedControls);
	}

	pub struct VmxPriProcCtrlMsr(pub u64);
	impl VmxPriProcCtrlMsr
	{
		build_rdmsr_method!(MSR_VMX_PROC_BASED_CTLS,MSR_VMX_TRUE_PROC_BASED_CTLS);
		build_allowed_fields!(VmxPrimaryProcessorControls);
	}

	pub struct VmxSecProcCtrlMsr(pub u64);
	impl VmxSecProcCtrlMsr
	{
		build_rdmsr_method!(MSR_VMX_PROC_BASED_CTLS2);
		build_allowed_fields!(VmxSecondaryProcessorControls);
	}

	pub struct VmxTerProcCtrlMsr(pub u64);
	impl VmxTerProcCtrlMsr
	{
		build_rdmsr_method!(MSR_VMX_PROC_BASED_CTLS3);
		build_allowed_fields64!(VmxTertiaryProcessorControls);
	}

	pub struct VmxExitCtrlMsr(pub u64);
	impl VmxExitCtrlMsr
	{
		build_rdmsr_method!(MSR_VMX_EXIT_CTLS,MSR_VMX_TRUE_EXIT_CTLS);
		build_allowed_fields!(VmxExitControls);
	}

	pub struct VmxExitCtrl2Msr(pub u64);
	impl VmxExitCtrl2Msr
	{
		build_rdmsr_method!(MSR_VMX_EXIT_CTLS2);
		build_allowed_fields64!(VmxExitControls2);
	}

	pub struct VmxEntryCtrlMsr(pub u64);
	impl VmxEntryCtrlMsr
	{
		build_rdmsr_method!(MSR_VMX_ENTRY_CTLS,MSR_VMX_TRUE_ENTRY_CTLS);
		build_allowed_fields!(VmxEntryControls);
	}

	#[bitfield(u64)] pub struct VmxMiscMsr
	{
		#[bits(5)] pub tsc_preemption_scale:u64,
		pub store_lma_to_entry_on_exit:bool,
		pub support_hlt_state:bool,
		pub support_shutdown_state:bool,
		pub support_wait_for_sipi_state:bool,
		#[bits(5)] rsvd0:u64,
		pub allow_pt_in_vmx:bool,
		pub allow_read_smbase_in_smm:bool,
		#[bits(9)] pub cr3_target_values:usize,
		#[bits(3)] pub best_msr_store_count:usize,
		pub allow_unblock_smi:bool,
		pub allow_vmcs_write_anywhere:bool,
		pub allow_null_injection:bool,
		rsvd1:bool,
		pub mseg_revision_id:u32
	}

	impl VmxMiscMsr
	{
		build_rdmsr_method!(MSR_VMX_MISC);
	}

	#[bitfield(u64)] pub struct VmxEptVpidCapMsr
	{
		pub support_exec_only:bool,
		#[bits(5)] rsvd0:u64,
		pub support_lv4_page_walk:bool,
		pub support_uc_ept:bool,
		#[bits(6)] rsvd1:u64,
		pub support_wb_ept:bool,
		rsvd2:bool,
		pub support_2mb_paging:bool,
		pub support_1gb_paging:bool,
		#[bits(2)] rsvd3:u64,
		pub support_invept:bool,
		pub support_ad_flags:bool,
		pub report_advanced_eptv_exit_info:bool,
		#[bits(2)] rsvd4:u64,
		pub support_single_context_invept:bool,
		pub support_global_context_invept:bool,
		#[bits(5)] rsvd5:u64,
		pub support_invvpid:bool,
		#[bits(7)] rsvd6:u64,
		pub support_ia_invvpid:bool,
		pub support_sc_invvpid:bool,
		pub support_ac_invvpid:bool,
		pub support_scrg_invvpid:bool,
		#[bits(20)] rsvd7:u64
	}

	impl VmxEptVpidCapMsr
	{
		build_rdmsr_method!(MSR_VMX_EPT_VPID_CAP);
	}
}