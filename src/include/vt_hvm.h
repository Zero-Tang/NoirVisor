/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines structures and constants for VMX Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

#define noir_vt_callexit		1

#define noir_nvt_vmxe			0
#define noir_nvt_vmxon			1

// Definition of Enabled Features
#define noir_vt_extended_paging		1		// Bit	0
#define noir_vt_vpid_tagged_tlb		2		// Bit	1
#define noir_vt_vmcs_shadowing		4		// Bit	2
#define noir_vt_cpuid_caching		8		// Bit	3
#define noir_vt_ept_with_hooks		16		// Bit	4

typedef struct _memory_descriptor
{
	void* virt;
	u64 phys;
}memory_descriptor,*memory_descriptor_p;

typedef struct _noir_vt_hvm
{
	memory_descriptor msr_bitmap;
	memory_descriptor io_bitmap_a;
	memory_descriptor io_bitmap_b;
	memory_descriptor msr_auto_list;
	u32 std_leaftotal;
	u32 hvm_leaftotal;
	u32 ext_leaftotal;
}noir_vt_hvm,*noir_vt_hvm_p;

typedef struct _noir_vt_msr_entry
{
	u32 index;
	u32 reserved;
	u64 value;
}noir_vt_msr_entry,*noir_vt_msr_entry_p;

typedef struct _noir_vt_cpuid_info
{
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
}noir_vt_cpuid_info,*noir_vt_cpuid_info_p;

typedef struct _noir_vt_cached_cpuid
{
	void** std_leaf;		// 0x00000000 - 0x0FFFFFFF
	void** hvm_leaf;		// 0x40000000 - 0x4FFFFFFF
	void** ext_leaf;		// 0x80000000 - 0x8FFFFFFF
	void** res_leaf;		// 0xC0000000 - 0xC0000000
	void* cache_base;
	u32 max_leaf[4];
}noir_vt_cached_cpuid,*noir_vt_cached_cpuid_p;

typedef struct _noir_vt_nested_vcpu
{
	memory_descriptor vmxon;
	// Abstracted-to-user VMCS.
	memory_descriptor vmcs_c;
	// Abstracted-to-CPU VMCS.
	memory_descriptor vmcs_t;
	u64 vmx_msr[0x12];
	u32 status;
}noir_vt_nested_vcpu,*noir_vt_nested_vcpu_p;

typedef struct _noir_vt_vcpu
{
	memory_descriptor vmxon;
	memory_descriptor vmcs;
	void* hv_stack;
	noir_vt_hvm_p relative_hvm;
	void* ept_manager;
	noir_vt_cached_cpuid cpuid_cache;
	noir_vt_nested_vcpu nested_vcpu;
	u8 status;
	u8 enabled_feature;
}noir_vt_vcpu,*noir_vt_vcpu_p;

u8 fastcall nvc_vt_subvert_processor_a(noir_vt_vcpu_p vcpu);
noir_status nvc_vt_build_exit_handlers();
void nvc_vt_build_cpuid_cache_per_vcpu(noir_vt_vcpu_p vcpu);
void nvc_vt_teardown_exit_handlers();
bool nvc_vt_build_cpuid_handler(u32 std_count,u32 hvm_count,u32 ext_count,u32 res_count);
void nvc_vt_teardown_cpuid_handler();
void fastcall nvc_vt_reserved_cpuid_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void nvc_vt_resume_without_entry(noir_gpr_state_p state);
void nvc_vt_exit_handler_a();
void noir_vt_vmsuccess();
void noir_vt_vmfail_invalid();
void noir_vt_vmfail_valid();
void noir_vt_vmfail(noir_vt_nested_vcpu_p nested_vcpu,u32 message);
bool noir_vt_nested_vmread(void* vmcs,u32 encoding,ulong_ptr* data);
bool noir_vt_nested_vmwrite(void* vmcs,u32 encoding,ulong_ptr data);