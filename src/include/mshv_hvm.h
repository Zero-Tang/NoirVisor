/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

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

#define noir_mshv_npiep_prevent_sgdt		0
#define noir_mshv_npiep_prevent_sidt		1
#define noir_mshv_npiep_prevent_sldt		2
#define noir_mshv_npiep_prevent_str			3

#define noir_mshv_npiep_prevent_sgdt_bit	0x1
#define noir_mshv_npiep_prevent_sidt_bit	0x2
#define noir_mshv_npiep_prevent_sldt_bit	0x4
#define noir_mshv_npiep_prevent_str_bit		0x8

#define noir_mshv_npiep_prevent_all			0xF

typedef struct _noir_mshv_vcpu
{
	void* root_vcpu;
	u64 npiep_config;
	// Typically, only Type-I NoirVisor on AMD-V will be using SynIC.
	struct
	{
		u64 eoi;
		u64 icr;
		u64 tpr;
	}local_synic;
}noir_mshv_vcpu,*noir_mshv_vcpu_p;