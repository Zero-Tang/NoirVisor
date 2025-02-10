/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file manages VMCS in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{arch::x86_64::_bittest, fmt::{self,Display}};
use paste::paste;

use crate::{xpf_core::{asm::vt::*, x86::{crdr::DR6_BS, rflags::RFLAGS_TF_BIT}},*};

// 16-Bit Control Fields
pub const GUEST_VPID:usize=0x0;
pub const POSTED_INTERRUPT_NOTIFICATION_VECTOR:usize=0x2;
pub const EPT_POINTER_INDEX:usize=0x4;
pub const HLAT_PREFIX_SIZE:usize=0x6;
pub const LAST_PID_POINTER_INDEX:usize=0x8;
// 16-Bit Guest State Fields
pub const GUEST_ES_SELECTOR:usize=0x800;
pub const GUEST_CS_SELECTOR:usize=0x802;
pub const GUEST_SS_SELECTOR:usize=0x804;
pub const GUEST_DS_SELECTOR:usize=0x806;
pub const GUEST_FS_SELECTOR:usize=0x808;
pub const GUEST_GS_SELECTOR:usize=0x80A;
pub const GUEST_LDTR_SELECTOR:usize=0x80C;
pub const GUEST_TR_SELECTOR:usize=0x80E;
pub const GUEST_INTERRUPT_STATUS:usize=0x810;
pub const PML_INDEX:usize=0x812;
pub const GUEST_UINTV:usize=0x814;
// 16-Bit Host State Fields
pub const HOST_ES_SELECTOR:usize=0xC00;
pub const HOST_CS_SELECTOR:usize=0xC02;
pub const HOST_SS_SELECTOR:usize=0xC04;
pub const HOST_DS_SELECTOR:usize=0xC06;
pub const HOST_FS_SELECTOR:usize=0xC08;
pub const HOST_GS_SELECTOR:usize=0xC0A;
pub const HOST_TR_SELECTOR:usize=0xC0C;
// 64-Bit Control Fields
pub const ADDRESS_OF_IO_BITMAP_A:usize=0x2000;
pub const ADDRESS_OF_IO_BITMAP_B:usize=0x2002;
pub const ADDRESS_OF_MSR_BITMAP:usize=0x2004;
pub const VMEXIT_MSR_STORE_ADDRESS:usize=0x2006;
pub const VMEXIT_MSR_LOAD_ADDRESS:usize=0x2008;
pub const VMENTRY_MSR_LOAD_ADDRESS:usize=0x200A;
pub const EXECUTIVE_VMCS_POINTER:usize=0x200C;
pub const PML_ADDRESS:usize=0x200E;
pub const TSC_OFFSET:usize=0x2010;
pub const VIRTUAL_APIC_ADDRESS:usize=0x2012;
pub const APIC_ACCESS_ADDRESS:usize=0x2014;
pub const POSTED_INTERRUPT_DESCRIPTOR_ADDRESS:usize=0x2016;
pub const VM_FUNCTION_CONTROLS:usize=0x2018;
pub const EPT_POINTER:usize=0x201A;
pub const EOI_EXIT_BITMAP0:usize=0x201C;
pub const EOI_EXIT_BITMAP1:usize=0x201E;
pub const EOI_EXIT_BITMAP2:usize=0x2020;
pub const EOI_EXIT_BITMAP3:usize=0x2022;
pub const EPTP_LIST_ADDRESS:usize=0x2024;
pub const VMREAD_BITMAP_ADDRESS:usize=0x2026;
pub const VMWRITE_BITMAP_ADDRESS:usize=0x2028;
pub const VE_INFORMATION_ADDRESS:usize=0x202A;
pub const XSS_EXITING_BITMAP:usize=0x202C;
pub const ENCLS_EXITING_BITMAP:usize=0x202E;
pub const SUB_PAGE_PERMISSION_TABLE:usize=0x2030;
pub const TSC_MULTIPLIER:usize=0x2032;
pub const TERTIARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS:usize=0x2034;
pub const ENCLV_EXITING_BITMAP:usize=0x2036;
pub const LOW_PASID_DIRECTORY_ADDRESS:usize=0x2038;
pub const HIGH_PASID_DIRECTORY_ADDRESS:usize=0x203A;
pub const SHARED_EPT_POINTER:usize=0x203C;
pub const PCONFIG_EXITING_BITMAP:usize=0x203E;
pub const HLAT_POINTER:usize=0x2040;
pub const PID_POINTER_TABLE_ADDRESS:usize=0x2042;
pub const SECONDARY_VMEXIT_CONTROLS:usize=0x2044;
pub const SPEC_CTRL_MASK:usize=0x204A;
pub const SPEC_CTRL_SHADOW:usize=0x204C;
// 64-Bit Read-Only Fields
pub const GUEST_PHYSICAL_ADDRESS:usize=0x2400;
// 64-Bit Guest State Fields
pub const VMCS_LINK_POINTER:usize=0x2800;
pub const GUEST_MSR_IA32_DEBUG_CTRL:usize=0x2802;
pub const GUEST_MSR_IA32_PAT:usize=0x2804;
pub const GUEST_MSR_IA32_EFER:usize=0x2806;
pub const GUEST_MSR_IA32_PERF_GLOBAL_CTRL:usize=0x2808;
pub const GUEST_PDPTE0:usize=0x280A;
pub const GUEST_PDPTE1:usize=0x280C;
pub const GUEST_PDPTE2:usize=0x280E;
pub const GUEST_PDPTE3:usize=0x2810;
pub const GUEST_MSR_IA32_BOUND_CONFIG:usize=0x2812;
pub const GUEST_MSR_IA32_RTIT_CTRL:usize=0x2814;
pub const GUEST_MSR_IA32_LBR_CTRL:usize=0x2816;
pub const GUEST_MSR_IA32_PKRS:usize=0x2818;
// 64-Bit Host State Fields
pub const HOST_MSR_IA32_PAT:usize=0x2C00;
pub const HOST_MSR_IA32_EFER:usize=0x2C02;
pub const HOST_MSR_IA32_PERF_GLOBAL_CTRL:usize=0x2C04;
pub const HOST_MSR_IA32_PKRS:usize=0x2C06;
// 32-Bit Control Fields
pub const PIN_BASED_VM_EXECUTION_CONTROLS:usize=0x4000;
pub const PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS:usize=0x4002;
pub const EXCEPTION_BITMAP:usize=0x4004;
pub const PAGE_FAULT_ERROR_CODE_MASK:usize=0x4006;
pub const PAGE_FAULT_ERROR_CODE_MATCH:usize=0x4008;
pub const CR3_TARGET_COUNT:usize=0x400A;
pub const VMEXIT_CONTROLS:usize=0x400C;
pub const VMEXIT_MSR_STORE_COUNT:usize=0x400E;
pub const VMEXIT_MSR_LOAD_COUNT:usize=0x4010;
pub const VMENTRY_CONTROLS:usize=0x4012;
pub const VMENTRY_MSR_LOAD_COUNT:usize=0x4014;
pub const VMENTRY_INTERRUPTION_INFORMATION_FIELD:usize=0x4016;
pub const VMENTRY_EXCEPTION_ERROR_CODE:usize=0x4018;
pub const VMENTRY_INSTRUCTION_LENGTH:usize=0x401A;
pub const TPR_THRESHOLD:usize=0x401C;
pub const SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS:usize=0x401E;
pub const PLE_GAP:usize=0x4020;
pub const PLE_WINDOW:usize=0x4022;
pub const INSTRUCTION_TIMEOUT_CONTROL:usize=0x4024;
// 32-Bit Read-Only Fields
pub const VM_INSTRUCTION_ERROR:usize=0x4400;
pub const VMEXIT_REASON:usize=0x4402;
pub const VMEXIT_INTERRUPTION_INFORMATION:usize=0x4404;
pub const VMEXIT_INTERRUPTION_ERROR_CODE:usize=0x4406;
pub const IDT_VECTORING_INFORMATION:usize=0x4408;
pub const IDT_VECTORING_ERROR_CODE:usize=0x440A;
pub const VMEXIT_INSTRUCTION_LENGTH:usize=0x440C;
pub const VMEXIT_INSTRUCTION_INFORMATION:usize=0x440E;
// 32-Bit Guest State Fields
pub const GUEST_ES_LIMIT:usize=0x4800;
pub const GUEST_CS_LIMIT:usize=0x4802;
pub const GUEST_SS_LIMIT:usize=0x4804;
pub const GUEST_DS_LIMIT:usize=0x4806;
pub const GUEST_FS_LIMIT:usize=0x4808;
pub const GUEST_GS_LIMIT:usize=0x480A;
pub const GUEST_LDTR_LIMIT:usize=0x480C;
pub const GUEST_TR_LIMIT:usize=0x480E;
pub const GUEST_GDTR_LIMIT:usize=0x4810;
pub const GUEST_IDTR_LIMIT:usize=0x4812;
pub const GUEST_ES_ACCESS_RIGHTS:usize=0x4814;
pub const GUEST_CS_ACCESS_RIGHTS:usize=0x4816;
pub const GUEST_SS_ACCESS_RIGHTS:usize=0x4818;
pub const GUEST_DS_ACCESS_RIGHTS:usize=0x481A;
pub const GUEST_FS_ACCESS_RIGHTS:usize=0x481C;
pub const GUEST_GS_ACCESS_RIGHTS:usize=0x481E;
pub const GUEST_LDTR_ACCESS_RIGHTS:usize=0x4820;
pub const GUEST_TR_ACCESS_RIGHTS:usize=0x4822;
pub const GUEST_INTERRUPTIBILITY_STATE:usize=0x4824;
pub const GUEST_ACTIVITY_STATE:usize=0x4826;
pub const GUEST_SMBASE:usize=0x4828;
pub const GUEST_MSR_IA32_SYSENTER_CS:usize=0x482A;
pub const VMX_PREEMPTION_TIMER_VALUE:usize=0x482E;
// 32-Bit Host State Fields
pub const HOST_MSR_IA32_SYSENTER_CS:usize=0x4C00;
// Natural-Width Control Fields
pub const CR0_GUEST_HOST_MASK:usize=0x6000;
pub const CR4_GUEST_HOST_MASK:usize=0x6002;
pub const CR0_READ_SHADOW:usize=0x6004;
pub const CR4_READ_SHADOW:usize=0x6006;
pub const CR3_TARGET_VALUE0:usize=0x6008;
pub const CR3_TARGET_VALUE1:usize=0x600A;
pub const CR3_TARGET_VALUE2:usize=0x600C;
pub const CR3_TARGET_VALUE3:usize=0x600E;
// Natural-Width Read-Only Fields
pub const VMEXIT_QUALIFICATION:usize=0x6400;
pub const IO_RCX:usize=0x6402;
pub const IO_RSI:usize=0x6404;
pub const IO_RDI:usize=0x6406;
pub const IO_RIP:usize=0x6408;
pub const GUEST_LINEAR_ADDRESS:usize=0x640A;
// Natural-Width Guest State Fields
pub const GUEST_CR0:usize=0x6800;
pub const GUEST_CR3:usize=0x6802;
pub const GUEST_CR4:usize=0x6804;
pub const GUEST_ES_BASE:usize=0x6806;
pub const GUEST_CS_BASE:usize=0x6808;
pub const GUEST_SS_BASE:usize=0x680A;
pub const GUEST_DS_BASE:usize=0x680C;
pub const GUEST_FS_BASE:usize=0x680E;
pub const GUEST_GS_BASE:usize=0x6810;
pub const GUEST_LDTR_BASE:usize=0x6812;
pub const GUEST_TR_BASE:usize=0x6814;
pub const GUEST_GDTR_BASE:usize=0x6816;
pub const GUEST_IDTR_BASE:usize=0x6818;
pub const GUEST_DR7:usize=0x681A;
pub const GUEST_RSP:usize=0x681C;
pub const GUEST_RIP:usize=0x681E;
pub const GUEST_RFLAGS:usize=0x6820;
pub const GUEST_PENDING_DEBUG_EXCEPTIONS:usize=0x6822;
pub const GUEST_MSR_IA32_SYSENTER_ESP:usize=0x6824;
pub const GUEST_MSR_IA32_SYSENTER_EIP:usize=0x6826;
pub const GUEST_S_CET:usize=0x6828;
pub const GUEST_SSP:usize=0x682A;
pub const GUEST_MSR_IA32_INTERRUPT_SSP_TABLE_ADDR:usize=0x682C;
// Natural-Width Host State Fields
pub const HOST_CR0:usize=0x6C00;
pub const HOST_CR3:usize=0x6C02;
pub const HOST_CR4:usize=0x6C04;
pub const HOST_FS_BASE:usize=0x6C06;
pub const HOST_GS_BASE:usize=0x6C08;
pub const HOST_TR_BASE:usize=0x6C0A;
pub const HOST_GDTR_BASE:usize=0x6C0C;
pub const HOST_IDTR_BASE:usize=0x6C0E;
pub const HOST_MSR_IA32_SYSENTER_ESP:usize=0x6C10;
pub const HOST_MSR_IA32_SYSENTER_EIP:usize=0x6C12;
pub const HOST_RSP:usize=0x6C14;
pub const HOST_RIP:usize=0x6C16;
pub const HOST_S_CET:usize=0x6C18;
pub const HOST_SSP:usize=0x6C1A;
pub const HOST_MSR_IA32_INTERRUPT_SSP_TABLE_ADDR:usize=0x6C1C;

