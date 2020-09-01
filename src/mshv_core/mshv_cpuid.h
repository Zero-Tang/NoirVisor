/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file includes definitions of CPUID Leaves for MSHV-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /mshv_core/mshv_cpuid.h
*/

#include <nvdef.h>

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

typedef struct _noir_mshv_cpuid_leaf_range
{
	u32 maximum;
	char vendor_id[12];
}noir_mshv_cpuid_leaf_range,*noir_mshv_cpuid_leaf_range_p;

typedef struct _noir_mshv_cpuid_vendor_neutral_inferface_id
{
	u32 interface_signature;
	u32 reserved[3];
}noir_mshv_cpuid_vendor_neutral_inferface_id,*noir_mshv_cpuid_vendor_neutral_inferface_id_p;

typedef struct _noir_mshv_cpuid_hypervisor_system_id
{
	u32 build_number;
	union
	{
		struct
		{
			u16 major_version;
			u16 minor_version;
		};
		u32 value;
	}version;
	u32 service_pack;
	union
	{
		struct
		{
			u32 service_number:24;
			u32 service_branch:8;
		};
		u32 value;
	}service_info;
}noir_mshv_cpuid_hypervisor_system_id,*noir_mshv_cpuid_hypervisor_system_id_p;

typedef struct _noir_mshv_cpuid_hypervisor_feature_id
{
	union
	{
		struct
		{
			u32 access_vp_runtime_msr:1;		// Bit	0
			u32 access_partition_ref_counter:1;	// Bit	1
			u32 access_synic_msrs:1;			// Bit	2
			u32 access_synthetic_timer_msrs:1;	// Bit	3
			u32 access_apic_msrs:1;				// Bit	4
			u32 access_hypercall_msrs:1;		// Bit	5
			u32 access_vp_index:1;				// Bit	6
			u32 access_reset_msr:1;				// Bit	7
			u32 access_stats_msr:1;				// Bit	8
			u32 access_partition_ref_tsc:1;		// Bit	9
			u32 access_guest_idle_msr:1;		// Bit	10
			u32 access_frequency_msrs:1;		// Bit	11
			u32 reserved:20;					// Bits	12-31
		};
		u32 value;
	}feat1;
	union
	{
		struct
		{
			u32 create_partitions:1;		// Bit	0
			u32 access_partition_id:1;		// Bit	1
			u32 access_memory_pool:1;		// Bit	2
			u32 adjust_message_buffers:1;	// Bit	3
			u32 port_messages:1;			// Bit	4
			u32 signal_events:1;			// Bit	5
			u32 create_port:1;				// Bit	6
			u32 connect_port:1;				// Bit	7
			u32 access_stats:1;				// Bit	8
			u32 reserved1:2;				// Bits	9-10
			u32 debugging:1;				// Bit	11
			u32 cpu_management:1;			// Bit	12
			u32 configure_profiler:1;		// Bit	13
			u32 reserved2:18;				// Bits	14-31
		};
		u32 value;
	}feat2;
	u32 power_mgmt;
	union
	{
		struct
		{
			u32 mwait_avail:1;				// Bit	0	Deprecated
			u32 guest_debug:1;				// Bit	1
			u32 performance_monitor:1;		// Bit	2
			u32 phys_cpu_dynp_event:1;		// Bit	3
			u32 hypercall_xmm_support:1;	// Bit	4
			u32 virt_guest_idle_state:1;	// Bit	5
			u32 hv_sleep_state:1;			// Bit	6
			u32 query_numa_distance:1;		// Bit	7
			u32 determine_timer_freq:1;		// Bit	8
			u32 inject_synthetic_mc:1;		// Bit	9
			u32 guest_crash_msr:1;			// Bit	10
			u32 guest_debug_msr:1;			// Bit	11
			u32 npiep:1;					// Bit	12
			u32 disable_hypervisor:1;		// Bit	13
			u32 ext_gvar_flush_va_list:1;	// Bit	14
			u32 ret_hypercall_by_xmm:1;		// Bit	15
			u32 reserved1:1;				// Bit	16
			u32 sint_polling_mode:1;		// Bit	17
			u32 hypercall_msr_lock:1;		// Bit	18
			u32 direct_synthetic_timer:1;	// Bit	19
			u32 avail_pat_for_vsm:1;		// Bit	20
			u32 avail_bndcfgs_vsm:1;		// Bit	21
			u32 reserved2:1;				// Bit	22
			u32 synthetic_unhalted_timer:1;	// Bit	23
			u32 reserved3:2;				// Bits	24-25
			u32 use_lbr_support:1;			// Bit	26
			u32 reserved4:5;				// Bits	27-31
		};
		u32 value;
	}feat3;
}noir_mshv_cpuid_hypervisor_feature_id,*noir_mshv_cpuid_hypervisor_feature_id_p;

typedef struct _noir_mshv_cpuid_implementation_recommendations
{
	union
	{
		struct
		{
			u32 mov_cr3:1;					// Bit	0
			u32 local_tlb:1;				// Bit	1
			u32 remote_tlb:1;				// Bit	2
			u32 msr_apic_access:1;			// Bit	3
			u32 msr_for_reset:1;			// Bit	4
			u32 relaxed_timing:1;			// Bit	5
			u32 dma_remap:1;				// Bit	6
			u32 int_remap:1;				// Bit	7
			u32 reserved1:1;				// Bit	8
			u32 deprecate_autoeoi:1;		// Bit	9
			u32 synth_clust_ipi:1;			// Bit	10
			u32 newer_exprocmask:1;			// Bit	11
			u32 nested_in_hyperv:1;			// Bit	12
			u32 int_mbec_syscall:1;			// Bit	13
			u32 enlightened_vmcs:1;			// Bit	14
			u32 synced_timeline:1;			// Bit	15
			u32 reserved2:1;				// Bit	16
			u32 direct_loflush_entire:1;	// Bit	17
			u32 no_nonarch_share:1;			// Bit	18
			u32 reserved3:13;				// Bit	19-31
		};
		u32 value;
	}recommendation1;
	u32 retry_attempts_spinlock;
	union
	{
		struct
		{
			u32 implemented_pa_bits:7;		// Bits	0-6
			u32 reserved:25;				// Bits	7-31
		};
		u32 value;
	}recommendation2;
	u32 reserved;
}noir_mshv_cpuid_implementation_recommendations,*noir_mshv_cpuid_implementation_recommendations_p;

