/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file defines structures for Microsoft's Specification of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/mshv_hvm.h
*/

#include "nvdef.h"

// Microsoft Hypervisor CPUID Leaves Definitions
#define hvm_cpuid_leaf_range_and_vendor_string			0x40000000
#define hvm_cpuid_vendor_neutral_interface_id			0x40000001
#define hvm_cpuid_hypervisor_system_id					0x40000002
#define hvm_cpuid_hypervisor_feature_id					0x40000003
#define hvm_cpuid_implementation_recommendations		0x40000004
#define hvm_cpuid_implementation_limits					0x40000005
#define hvm_cpuid_implementation_hardware_features		0x40000006
#define hvm_cpuid_cpu_management_features				0x40000007
#define hvm_cpuid_shared_virtual_memory_features		0x40000008
#define hvm_cpuid_nested_hypervisor_feature_id			0x40000009
#define hvm_cpuid_nested_virtualization_features		0x4000000A

// Microsoft Hypervisor Synthetic MSRs Index Definitions
#define hv_x64_msr_guest_os_id					0x40000000
#define hv_x64_msr_hypercall					0x40000001
#define hv_x64_msr_vp_index						0x40000002
#define hv_x64_msr_reset						0x40000003
#define hv_x64_msr_vp_runtime					0x40000010
#define hv_x64_msr_time_ref_count				0x40000020
#define hv_x64_msr_reference_tsc				0x40000021
#define hv_x64_msr_tsc_frequency				0x40000022
#define hv_x64_msr_apic_frequency				0x40000023
#define hv_x64_msr_npiep_config					0x40000040
#define hv_x64_msr_eoi							0x40000070
#define hv_x64_msr_icr							0x40000071
#define hv_x64_msr_tpr							0x40000072
#define hv_x64_msr_vp_assist_page				0x40000073
#define hv_x64_msr_scontrol						0x40000080
#define hv_x64_msr_sversion						0x40000081
#define hv_x64_msr_siefp						0x40000082
#define hv_x64_msr_simp							0x40000083
#define hv_x64_msr_eom							0x40000084
#define hv_x64_msr_sint0						0x40000090
#define hv_x64_msr_sint1						0x40000091
#define hv_x64_msr_sint2						0x40000092
#define hv_x64_msr_sint3						0x40000093
#define hv_x64_msr_sint4						0x40000094
#define hv_x64_msr_sint5						0x40000095
#define hv_x64_msr_sint6						0x40000096
#define hv_x64_msr_sint7						0x40000097
#define hv_x64_msr_sint8						0x40000098
#define hv_x64_msr_sint9						0x40000099
#define hv_x64_msr_sint10						0x4000009A
#define hv_x64_msr_sint11						0x4000009B
#define hv_x64_msr_sint12						0x4000009C
#define hv_x64_msr_sint13						0x4000009D
#define hv_x64_msr_sint14						0x4000009E
#define hv_x64_msr_sint15						0x4000009F
#define hv_x64_msr_stimer0_config				0x400000B0
#define hv_x64_msr_stimer0_count				0x400000B1
#define hv_x64_msr_stimer1_config				0x400000B2
#define hv_x64_msr_stimer1_count				0x400000B3
#define hv_x64_msr_stimer2_config				0x400000B4
#define hv_x64_msr_stimer2_count				0x400000B5
#define hv_x64_msr_stimer3_config				0x400000B6
#define hv_x64_msr_stimer3_count				0x400000B7
#define hv_x64_msr_guest_idle					0x400000F0
#define hv_x64_msr_crash_p0						0x40000100
#define hv_x64_msr_crash_p1						0x40000101
#define hv_x64_msr_crash_p2						0x40000102
#define hv_x64_msr_crash_p3						0x40000103
#define hv_x64_msr_crash_p4						0x40000104
#define hv_x64_msr_crash_ctl					0x40000105
#define hv_x64_msr_reenlightenment_control		0x40000106
#define hv_x64_msr_tsc_emulation_control		0x40000107
#define hv_x64_msr_tsc_emulation_status			0x40000108
#define hv_x64_msr_stime_unhalted_timer_config	0x40000114
#define hv_x64_msr_stime_unhalted_timer_count	0x40000115
#define hv_x64_msr_nested_vp_index				0x40001002
#define hv_x64_msr_nested_scontrol				0x40001080
#define hv_x64_msr_nested_sversion				0x40001081
#define hv_x64_msr_nested_siefp					0x40001082
#define hv_x64_msr_nested_simp					0x40001083
#define hv_x64_msr_nested_eom					0x40001084
#define hv_x64_msr_nested_sint0					0x40001090
#define hv_x64_msr_nested_sint1					0x40001091
#define hv_x64_msr_nested_sint2					0x40001092
#define hv_x64_msr_nested_sint3					0x40001093
#define hv_x64_msr_nested_sint4					0x40001094
#define hv_x64_msr_nested_sint5					0x40001095
#define hv_x64_msr_nested_sint6					0x40001096
#define hv_x64_msr_nested_sint7					0x40001097
#define hv_x64_msr_nested_sint8					0x40001098
#define hv_x64_msr_nested_sint9					0x40001099
#define hv_x64_msr_nested_sint10				0x4000109A
#define hv_x64_msr_nested_sint11				0x4000109B
#define hv_x64_msr_nested_sint12				0x4000109C
#define hv_x64_msr_nested_sint13				0x4000109D
#define hv_x64_msr_nested_sint14				0x4000109E
#define hv_x64_msr_nested_sint15				0x4000109F