#[inline] pub fn vt_attrib(selector:u16,attrib:u16)->u32
{
	let mut ar=SegmentAccessRights(attrib as u32);
	ar.set_unusable(selector==0);
	ar.0
}

#[inline] pub unsafe fn advance_rip()
{
	let mut gip=vmreadptr(GUEST_RIP).unwrap();
	let ins_len=vmread32(VMEXIT_INSTRUCTION_LENGTH).unwrap();
	let rflags=vmread32(GUEST_RFLAGS).unwrap() as i32;
	if _bittest(&raw const rflags,RFLAGS_TF_BIT as i32)!=0
	{
		// Single-Stepping is enabled! Inject #DB exception...
		let pending_de=vmreadptr(GUEST_PENDING_DEBUG_EXCEPTIONS).unwrap();
		vmwriteptr(GUEST_PENDING_DEBUG_EXCEPTIONS,pending_de|DR6_BS as usize);
		// Remove the interrupt shadowing.
		let mut interruptibility=InterruptibilityState(vmread32(GUEST_INTERRUPTIBILITY_STATE).unwrap());
		interruptibility.set_blocking_by_sti(false);
		interruptibility.set_blocking_by_mov_ss(false);
		vmwrite32(GUEST_INTERRUPTIBILITY_STATE,interruptibility.0);
	}
	gip=gip.wrapping_add(ins_len as usize);
	let cs_ar=SegmentAccessRights(vmread32(GUEST_CS_ACCESS_RIGHTS).unwrap());
	if !cs_ar.get_long_mode()
	{
		// The rip might overflow if the guest is not in long mode.
		gip&=0xFFFFFFFF;
	}
	vmwriteptr(GUEST_RIP,gip);
}