typedef struct _noir_mshv_cpuid_implementation_limits
{
	u32 max_virtual_processors;
	u32 max_logical_processors;
	u32 max_phys_int_vector_remapping;
	u32 reserved;
}noir_mshv_cpuid_implementation_limits,*noir_mshv_cpuid_implementation_limits_p;

typedef struct _noir_mshv_cpuid_hardware_features
{
	union
	{
		struct
		{
			u32 apic_overlay_assist_in_use:1;	// Bit	0
			u32 msr_bitmaps_in_use:1;			// Bit	1
			u32 arch_perf_count_in_use:1;		// Bit	2
			u32 slat_in_use:1;					// Bit	3
			u32 dma_remapping_in_use:1;			// Bit	4
			u32 int_remapping_in_use:1;			// Bit	5
			u32 mem_patrol_scrubber_in_use:1;	// Bit	6
			u32 dma_protection_in_use:1;		// Bit	7
			u32 hpet_is_requested:1;			// Bit	8
			u32 synthetic_timer_volatile:1;		// Bit	9
			u32 reserved:22;					// Bits	10-31
		};
		u32 value;
	}limit1;
	u32 reserved[3];
}noir_mshv_cpuid_hardware_features,*noir_mshv_cpuid_hardware_features_p;

typedef struct _noir_mshv_cpuid_cpu_mgmt_features
{
	union
	{
		struct
		{
			u32 start_logical_processor:1;		// Bit	0
			u32 create_root_vcpu:1;				// Bit	1
			u32 performance_counter_sync:1;		// Bit	2
			u32 reserved:28;					// Bits	3-30
			u32 reserved_identity:1;			// Bit	31
		};
		u32 value;
	}feat1;
	union
	{
		struct
		{
			u32 processor_power_mgmt:1;			// Bit	0
			u32 mwait_idle_states:1;			// Bit	1
			u32 logical_processor_idling:1;		// Bit	2
			u32 reserved:29;					// Bits	3-31
		};
		u32 value;
	}feat2;
	union
	{
		struct
		{
			u32 remap_guest_uncached:1;			// Bit	0
			u32 reserved:31;					// Bits	1-31
		};
		u32 value;
	}feat3;
	u32 reserved;
}noir_mshv_cpuid_cpu_mgmt_features,*noir_mshv_cpuid_cpu_mgmt_features_p;

typedef struct _noir_mshv_cpuid_shared_vm_features
{
	union
	{
		struct
		{
			u32 svm_supported:1;		// Bit	0
			u32 reserved0:10;			// Bits	1-10
			u32 max_pasid_count:21;		// Bits	11-31
		};
		u32 value;
	}feature;
	u32 reserved[3];
}noir_mshv_cpuid_shared_vm_features,*noir_mshv_cpuid_shared_vm_features_p;

typedef struct _noir_mshv_cpuid_nested_hv_feature_id
{
	union
	{
		struct
		{
			u32 reserved1:2;				// Bits	0-1
			u32 access_synic_regs:1;		// Bit	2
			u32 reserved2:1;				// Bit	3
			u32 access_intr_ctrl_regs:1;	// Bit	4
			u32 access_hypercall_msrs:1;	// Bit	5
			u32 access_vp_index:1;			// Bit	6
			u32 reserved3:5;				// Bits	7-11
			u32 access_reenlignten_ctrl:1;	// Bit	12
			u32 reserved4:19;				// Bits	13-31
		};
		u32 value;
	}feat1;
	u32 reserved[2];
	union
	{
		struct
		{
			u32 reserved1:4;				// Bits	0-3
			u32 xmm_reg_hypercall:1;		// Bit	4
			u32 reserved2:11;				// Bits	5-14
			u32 fast_hypercall_output:1;	// Bit	15
			u32 reserved3:1;				// Bit	16
			u32 sint_polling_mode:1;		// Bit	17
			u32 reserved4:14;				// Bits	18-31
		};
		u32 value;
	}feat2;
}noir_mshv_cpuid_nested_hv_feature_id,*noir_mshv_cpuid_nested_hv_feature_id_p;

typedef struct _noir_mshv_cpuid_nested_virt_features
{
	union
	{
		struct
		{
			u32 enlightened_vmcs_version_lo:8;	// Bits	0-7
			u32 enlightened_vmcs_version_hi:8;	// Bits	8-15
			u32 reserved1:1;					// Bit	16
			u32 direct_vflush_hypercall:1;		// Bit	17
			u32 flush_guest_pa_hypercalls:1;	// Bit	18
			u32 enlightened_msr_bitmaps:1;		// Bit	19
			u32 combine_ve_pf_exceptions:1;		// Bit	20
			u32 reserved2:11;					// Bits	21-31
		};
		u32 value;
	}feat1;
	u32 reserved[3];
}noir_mshv_cpuid_nested_virt_features,*noir_mshv_cpuid_nested_virt_features_p;