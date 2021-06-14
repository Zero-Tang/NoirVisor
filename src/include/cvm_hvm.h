/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is header file for Customizable VM of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/cvm_hvm.h
*/

#include <nvdef.h>

typedef struct _noir_cr_state
{
	u64 cr0;
	u64 cr2;
	u64 cr3;
	u64 cr4;
	u64 cr8;
}noir_cr_state,*noir_cr_state_p;

typedef struct _noir_dr_state
{
	u64 dr0;
	u64 dr1;
	u64 dr2;
	u64 dr3;
	u64 dr6;
	u64 dr7;
}noir_dr_state,*noir_dr_state_p;

typedef struct _noir_msr_state
{
	u64 sysenter_cs;
	u64 sysenter_esp;
	u64 sysenter_eip;
	u64 pat;
	u64 efer;
	u64 star;
	u64 lstar;
	u64 cstar;
	u64 sfmask;
}noir_msr_state,*noir_msr_state_p;

typedef union _noir_cvm_vcpu_options
{
	struct
	{
		u32 intercept_cpuid:1;
		u32 intercept_rdmsr:1;
		u32 intercept_wrmsr:1;
		u32 intercept_io:1;
		u32 intercept_exceptions:1;
		u32 intercept_cr0:1;
		u32 intercept_cr2:1;
		u32 intercept_cr3:1;
		u32 intercept_cr4:1;
		u32 intercept_drx:1;
		u32 reserved:22;
	};
	u32 value;
}noir_cvm_vcpu_options,*noir_cvm_vcpu_options_p;

typedef struct _noir_cvm_virtual_cpu
{
	noir_gpr_state gpr;
	noir_fx_state fxs;
	noir_cr_state crs;
	noir_dr_state drs;
	noir_msr_state msrs;
	void* core;
	u64 rflags;
	u64 rip;
	noir_cvm_vcpu_options vcpu_options;
}noir_cvm_virtual_cpu,*noir_cvm_virtual_cpu_p;

typedef struct _noir_cvm_address_mapping
{
	u64 gpa;
	u64 hva;
	u32 pages;
	union
	{
		struct
		{
			u32 present:1;
			u32 write:1;
			u32 execute:1;
			u32 user:1;
			u32 caching:3;
			u32 psize:2;
			u32 reserved:23;
		};
		u32 value;
	}attributes;
}noir_cvm_address_mapping,*noir_cvm_address_mapping_p;

typedef struct _noir_cvm_virtual_machine
{
	u32 vcpu_count;
	u32 mappings_count;
	noir_cvm_virtual_cpu_p vcpu;
	noir_cvm_address_mapping mappings[1];
}noir_cvm_virtual_machine,*noir_cvm_virtual_machine_p;