impl<T:Display> Display for VmxResult<T>
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		match self
		{
			VmxResult::Ok(v)=>write!(f,"VMX Instruction succeeded and returned {v}!"),
			VmxResult::Err(e)=>match VMX_INSTRUCTION_ERROR_MESSAGE.get(*e as usize)
			{
				Some(v)=>write!(f,"VMX Instruction failed! Reason: {}",*v),
				None=>write!(f,"VMX Instruction failed with invalid number {}!",*e)
			},
			VmxResult::NoVmcs=>write!(f,"VMX Instruction failed while no VMCS was loaded!")
		}
	}
}

pub const VMX_INSTRUCTION_ERROR_MESSAGE:[&str;0x20]=
[
	"Invalid Error, Number=0!",										// Error=0
	"vmcall is executed in VMX Root Operation!",					// Error=1
	"vmclear is given invalid physical address as operand!",		// Error=2
	"vmclear is given vmxon region as operand!",					// Error=3
	"vmlaunch is given non-clear vmcs as operand!",					// Error=4
	"vmresume is given non-launched vmcs as operand!",				// Error=5
	"vmresume is executed after vmxoff!",							// Error=6
	"VM-Entry failed due to invalid control fields!",				// Error=7
	"VM-Entry failed due to invalid host state!",					// Error=8
	"vmptrld is given invalid physical address as operand!",		// Error=9
	"vmptrld is given vmxon region as operand!",					// Error=10
	"vmptrld is given vmcs with incorrect revision id!",			// Error=11
	"vmread/vmwrite is given unsupported field as operand!",		// Error=12
	"vmwrite is given read-only field as operand!",					// Error=13
	"Invalid Error, Number=14!",									// Error=14
	"vmxon is executed in VMX Root Operation!",						// Error=15
	"VM-Entry failed due to invalid executive-vmcs!",				// Error=16
	"VM-Entry failed due to non-launched executive-vmcs!",			// Error=17
	// Error=18
	"VM-Entry failed due to executive-vmcs not vmxon region! (Are you attempting to deactivate dual-monitor treatment?)",
	// Error=19
	"VM-Entry failed due to non-clear vmcs! (Are you attempting to deactivate dual-monitor treatment?)",
	"vmcall is given invalid vmexit control fields!",				// Error=20
	"Invalid Error, Number=21!",									// Error=21
	// Error=22
	"vmcall is given incorrect mseg revision id! (Are you attempting to deactivate dual-monitor treatment?)",
	"vmxoff is executed under dual-monitor treatment!",				// Error=23
	// Error=24
	"vmcall is given invalid SMM-Monitor features! (Are you attempting to activate dual-monitor treatment?)",
	// Error=25
	"VM-Entry failed due to invalid VM-Execution Control! (Are attempting to return from SMM?)",
	"VM-Entry failed due to events blocked by mov ss!",				// Error=26,
	"Invalid Error, Number=27!",										// Error=27
	"Invalid Operand to invept/invvpid Instructions!",				// Error=28
	"Invalid Error, Number=29!",									// Error=29
	"Invalid Error, Number=30!",									// Error=30
	"Invalid Error, Number=31!"										// Error=31
];

