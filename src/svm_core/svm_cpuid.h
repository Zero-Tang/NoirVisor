/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines cpuid Exit Handler facility.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /svm_core/svm_cpuid.h
*/

#include <nvdef.h>

// This is AMD Specific
#define noir_svm_std_cpuid_submask		0x00002080
#define noir_svm_ext_cpuid_submask		0x20000000

// Index of Standard Leaves
#define std_max_num_vstr		0x0
#define std_proc_feature		0x1
#define std_monitor_feat		0x5
#define std_apic_timer_i		0x6
#define std_struct_extid		0x7
#define std_pestate_enum		0xD

// Index of Extended Leaves
#define ext_max_num_vstr		0x0
#define ext_proc_feature		0x1
#define ext_brand_str_p1		0x2
#define ext_brand_str_p2		0x3
#define ext_brand_str_p3		0x4
#define ext_caching_tlbs		0x5
#define ext_l23cache_tlb		0x6
#define ext_powermgr_ras		0x7
#define ext_pcap_prm_eid		0x8
#define ext_svm_features		0xA
#define ext_svm_tlbs_1gb		0x19
#define ext_ins_optimize		0x1A
#define ext_ins_sampling		0x1B
#define ext_lw_profiling		0x1C
#define ext_cache_topinf		0x1D
#define ext_proc_topoinf		0x1E
#define ext_mem_crypting		0x1F

// Index of Hypervisor Leaves
#define hvm_max_num_vstr		0x0
#define hvm_interface_id		0x1

typedef struct _noir_svm_cpuid_default
{
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
}noir_svm_cpuid_default,*noir_svm_cpuid_default_p;

typedef struct _noir_svm_cpuid_max_num_vstr
{
	u32 maximum;
	char vendor_string[12];
}noir_svm_cpuid_max_num_vstr,*noir_svm_cpuid_max_num_vstr_p;

typedef struct _noir_svm_cpuid_svm_feature_id
{
	u32 rev_number;
	u32 avail_asid;
	u32 reserved;
	u32 feature_id;
}noir_svm_cpuid_svm_feature_id,*noir_svm_cpuid_svm_feature_id_p;

typedef void (fastcall *noir_svm_cpuid_exit_handler)
(
 noir_gpr_state_p gpr_state,
 noir_svm_vcpu_p vcpu
);

#if defined(_svm_cpuid)
noir_svm_cpuid_exit_handler** svm_cpuid_handlers=null;
#endif