/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines cpuid Exit Handler facility.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /vt_core/vt_cpuid.h
*/

#include <nvdef.h>

// This is Intel specific.
#define noir_vt_std_cpuid_submask		0x195a890

// Index of Standard Leaves
#define std_max_num_vstr		0x0
#define std_proc_feature		0x1
#define std_cache_tlb_ft		0x2
#define std_proc_serialn		0x3
#define std_det_cc_param		0x4
#define std_monitor_feat		0x5
#define std_thermal_feat		0x6
#define std_struct_extid		0x7
#define std_di_cache_inf		0x9
#define std_arch_perfmon		0xA
#define std_ex_proc_topo		0xB
#define std_pestate_enum		0xD
#define std_irdt_mon_cap		0xF
#define std_irdt_alloc_e		0x10
#define std_sgx_cap_enum		0x12
#define std_prtrace_enum		0x14
#define std_tsc_ncc_info		0x15
#define std_proc_frq_inf		0x16
#define std_sys_ocv_attr		0x17
#define std_atrans_param		0x18

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

// Index of Hypervisor Leaves
#define hvm_max_num_vstr		0x0
#define hvm_interface_id		0x1

#if defined(_vt_cpuid)
noir_vt_cpuid_exit_handler** vt_cpuid_handlers=null;
#endif