#[repr(C)] pub struct VmxMsrAutoItem
{
	pub index:u32,
	reserved:u32,
	pub data:u64
}

impl VmxMsrAutoItem
{
	pub fn new(index:u32,data:u64)->Self
	{
		Self
		{
			index,
			reserved:0,
			data
		}
	}
}

pub struct SegmentAccessRights(pub u32);
impl SegmentAccessRights
{
	build_int_mut_method!(segment_type,0,4,u32);
	build_bit_mut_method!(descriptor_type,4);
	build_int_mut_method!(dpl,5,2,u32);
	build_bit_mut_method!(present,7);
	build_bit_mut_method!(avl,12);
	build_bit_mut_method!(long_mode,13);
	build_bit_mut_method!(default_size,14);
	build_bit_mut_method!(granularity,15);
	build_bit_mut_method!(unusable,16);
}

pub struct InterruptibilityState(pub u32);
impl InterruptibilityState
{
	build_bit_mut_method!(blocking_by_sti,0);
	build_bit_mut_method!(blocking_by_mov_ss,1);
	build_bit_mut_method!(blocking_by_smi,2);
	build_bit_mut_method!(blocking_by_nmi,3);
	build_bit_mut_method!(enclave_interruption,4);
}

pub struct VmxPinBasedControls(pub u32);
impl VmxPinBasedControls
{
	build_bit_mut_method!(external_interrupt_exiting,0);
	build_bit_mut_method!(nmi_exiting,3);
	build_bit_mut_method!(virtual_nmi,5);
	build_bit_mut_method!(activate_vmx_preemption_timer,6);
	build_bit_mut_method!(process_posted_interrupts,7);
}

