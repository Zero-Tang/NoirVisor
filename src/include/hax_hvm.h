/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the NoirVisor CVM API Core Wrapper for Intel HAXM.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/hax_hvm.h
*/

#include <nvdef.h>
#include <nvbdk.h>

// Currently, we implement minimal HAXM functionality.
// QEMU 7.0.0 requires HAX version 4
#define noir_hax_current_version	0x0004
#define noir_hax_compat_version		0x0001

typedef struct _noir_hax_module_version
{
	u32 compat_version;
	u32 current_version;
}noir_hax_module_version,*noir_hax_module_version_p;

#define noir_hax_cap_status_working		0x01
#define noir_hax_cap_memquota			0x02
#define noir_hax_workstatus_mask		0x01

#define noir_hax_cap_failreason_vt		0x01
#define noir_hax_cap_failreason_nx		0x02

#define noir_hax_cap_ept				0x001
#define noir_hax_cap_fastmmio			0x002
#define noir_hax_cap_ug					0x004
#define noir_hax_cap_64bit_ramblock		0x008
#define noir_hax_cap_64bit_setram		0x010
#define noir_hax_cap_tunnel_page		0x020
#define noir_hax_cap_debug				0x080
#define noir_hax_cap_cpuid				0x100

typedef struct _noir_hax_capability_info
{
	u16 status;
	u16 info;
	u32 ref_count;
	u64 mem_quota;
}noir_hax_capability_info,*noir_hax_capability_info_p;

typedef struct _noir_hax_set_ram_info
{
	u64 gpa;
	u32 bytes;
	struct
	{
		u8 read_only:1;
		u8 reserved:6;
		u8 invalid:1;
	}flags;
	u8 pad[3];
	u64 hva;
}noir_hax_set_ram_info,*noir_hax_set_ram_info_p;

typedef struct _noir_hax_tunnel_info
{
	u64 va;
	u64 io_va;
	u16 size;
	u16 pad[3];
}noir_hax_tunnel_info,*noir_hax_tunnel_info_p;

/*
  QEMU on Intel HAXM only supports the following Exit Statuses:

  I/O Instruction
  Fast MMIO Operation
  State Change
  Unknown VM-Exit
  Halt Instruction
  Interrupt
  Pause Instruction
*/
typedef enum _noir_hax_exit_status
{
	hax_exit_io=1,
	hax_exit_mmio,
	hax_exit_real_mode,
	hax_exit_interrupt,
	hax_exit_unknown,
	hax_exit_hlt,
	hax_exit_state_change,
	hax_exit_paused,
	hax_exit_fast_mmio,
	hax_exit_page_fault,
	hax_exit_debug
}noir_hax_exit_status,*noir_hax_exit_status_p;

// HAX Tunnel is actually a storage for VM-Exit Information.
// It seems HAX Tunnel is duplex-interactive.
typedef struct _noir_hax_tunnel
{
	u32 exit_reason;
	u32 pad0;
	u32 exit_status;
	u32 user_event_pending;
	i32 ready_for_event_injection;
	i32 request_interrupt_window;
	union
	{
		struct
		{
			u8 direction;
			u8 df;
			u16 size;
			u16 port;
			u16 count;
			struct
			{
				u8 string:1;
				u8 rsvd:7;
			}flags;
			u8 pad0;
			u16 pad1;
			u32 pad2;
			u64 gva;
		}io;
		struct
		{
			u64 gpa;
		}mmio;
		struct
		{
			u64 gpa;
			struct
			{
				u32 acc_r:1;		// Bit	0
				u32 acc_w:1;		// Bit	1
				u32 acc_x:1;		// Bit	2
				u32 rsvd0:1;		// Bit	3
				u32 perm_r:1;		// Bit	4
				u32 perm_w:1;		// Bit	5
				u32 perm_x:1;		// Bit	6
				u32 rsvd1:25;		// Bits	7-31
			}flags;
			u32 reserved1;
			u32 reserved2;
		}pagefault;
		struct
		{
			u64 dummy;
		}state;
		struct
		{
			u64 rip;
			u64 dr6;
			u64 dr7;
		}debug;
	};
	u64 apic_base;
}noir_hax_tunnel,*noir_hax_tunnel_p;

// Intel HAXM implements 64 bytes at most.
#define noir_hax_io_string_max_size		64

typedef struct _noir_hax_fastmmio
{
	u64 gpa;
	union
	{
		u64 value;
		u64 gpa2;
	};
	u8 size;
	u8 direction;
	u16 reg_index;
	u32 pad;
	u64 cr0;
	u64 cr2;
	u64 cr3;
	u64 cr4;
}noir_hax_fastmmio,*noir_hax_fastmmio_p;

typedef struct _noir_hax_segment
{
	u16 selector;
	u16 dummy;
	u32 limit;
	u64 base;
	u32 access_rights;
	u32 pad;
}noir_hax_segment,*noir_hax_segment_p;

typedef struct _noir_hax_vcpu_state
{
	// General-Purpose Registers...
	noir_gpr_state gpr;
	u64 rip;
	u64 rflags;
	// Segment Registers...
	noir_hax_segment cs;
	noir_hax_segment ss;
	noir_hax_segment ds;
	noir_hax_segment es;
	noir_hax_segment fs;
	noir_hax_segment gs;
	noir_hax_segment ldtr;
	noir_hax_segment tr;
	noir_hax_segment gdtr;
	noir_hax_segment idtr;
	// Control Registers...
	u64 cr0;
	u64 cr2;
	u64 cr3;
	u64 cr4;
	// Debug Registers...
	u64 dr0;
	u64 dr1;
	u64 dr2;
	u64 dr3;
	u64 dr6;
	u64 dr7;
	u64 pde;
	u32 efer;
	u32 sysenter_cs;
	u64 sysenter_eip;
	u64 sysenter_esp;
	u32 activity_state;
	u32 pad;
	u64 interruptibility_state;
}noir_hax_vcpu_state,*noir_hax_vcpu_state_p;

// MSRs used by QEMU-HAX
#define hax_msr_tsc				0x10
#define hax_msr_sysenter_cs		0x174
#define hax_msr_sysenter_esp	0x175
#define hax_msr_sysenter_eip	0x176
#define hax_msr_efer			0xC0000080
#define hax_msr_star			0xC0000081
#define hax_msr_lstar			0xC0000082
#define hax_msr_cstar			0xC0000083
#define hax_msr_fmask			0xC0000084
#define hax_msr_kernel_gs_base	0xC0000102

#define hax_max_msr_array		20

typedef struct _noir_hax_vmx_msr
{
	u64 entry;
	u64 value;
}noir_hax_vmx_msr,*noir_hax_vmx_msr_p;

typedef struct _noir_hax_msr_data
{
	u16 nr_msr;
	u16 done;
	u16 pad[2];
	noir_hax_vmx_msr entries[hax_max_msr_array];
}noir_hax_msr_data,*noir_hax_msr_data_p;