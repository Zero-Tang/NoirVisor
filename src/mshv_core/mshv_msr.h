/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file includes definitions of Synthetic MSRs for MSHV-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /mshv_core/mshv_msr.h
*/

#include <nvdef.h>

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

typedef union _noir_mshv_msr_proprietary_guest_os_id
{
	struct
	{
		u64 build_number:16;		// Bits	0-15
		u64 service_version:8;		// Bits	15-23
		u64 minor_version:8;		// Bits	24-31
		u64 major_version:8;		// Bits	32-39
		u64 os_id:8;				// Bits	40-47
		u64 vendor_id:15;			// Bits	48-62
		u64 os_type:1;				// Bit	63
	};
	u64 value;
}noir_mshv_msr_proprietary_guest_os_id,*noir_mshv_msr_proprietary_guest_os_id_p;

typedef enum _noir_mshv_known_proprietary_vendors
{
	vendor_microsoft=0x0001,
	vendor_hpe=0x0002,
	vendor_lancom=0x0200
}noir_mshv_known_proprietary_vendors,*noir_mshv_known_proprietary_vendors_p;

typedef union _noir_mshv_msr_open_source_guest_os_id
{
	struct
	{
		u64 build_number:16;		// Bits	0-15
		u64 version:32;				// Bits	16-47
		u64 os_id:8;				// Bits	48-55
		u64 os_type:7;				// Bits	56-62
		u64 open_source:1;			// Bit	63
	};
	u64 value;
}noir_mshv_msr_open_source_guest_os_id,*noir_mshv_msr_open_source_guest_os_id_p;

typedef enum _noir_mshv_known_open_source_os_types
{
	linux=0x1,
	freebsd=0x2,
	xen=0x3,
	illumos=0x4
}noir_mshv_known_open_source_os_types,*noir_mshv_known_open_source_os_types_p;

typedef union _noir_mshv_msr_hypercall
{
	struct
	{
		u64 enable:1;			// Bit	0
		u64 locked:1;			// Bit	1
		u64 reserved:10;		// Bits	2-11
		u64 hypercall_gpfn:52;	// Bits	12-63
	};
	u64 value;
}noir_mshv_msr_hypercall,*noir_mshv_msr_hypercall_p;

typedef u32 hv_vp_index;
#define hv_any_vp			((hv_vp_index)-1)
#define hv_vp_index_self	((hv_vp_index)-2)

// For minimal Hv#1 interface, only Synthetic MSRs 0x40000000-0x40000002 are required.
// Definitions for the rest of Synthetic MSRs are omitted at this moment,
// but will be detailedly defined in the future development.