pub struct VmxPrimaryProcessorControls(pub u32);
impl VmxPrimaryProcessorControls
{
	build_bit_mut_method!(interrupt_window_exiting,2);
	build_bit_mut_method!(use_tsc_offsetting,3);
	build_bit_mut_method!(hlt_exiting,7);
	build_bit_mut_method!(invlpg_exiting,9);
	build_bit_mut_method!(mwait_exiting,10);
	build_bit_mut_method!(rdpmc_exiting,11);
	build_bit_mut_method!(rdtsc_exiting,12);
	build_bit_mut_method!(cr3_load_exiting,15);
	build_bit_mut_method!(cr3_store_exiting,16);
	build_bit_mut_method!(activate_tertiary_controls,17);
	build_bit_mut_method!(cr8_load_exiting,19);
	build_bit_mut_method!(cr8_store_exiting,20);
	build_bit_mut_method!(use_tpr_shadow,21);
	build_bit_mut_method!(nmi_window_exiting,22);
	build_bit_mut_method!(mov_dr_exiting,23);
	build_bit_mut_method!(unconditional_io_exiting,24);
	build_bit_mut_method!(use_io_bitmap,25);
	build_bit_mut_method!(monitor_trap_flag,27);
	build_bit_mut_method!(use_msr_bitmap,28);
	build_bit_mut_method!(monitor_exiting,29);
	build_bit_mut_method!(pause_exiting,30);
	build_bit_mut_method!(activate_secondary_controls,31);
}

