/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the cpuid Exit Handler of Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_cpuid.c
*/

#include <nvdef.h>
#include <nvstatus.h>
#include <nvbdk.h>
#include <vt_intrin.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include "vt_vmcs.h"
#include "vt_def.h"
#include "vt_exit.h"
#include "vt_cpuid.h"

/*
  The CPUID leaf can be divided into following:
  Bits 30-31: Leaf Class.
  Bits 00-29: Leaf Index.

  Class 0: Standard Leaf Function		Range: 0x00000000-0x3FFFFFFF
  Class 1: Hypervisor Leaf Function		Range: 0x40000000-0x7FFFFFFF
  Class 2: Extended Leaf Function		Range: 0x80000000-0xBFFFFFFF
  Class 3: Reserved Leaf Function		Range: 0xC0000000-0xFFFFFFFF
*/

/*
  Build cache for CPUID instruction per virtual processor.

  Generic Rules:
  1. Use cpuid instruction with ecx=0 to initialize generic data.
  2. If we have special treatings, make specific initializations.
*/
void nvc_vt_build_cpuid_cache_per_vcpu(noir_vt_vcpu_p vcpu)
{
	if(vcpu->enabled_feature & noir_vt_cpuid_caching)
	{
		noir_vt_cpuid_info_p info;
		u32 i;
		// Generic Initialization
		for(i=0;i<vcpu->cpuid_cache.max_leaf[std_leaf_index];i++)
		{
			info=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.std_leaf[i];
			noir_cpuid(i,0,&info->eax,&info->ebx,&info->ecx,&info->edx);
		}
		for(i=0;i<vcpu->cpuid_cache.max_leaf[ext_leaf_index];i++)
		{
			info=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.ext_leaf[i];
			noir_cpuid(i+0x80000000,0,&info->eax,&info->ebx,&info->ecx,&info->edx);
		}
		// Function leaf 0x00000001 - Processor and Processor Feature Identifiers
		// Indicate Hypervisor Presence and VMX Supportability here
		info=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.hvm_leaf[std_proc_feature];
		noir_btr(&info->ecx,ia32_cpuid_hv_presence);
		noir_bts(&info->ecx,ia32_cpuid_vmx);
		// Function leaf 0x00000007 - Structured Extended Feature Flags Enumeration Leaf
		info=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.hvm_leaf[std_struct_extid];
		for(i=1;i<=info->eax;i++)
			noir_cpuid(std_struct_extid,i,&info[i].eax,&info[i].ebx,&info[i].ecx,&info[i].edx);
		// Function leaf 0x40000000 - Maximum Hypervisor Function Number and Vendor String
		info=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.hvm_leaf[hvm_max_num_vstr];
		info->eax=0x40000001;		// Indicate the highest function leaf.
		// Vendor String="NoirVisor ZT"
		info->ebx='rioN';
		info->ecx='osiV';
		info->edx='TZ r';
		// Function leaf 0x40000001 - Hypervisor Vendor-Neutral Interface Identification
		info=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.hvm_leaf[hvm_interface_id];
		info->eax='0#vH';	// Hypervisor Interface Signature - Indicate Non-Conformance to Microsoft TLFS
	}
}

// Default Function Leaf
// You should make sure that this leaf has no sub-leaves.
void static fastcall nvc_vt_default_cpuid_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	// Classify the leaf.
	u32 leaf=(u32)gpr_state->rax;
	u32 subleaf=(u32)gpr_state->rcx;
	u32 leaf_class=noir_cpuid_class(leaf);
	u32 leaf_index=noir_cpuid_index(leaf);
	// Locate the cache
	noir_vt_cpuid_info_p cache=null;
	switch(leaf_class)
	{
		case std_leaf_index:
		{
			cache=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.std_leaf[leaf_index];
			break;
		}
		case hvm_leaf_index:
		{
			cache=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.hvm_leaf[leaf_index];
			break;
		}
		case ext_leaf_index:
		{
			cache=(noir_vt_cpuid_info_p)vcpu->cpuid_cache.ext_leaf[leaf_index];
			break;
		}
		default:
		{
			// In principle, this branch is impossible to reach.
			cache=null;
			noir_int3();
			break;
		}
	}
	// Copy from cache
	if(subleaf)
	{
		;
	}
	else
	{
		*(u32*)gpr_state->rax=cache->eax;
		*(u32*)gpr_state->rax=cache->eax;
		*(u32*)gpr_state->rax=cache->eax;
		*(u32*)gpr_state->rax=cache->eax;
	}
}

void fastcall nvc_vt_reserved_cpuid_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu)
{
	*(u32*)gpr_state->rax=0;
	*(u32*)gpr_state->rbx=0;
	*(u32*)gpr_state->rcx=0;
	*(u32*)gpr_state->rdx=0;
}

bool nvc_vt_build_cpuid_handler(u32 std_count,u32 hvm_count,u32 ext_count,u32 res_count)
{
	vt_cpuid_handlers=noir_alloc_nonpg_memory(sizeof(void*)*4);
	if(vt_cpuid_handlers)
	{
		vt_cpuid_handlers[std_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*std_count);
		vt_cpuid_handlers[hvm_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*hvm_count);
		vt_cpuid_handlers[ext_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*ext_count);
		if(vt_cpuid_handlers[std_leaf_index] && vt_cpuid_handlers[hvm_leaf_index] && vt_cpuid_handlers[ext_leaf_index])
		{
			// Initialize handlers with default handlers.
			noir_stosp(vt_cpuid_handlers[std_leaf_index],(ulong_ptr)nvc_vt_default_cpuid_handler,std_count);
			noir_stosp(vt_cpuid_handlers[hvm_leaf_index],(ulong_ptr)nvc_vt_default_cpuid_handler,hvm_count);
			noir_stosp(vt_cpuid_handlers[ext_leaf_index],(ulong_ptr)nvc_vt_default_cpuid_handler,ext_count);
			// Default handlers are set. Setup the customized handlers here.
			return true;
		}
	}
	return false;
}

void nvc_vt_teardown_cpuid_handler()
{
	if(vt_cpuid_handlers)
	{
		if(vt_cpuid_handlers[std_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[std_leaf_index]);
		if(vt_cpuid_handlers[hvm_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[hvm_leaf_index]);
		if(vt_cpuid_handlers[ext_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[ext_leaf_index]);
		if(vt_cpuid_handlers[res_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[res_leaf_index]);
		noir_free_nonpg_memory(vt_cpuid_handlers);
		vt_cpuid_handlers=null;
	}
}