/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the cpuid Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_cpuid.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
#include "svm_exit.h"
#include "svm_cpuid.h"

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
void nvc_svm_build_cpuid_cache_per_vcpu(noir_svm_vcpu_p vcpu)
{
	noir_svm_cpuid_default_p info;
	u32 i;
	// Generic Initialization
	for(i=0;i<vcpu->cpuid_cache.max_leaf[std_leaf_index];i++)
	{
		info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.std_leaf[i];
		noir_cpuid(i,0,&info->eax,&info->ebx,&info->ecx,&info->edx);
	}
	for(i=0;i<vcpu->cpuid_cache.max_leaf[ext_leaf_index];i++)
	{
		info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.ext_leaf[i];
		noir_cpuid(i+0x80000000,0,&info->eax,&info->ebx,&info->ecx,&info->edx);
	}
	// Specific Initialization
	// Function Leaf 0x1 - Processor and Processor Feature Identifiers
	// We indicate hypervisor presense here.
	info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.std_leaf[std_proc_feature];
	noir_bts(&info->ecx,amd64_cpuid_hv_presence);
	// Function leaf 0x7 - Structured Extended Feature Identifiers
	// There is only one subleaf in this function leaf.

	// Function leaf 0xD - Processor Extended State Enumeration
	// There are multiple subfunctions in this leaf.
	info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.std_leaf[std_pestate_enum];
	noir_cpuid(std_pestate_enum,1,&info[1].eax,&info[1].ebx,&info[1].ecx,&info[1].edx);		// Sub-leaf 1
	noir_cpuid(std_pestate_enum,2,&info[2].eax,&info[2].ebx,&info[2].ecx,&info[2].edx);		// Sub-leaf 2
	noir_cpuid(std_pestate_enum,62,&info[3].eax,&info[3].ebx,&info[3].ecx,&info[3].edx);	// Sub-leaf 62
	// Function leaf 0x40000000 - Maximum Hypervisor Function Number and Vendor String
	info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.hvm_leaf[hvm_max_num_vstr];
	info->eax=0x40000001;		// Indicate the highest function leaf.
	// Vendor String="NoirVisor ZT"
	info->ebx='rioN';
	info->ecx='osiV';
	info->edx='TZ r';
	// Function leaf 0x40000001 - Hypervisor Vendor-Neutral Interface Identification
	info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.hvm_leaf[hvm_interface_id];
	info->eax='0#vH';	// Hypervisor Interface Signature - Indicate Non-Conformance to Microsoft TLFS
	info->ebx=info->ecx=info->edx=0;		// They are reserved values
	// Function leaf 0x80000001 - Extended Processor and Processor Feature Identifiers
	// By now, we might have to indicate no support to nested virtualization.
	info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.ext_leaf[ext_proc_feature];
	noir_btr(&info->ecx,amd64_cpuid_svm);
	// Function leaf 0x8000000A - SVM Features
	// Erase some features that we don't have algorithms to support.
	info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.ext_leaf[ext_svm_features];
	info->ebx--;		// Decrement available ASID by 1.
	noir_btr(&info->edx,amd64_cpuid_npt);	// NoirVisor does not have an algorithm to it.
	// Function leaf 0x8000001D - Cache Topology Information
	// This leaf includes variable count of subfunctions.
	info=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.ext_leaf[ext_cache_topinf];
	for(i=1;(info[i-1].eax & 0x1f)!=0;i++)
	{
		if(i==8)
		{
			nv_panicf("Overflow in Cache Topology CPUID Leaf!\n");
			// If this breakpoint is triggered, we should increase length of allocation for this subleaf.
			noir_int3();
			break;
		}
		noir_cpuid(ext_cache_topinf+0x80000000,i,&info[i].eax,&info[i].ebx,&info[i].ecx,&info[i].edx);
	}
}