pub struct VmxSecondaryProcessorControls(pub u32);
impl VmxSecondaryProcessorControls
{
	build_bit_mut_method!(virtualize_apic_accesses,0);
	build_bit_mut_method!(enable_ept,1);
	build_bit_mut_method!(descriptor_table_exiting,2);
	build_bit_mut_method!(enable_rdtscp,3);
	build_bit_mut_method!(virtualize_x2apic_mode,4);
	build_bit_mut_method!(enable_vpid,5);
	build_bit_mut_method!(wbinvd_exiting,6);
	build_bit_mut_method!(unrestricted_guest,7);
	build_bit_mut_method!(apic_register_virtualization,8);
	build_bit_mut_method!(virtual_interrupt_delivery,9);
	build_bit_mut_method!(pause_loop_exiting,10);
	build_bit_mut_method!(rdrand_exiting,11);
	build_bit_mut_method!(enable_invpcid,12);
	build_bit_mut_method!(enable_vm_functions,13);
	build_bit_mut_method!(vmcs_shadowing,14);
	build_bit_mut_method!(encls_exiting,15);
	build_bit_mut_method!(rdseed_exiting,16);
	build_bit_mut_method!(enable_pml,17);
	build_bit_mut_method!(ept_violation_to_ve,18);
	build_bit_mut_method!(conceal_vmx_from_pt,19);
	build_bit_mut_method!(enable_xsaves_xrstors,20);
	build_bit_mut_method!(enable_pasid_translation,21);
	build_bit_mut_method!(ept_mode_based_execution_control,22);
	build_bit_mut_method!(ept_sub_page_write_permission,23);
	build_bit_mut_method!(pt_use_gpa,24);
	build_bit_mut_method!(use_tsc_scaling,25);
	build_bit_mut_method!(enable_umwait,26);
	build_bit_mut_method!(enable_pconfig,27);
	build_bit_mut_method!(enclv_exiting,28);
	build_bit_mut_method!(vmm_buslock_detection,30);
	build_bit_mut_method!(instruction_timeout,31);
}

pub struct VmxTertiaryProcessorControls(pub u64);
impl VmxTertiaryProcessorControls
{
	build_bit_mut_method!(loadiwkey_exiting,0);
	build_bit_mut_method!(enable_hlat,1);
	build_bit_mut_method!(ept_paging_write_control,2);
	build_bit_mut_method!(guest_paging_verification,3);
	build_bit_mut_method!(ipi_virtualization,4);
	build_bit_mut_method!(enable_msr_list_instruction,6);
	build_bit_mut_method!(virtualize_spec_ctrl,7);
}

pub struct VmxEptPointer(pub u64);
impl VmxEptPointer
{
	build_int_mut_method!(ept_memory_type,0,3,u64);
	build_int_mut_method!(page_walk_length,3,3,u64);
	build_bit_mut_method!(enable_ad_flags,6);
	build_bit_mut_method!(enable_sss_enforcement,7);
	build_int_mut_method!(eptp_pa,12,52,u64);
}

