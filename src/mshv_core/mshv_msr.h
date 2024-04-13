/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file includes definitions of Synthetic MSRs for MSHV-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /mshv_core/mshv_msr.h
*/

#include <nvdef.h>

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