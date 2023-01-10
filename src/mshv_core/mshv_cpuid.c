/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the cpuid Handler of MSHV Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /mshv_core/mshv_cpuid.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include "mshv_cpuid.h"

// Hypervisor CPUID Leaf Range and Vendor ID
void static fastcall nvc_mshv_cpuid_fn40000000_handler(noir_cpuid_general_info_p param)
{
	noir_mshv_cpuid_leaf_range_p info=(noir_mshv_cpuid_leaf_range_p)param;
	info->maximum=hvm_cpuid_implementation_limits;		// Minimal Hv#1 Interface Requirement
	noir_movsb(info->vendor_id,"NoirVisor ZT",12);		// The Vendor String is "NoirVisor ZT"
}

// Hypervisor Vendor-Neutral Interface Identification
void static fastcall nvc_mshv_cpuid_fn40000001_handler(noir_cpuid_general_info_p param)
{
	noir_mshv_cpuid_vendor_neutral_inferface_id_p info=(noir_mshv_cpuid_vendor_neutral_inferface_id_p)param;
	noir_movsb((char*)&info->interface_signature,"Hv#1",4);	// The Hv#1 signature.
	noir_stosd(info->reserved,0,3);		// Clear out the reserved area.
}

// Hypervisor System Identity
void static fastcall nvc_mshv_cpuid_fn40000002_handler(noir_cpuid_general_info_p param)
{
	noir_mshv_cpuid_hypervisor_system_id_p info=(noir_mshv_cpuid_hypervisor_system_id_p)param;
	// NoirVisor doesn't have a versioning strategy right now.
	// Hence, clear out the fields, since it is not required to fill them out.
	noir_stosd((u32*)info,0,4);
}

// Hypervisor Feature Identification
void static fastcall nvc_mshv_cpuid_fn40000003_handler(noir_cpuid_general_info_p param)
{
	noir_mshv_cpuid_hypervisor_feature_id_p info=(noir_mshv_cpuid_hypervisor_feature_id_p)param;
	noir_stosd((u32*)info,0,4);		// Initialization
	// Requirements of Minimal Hv#1 Interface
	info->feat1.access_hypercall_msrs=true;
	info->feat1.access_vp_index=true;
	// Support of Non-Privileged Instruction Execution Prevention (NPIEP)
	info->feat3.npiep=true;
}

// Enlightenment Implementation Recommendations
void static fastcall nvc_mshv_cpuid_fn40000004_handler(noir_cpuid_general_info_p param)
{
	noir_mshv_cpuid_implementation_recommendations_p info=(noir_mshv_cpuid_implementation_recommendations_p)param;
	noir_stosd((u32*)info,0,4);		// Initialization
#if defined(_hv_type1)
	// For Type-I hypervisor on AMD-V, it is efficient to virtualize APIC via Microsoft Synthetic MSRs.
	if(hvm_p->selected_core==use_svm_core)info->recommendation1.msr_apic_access=true;
#endif
}

// Hypervisor Implementation Limits
void static fastcall nvc_mshv_cpuid_fn40000005_handler(noir_cpuid_general_info_p param)
{
	noir_mshv_cpuid_implementation_limits_p info=(noir_mshv_cpuid_implementation_limits_p)param;
	// We may limit the guest nothing. Hence, clear it out.
	noir_stosd((u32*)info,0,4);
}

u32 fastcall nvc_mshv_build_cpuid_handlers()
{
	const u32 lim=hvm_cpuid_implementation_limits-hvm_cpuid_base+1;
	hvm_cpuid_handlers=noir_alloc_nonpg_memory(sizeof(void*)*lim);
	if(hvm_cpuid_handlers)
	{
		hvm_cpuid_handlers[0]=nvc_mshv_cpuid_fn40000000_handler;
		hvm_cpuid_handlers[1]=nvc_mshv_cpuid_fn40000001_handler;
		hvm_cpuid_handlers[2]=nvc_mshv_cpuid_fn40000002_handler;
		hvm_cpuid_handlers[3]=nvc_mshv_cpuid_fn40000003_handler;
		hvm_cpuid_handlers[4]=nvc_mshv_cpuid_fn40000004_handler;
		hvm_cpuid_handlers[5]=nvc_mshv_cpuid_fn40000005_handler;
		return lim;
	}
	return 0;
}

void fastcall nvc_mshv_teardown_cpuid_handlers()
{
	if(hvm_cpuid_handlers)
	{
		noir_free_nonpg_memory(hvm_cpuid_handlers);
		hvm_cpuid_handlers=null;
	}
}