pub struct VmxExitControls(pub u32);
impl VmxExitControls
{
	build_bit_mut_method!(save_debug_controls,2);
	build_bit_mut_method!(host_address_space_size,9);
	build_bit_mut_method!(load_perf_global_ctrl,12);
	build_bit_mut_method!(acknowledge_interrupt_on_exit,15);
	build_bit_mut_method!(save_pat,18);
	build_bit_mut_method!(load_pat,19);
	build_bit_mut_method!(save_efer,20);
	build_bit_mut_method!(load_efer,21);
	build_bit_mut_method!(save_vmx_preemption_timer_value,22);
	build_bit_mut_method!(clear_bndcfgs,23);
	build_bit_mut_method!(conceal_vmx_from_pt,24);
	build_bit_mut_method!(clear_rtit_ctrl,25);
	build_bit_mut_method!(clear_lbr_ctrl,26);
	build_bit_mut_method!(clear_uinv,27);
	build_bit_mut_method!(load_cet,28);
	build_bit_mut_method!(load_pkrs,29);
	build_bit_mut_method!(save_perf_global_ctrl,30);
	build_bit_mut_method!(activate_secondary_controls,31);
}

pub struct VmxExitControls2(pub u64);
impl VmxExitControls2
{
	build_bit_mut_method!(prematurely_busy_shadow_stack,3);
}

pub struct VmxEntryControls(pub u32);
impl VmxEntryControls
{
	build_bit_mut_method!(load_debug_controls,2);
	build_bit_mut_method!(ia32e_mode_guest,9);
	build_bit_mut_method!(entry_to_smm,10);
	build_bit_mut_method!(deactivate_dual_monitor,11);
	build_bit_mut_method!(load_perf_global_ctrl,13);
	build_bit_mut_method!(load_pat,14);
	build_bit_mut_method!(load_efer,15);
	build_bit_mut_method!(load_bndcfgs,16);
	build_bit_mut_method!(conceal_vmx_from_pt,17);
	build_bit_mut_method!(load_rtit_ctrl,18);
	build_bit_mut_method!(load_uinv,19);
	build_bit_mut_method!(load_cet_state,20);
	build_bit_mut_method!(load_guest_lbr_ctrl,21);
	build_bit_mut_method!(load_pkrs,22);
}

pub struct VmxEntryInterruptionInformation(pub u32);
impl VmxEntryInterruptionInformation
{
	build_int_mut_method!(vector,0,8,u32);
	build_int_mut_method!(interruption_type,8,3,u32);
	build_bit_mut_method!(deliver_error_code,11);
	build_bit_mut_method!(valid,31);
}

pub struct VmxExitReason(pub u32);
impl VmxExitReason
{
	build_int_mut_method!(basic_exit_reason,0,16,u32);
	build_bit_mut_method!(exit_causes_prematurely_busy_shadow_stack,25);
	build_bit_mut_method!(exit_after_buslock_assertion,26);
	build_bit_mut_method!(exit_in_enclave,27);
	build_bit_mut_method!(pending_mtf_exit,28);
	build_bit_mut_method!(exit_from_root_operation,29);
	build_bit_mut_method!(entry_failure,31);
}

pub struct VmxExitInterruptionInformation(pub u32);
impl VmxExitInterruptionInformation
{
	build_int_mut_method!(vector,0,8,u32);
	build_int_mut_method!(interruption_type,8,3,u32);
	build_bit_mut_method!(error_code_valid,11);
	build_bit_mut_method!(nmi_unblocking_due_to_iret,12);
	build_bit_mut_method!(valid,31);
}

pub struct VmxIdtVectoringInformation(pub u32);
impl VmxIdtVectoringInformation
{
	build_int_mut_method!(vector,0,8,u32);
	build_int_mut_method!(interruption_type,8,3,u32);
	build_bit_mut_method!(error_code_valid,11);
	build_bit_mut_method!(valid,31);
}

macro_rules! derive_qualification_reader
{
	() =>
	{
		#[inline] pub fn read()->Self
		{
			unsafe
			{
				Self(vmreadptr(VMEXIT_QUALIFICATION).unwrap())
			}
		}
	};
}

