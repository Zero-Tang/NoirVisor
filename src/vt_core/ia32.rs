/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
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
	use crate::{xpf_core::asm::msr::rdmsr, *};
	use vt_core::vmcs::*;
	use paste::paste;

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

	pub const MSR_FEATURE_CONTROL_LOCK:u64=0x1;
	pub const MSR_FEATURE_CONTROL_VMXON_IN_SMX:u64=0x2;
	pub const MSR_FEATURE_CONTROL_VMXON_OUT_SMX:u64=0x4;

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

	pub struct VmxBasicMsr(pub u64);
	impl VmxBasicMsr
	{
		build_int_get_method!(revision_id,0,31,u64);
		build_int_get_method!(region_size,32,12,u64);
		build_bit_get_method!(pa_width,48);
		build_bit_get_method!(dual_monitor,49);
		build_bit_get_method!(report_io_on_exit,54);
		build_bit_get_method!(use_true_msr,55);
		build_rdmsr_method!(MSR_VMX_BASIC);
	}

	macro_rules! build_allowed_fields
	{
		($type:tt) =>
		{
			#[inline] pub fn get_allowed0(&self)->$type
			{
				$type(self.0 as u32)
			}

			#[inline] pub fn get_allowed1(&self)->$type
			{
				$type((self.0>>32) as u32)
			}
		};
	}

	macro_rules! build_allowed_fields64
	{
		($type:tt) =>
		{
			#[inline] pub fn get_allowed(&self)->$type
			{
				$type(self.0)
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

	pub struct VmxMiscMsr(pub u64);
	impl VmxMiscMsr
	{
		build_int_get_method!(tsc_preemption_scale,0,5,u64);
		build_bit_get_method!(store_lma_to_entry_on_exit,5);
		build_bit_get_method!(support_hlt_state,6);
		build_bit_get_method!(support_shutdown_state,7);
		build_bit_get_method!(support_wait_for_sipi_state,8);
		build_bit_get_method!(allow_pt_in_vmx,14);
		build_bit_get_method!(allow_read_smbase_in_smm,15);
		build_int_get_method!(cr3_target_values,16,9,u64);
		build_int_get_method!(best_msr_store_count,25,3,u64);
		build_bit_get_method!(allow_unblock_smi,28);
		build_bit_get_method!(allow_vmcs_write_anywhere,29);
		build_bit_get_method!(allow_null_injection,30);
		build_int_get_method!(mseg_revision_id,32,32,u64);
		build_rdmsr_method!(MSR_VMX_MISC);
	}

	pub struct VmxEptVpidCapMsr(pub u64);
	impl VmxEptVpidCapMsr
	{
		build_bit_get_method!(support_exec_only,0);
		build_bit_get_method!(support_lv4_page_walk,6);
		build_bit_get_method!(support_uc_ept,7);
		build_bit_get_method!(support_wb_ept,14);
		build_bit_get_method!(support_2mb_paging,16);
		build_bit_get_method!(support_1gb_paging,17);
		build_bit_get_method!(support_invept,20);
		build_bit_get_method!(support_ad_flags,21);
		build_bit_get_method!(report_advanced_eptv_exit_info,22);
		build_bit_get_method!(support_single_context_invept,25);
		build_bit_get_method!(support_global_context_invept,26);
		build_bit_get_method!(support_invvpid,32);
		build_bit_get_method!(support_ia_invvpid,40);
		build_bit_get_method!(support_sc_invvpid,41);
		build_bit_get_method!(support_ac_invvpid,42);
		build_bit_get_method!(support_scrg_invvpid,43);
		build_rdmsr_method!(MSR_VMX_EPT_VPID_CAP);
	}
}