// Reserved Function Leaf.
// This would clear the CPUID data to zero.
void fastcall nvc_svm_reserved_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	*(u32*)&gpr_state->rax=0;
	*(u32*)&gpr_state->rbx=0;
	*(u32*)&gpr_state->rcx=0;
	*(u32*)&gpr_state->rdx=0;
}

// Default Function Leaf.
// You should make sure that this leaf has no sub-leaves.
void static fastcall nvc_svm_default_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	// Classify the leaf.
	u32 leaf=(u32)gpr_state->rax;
	u32 leaf_class=noir_cpuid_class(leaf);
	u32 leaf_index=noir_cpuid_index(leaf);
	// Locate the cache
	noir_svm_cpuid_default_p cache=null;
	switch(leaf_class)
	{
		case std_leaf_index:
		{
			cache=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.std_leaf[leaf_index];
			break;
		}
		case hvm_leaf_index:
		{
			cache=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.hvm_leaf[leaf_index];
			break;
		}
		case ext_leaf_index:
		{
			cache=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.ext_leaf[leaf_index];
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
	*(u32*)&gpr_state->rax=cache->eax;
	*(u32*)&gpr_state->rbx=cache->ebx;
	*(u32*)&gpr_state->rcx=cache->ecx;
	*(u32*)&gpr_state->rdx=cache->edx;
}

/*
  Standard Leaf Functions:

  In this section, handler functions should be regarding
  the standard function leaves. The range starts from 0.
*/

// Function Leaf: 0x00000007 - Structured Extended Feature Identifiers
// This leaf has multiple subleaves, so we have to make special treatments during caching.
void static fastcall nvc_svm_cpuid_std_struct_extid(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_cpuid_default_p cache=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.std_leaf[std_struct_extid];
	*(u32*)&gpr_state->rax=cache->eax;
	*(u32*)&gpr_state->rbx=cache->ebx;
	*(u32*)&gpr_state->rcx=cache->ecx;
	*(u32*)&gpr_state->rdx=cache->edx;
}

// Function Leaf: 0x0000000D - Processor Extended State Enumeration
// This leaf has multiple subleaves, so we have to make special treatments during caching.
void static fastcall nvc_svm_cpuid_std_pestate_enum(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_cpuid_default_p cache=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.std_leaf[std_pestate_enum];
	u32 subleaf=(u32)gpr_state->rcx;
	// We need to adjust index for compacted cache array.
	if(subleaf>2)
	{
		if(subleaf==62)
			subleaf=3;		// SubLeaf=62 is cached to index=3
		else
			subleaf=4;		// index=4 has zeroed data.
	}
	*(u32*)&gpr_state->rax=cache[subleaf].eax;
	*(u32*)&gpr_state->rbx=cache[subleaf].ebx;
	*(u32*)&gpr_state->rcx=cache[subleaf].ecx;
	*(u32*)&gpr_state->rdx=cache[subleaf].edx;
}

/*
  Hypervisor Leaf Functions:

  In this section, handler functions should be regarding the
  hypervisor specific functions. At this time, we are supposed
  to conform the Microsoft Hypervisor TLFS. However, NoirVisor,
  by now, is not in conformation.
*/

/*
  Extended Leaf Functions:

  In this section, handler functions should be regarding the
  extended function leaves. The range starts from 0x80000000.
*/

// Function Leaf: 0x8000000A - SVM Revision and Feature Id
void static fastcall nvc_svm_cpuid_ext_svm_feature_id(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	// Locate cached information.
	noir_svm_cpuid_svm_feature_id_p cache=(noir_svm_cpuid_svm_feature_id_p)vcpu->cpuid_cache.ext_leaf[ext_svm_features];
	// EAX - SVM Revision Number
	*(u32*)&gpr_state->rax=cache->rev_number;
	// EBX - SVM Number of Available ASIDs
	*(u32*)&gpr_state->rbx=cache->avail_asid;
	// ECX - Reserved
	*(u32*)&gpr_state->rcx=0;
	// EDX - SVM Feature Identification
	*(u32*)&gpr_state->rdx=cache->feature_id;
}

// Function Leaf: 0x8000001D - Cache Topology Information
// This leaf has multiple subleaves, so we have to make special treatments during caching.
void static fastcall nvc_svm_cpuid_ext_cache_topinf(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_cpuid_default_p cache=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.ext_leaf[ext_cache_topinf];
	u32 subleaf=(u32)gpr_state->rcx;
	if(subleaf>0 && subleaf<8)
	{
		if(cache[subleaf-1].eax & 0x1f)
		{
			// This subleaf is valid.
			*(u32*)&gpr_state->rax=cache[subleaf].eax;
			*(u32*)&gpr_state->rbx=cache[subleaf].ebx;
			*(u32*)&gpr_state->rcx=cache[subleaf].ecx;
			*(u32*)&gpr_state->rdx=cache[subleaf].edx;
		}
		else
		{
			// This subleaf is invalid. As defined by AMD64, raise #UD.
			noir_svm_inject_event(vcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
		}
	}
	else
	{
		if(subleaf>8)
			noir_svm_inject_event(vcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
		else
		{
			*(u32*)&gpr_state->rax=cache->eax;
			*(u32*)&gpr_state->rbx=cache->ebx;
			*(u32*)&gpr_state->rcx=cache->ecx;
			*(u32*)&gpr_state->rdx=cache->edx;
		}
	}
}

bool nvc_svm_build_cpuid_handler(u32 std_count,u32 hvm_count,u32 ext_count,u32 res_count)
{
	svm_cpuid_handlers=noir_alloc_nonpg_memory(sizeof(void*)*4);
	if(svm_cpuid_handlers)
	{
		svm_cpuid_handlers[std_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*std_count);
		svm_cpuid_handlers[hvm_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*hvm_count);
		svm_cpuid_handlers[ext_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*ext_count);
		if(svm_cpuid_handlers[std_leaf_index] && svm_cpuid_handlers[ext_leaf_index])
		{
			// Initialize CPUID handlers with default handlers.
			// Using stos instruction could accelerate the initialization.
			noir_stosp(svm_cpuid_handlers[std_leaf_index],(ulong_ptr)nvc_svm_default_cpuid_handler,std_count);
			noir_stosp(svm_cpuid_handlers[hvm_leaf_index],(ulong_ptr)nvc_svm_default_cpuid_handler,hvm_count);
			noir_stosp(svm_cpuid_handlers[ext_leaf_index],(ulong_ptr)nvc_svm_default_cpuid_handler,ext_count);
			// Default Handlers are set. Setup the customized handlers here.
			svm_cpuid_handlers[std_leaf_index][std_struct_extid]=nvc_svm_cpuid_std_struct_extid;
			svm_cpuid_handlers[std_leaf_index][std_pestate_enum]=nvc_svm_cpuid_std_pestate_enum;
			svm_cpuid_handlers[ext_leaf_index][ext_svm_features]=nvc_svm_cpuid_ext_svm_feature_id;
			svm_cpuid_handlers[ext_leaf_index][ext_cache_topinf]=nvc_svm_cpuid_ext_cache_topinf;
			return true;
		}
	}
	return false;
}

void nvc_svm_teardown_cpuid_handler()
{
	if(svm_cpuid_handlers)
	{
		if(svm_cpuid_handlers[std_leaf_index])
			noir_free_nonpg_memory(svm_cpuid_handlers[std_leaf_index]);
		if(svm_cpuid_handlers[hvm_leaf_index])
			noir_free_nonpg_memory(svm_cpuid_handlers[hvm_leaf_index]);
		if(svm_cpuid_handlers[ext_leaf_index])
			noir_free_nonpg_memory(svm_cpuid_handlers[ext_leaf_index]);
		if(svm_cpuid_handlers[res_leaf_index])
			noir_free_nonpg_memory(svm_cpuid_handlers[res_leaf_index]);
		noir_free_nonpg_memory(svm_cpuid_handlers);
		svm_cpuid_handlers=null;
	}
}