pub struct DebugExceptionQualification(pub usize);
impl DebugExceptionQualification
{
	derive_qualification_reader!();
	build_bit_mut_method!(b0,0);
	build_bit_mut_method!(b1,1);
	build_bit_mut_method!(b2,2);
	build_bit_mut_method!(b3,3);
	build_bit_mut_method!(bd,13);
	build_bit_mut_method!(bs,14);
	build_bit_mut_method!(rtm,16);
}

pub struct TaskSwitchQualification(pub usize);
impl TaskSwitchQualification
{
	derive_qualification_reader!();
	build_int_mut_method!(tss_selector,0,16,usize);
	build_int_mut_method!(source,30,2,usize);

	pub const CALL_INSTRUCTION:usize=0;
	pub const IRET_INSTRUCTION:usize=1;
	pub const JMP_INSTRUCTION:usize=2;
	pub const TASK_GATE:usize=3;
}

pub struct ControlRegisterQualification(pub usize);
impl ControlRegisterQualification
{
	derive_qualification_reader!();
	build_int_mut_method!(cr_index,0,4,usize);
	build_int_mut_method!(access_type,4,2,usize);
	build_bit_mut_method!(is_lmsw_memory_op,6);
	build_int_mut_method!(gpr_index,8,4,usize);
	build_int_mut_method!(lmsw_data,16,16,usize);

	pub const WRITE_CR:usize=0;
	pub const READ_CR:usize=1;
	pub const CLTS_OP:usize=2;
	pub const LMSW_OP:usize=3;
}

pub struct DebugRegisterQualification(pub usize);
impl DebugRegisterQualification
{
	derive_qualification_reader!();
	build_int_mut_method!(dr_index,0,3,usize);
	build_bit_mut_method!(is_read,4);
	build_int_mut_method!(gpr_index,8,4,usize);
}

pub struct IoQualification(pub usize);
impl IoQualification
{
	derive_qualification_reader!();
	build_int_mut_method!(access_size,0,3,usize);
	build_bit_mut_method!(is_input,3);
	build_bit_mut_method!(is_string,4);
	build_bit_mut_method!(is_repeat,5);
	build_bit_mut_method!(is_immediate,6);
	build_int_mut_method!(port_number,16,16,usize);
}

pub struct ApicAccessQualification(pub usize);
impl ApicAccessQualification
{
	derive_qualification_reader!();
	build_int_mut_method!(apic_offset,0,12,usize);
	build_int_mut_method!(access_type,12,4,usize);
	build_bit_mut_method!(async_op,16);

	pub const LINEAR_READ:usize=0;
	pub const LINEAR_WRITE:usize=1;
	pub const LINEAR_EXECUTE:usize=2;
	pub const LINEAR_ACCESS_DURING_EVENT_DELIVERY:usize=3;
	pub const LINEAR_ACCESS_FOR_MONITORING:usize=4;
	pub const GUEST_PHYSICAL_ACCESS_DURING_EVENT_DELIVERY:usize=10;
	pub const GUEST_PHYSICAL_ACCESS_FOR_MONITORING:usize=11;
	pub const GUEST_PHYSICAL_EXECUTE:usize=15;
}

pub struct EptViolationQualification(pub usize);
impl EptViolationQualification
{
	derive_qualification_reader!();
	build_bit_mut_method!(is_read,0);
	build_bit_mut_method!(is_write,1);
	build_bit_mut_method!(is_execute,2);
	build_bit_mut_method!(is_readable,3);
	build_bit_mut_method!(is_writable,4);
	build_bit_mut_method!(is_executable,5);
	build_bit_mut_method!(is_user_executable,6);
	build_bit_mut_method!(linear_address_valid,7);
	build_bit_mut_method!(caused_by_gpa_to_hpa,8);
	build_bit_mut_method!(linear_address_is_user,9);
	build_bit_mut_method!(linear_address_is_writable,10);
	build_bit_mut_method!(linear_address_is_no_execute,11);
	build_bit_mut_method!(nmi_unblocking_due_to_iret,12);
	build_bit_mut_method!(is_shadow_stack,13);
}