#define noir_mshv_npiep_prevent_sgdt		0
#define noir_mshv_npiep_prevent_sidt		1
#define noir_mshv_npiep_prevent_sldt		2
#define noir_mshv_npiep_prevent_str			3

#define noir_mshv_npiep_prevent_sgdt_bit	0x1
#define noir_mshv_npiep_prevent_sidt_bit	0x2
#define noir_mshv_npiep_prevent_sldt_bit	0x4
#define noir_mshv_npiep_prevent_str_bit		0x8

#define noir_mshv_npiep_prevent_all			0xF

#define noir_mshv_event_type_external_interrupt		0
#define noir_mshv_event_type_nonmaskable_interrupt	2
#define noir_mshv_event_type_hardware_exception		3
#define noir_mshv_event_type_software_exception		4

#define noir_mshv_exception_divide_error_fault			0
#define noir_mshv_exception_debug_fault_trap			1
#define noir_mshv_exception_nmi_interrupt				2
#define noir_mshv_exception_breakpoint_trap				3
#define noir_mshv_exception_overflow_trap				4
#define noir_mshv_exception_bound_range_fault			5
#define noir_mshv_exception_invalid_opcode_fault		6
#define noir_mshv_exception_device_na_fault				7
#define noir_mshv_exception_double_fault_abort			8
#define noir_mshv_exception_invalid_tss_fault			10
#define noir_mshv_exception_absent_segment_fault		11
#define noir_mshv_exception_stack_fault					12
#define noir_mshv_exception_general_protection_fault	13
#define noir_mshv_exception_page_fault					14
#define noir_mshv_exception_x87_pending_fault			16
#define noir_mshv_exception_alignment_check_fault		17
#define noir_mshv_exception_machine_check_fault			18
#define noir_mshv_exception_simd_fp_fault				19
#define noir_mshv_exception_control_protection_fault	21

typedef struct _noir_mshv_vcpu
{
	void* root_vcpu;
	u64 npiep_config;
	u64 vp_index;
	// Typically, only Type-I NoirVisor on AMD-V will be using SynIC.
	struct
	{
		u64 icr;
	}local_synic;
	// MSHV-Core may issue an event.
	union
	{
		struct
		{
			u64 event_vector:8;
			u64 event_type:3;
			u64 error_code_valid:1;
			u64 reserved:19;
			u64 valid:1;
			u64 error_code:32;
		};
		u64 value;
	}event;
}noir_mshv_vcpu,*noir_mshv_vcpu_p;