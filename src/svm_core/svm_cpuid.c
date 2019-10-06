/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

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
#include <intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
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
		case ext_leaf_index:
		{
			cache=(noir_svm_cpuid_default_p)vcpu->cpuid_cache.ext_leaf[leaf_index];
			break;
		}
		default:
		{
			cache=null;
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

// Function Leaf: 0x00000000 - Maximum Number of Leaves and Vendor String
void static fastcall nvc_svm_cpuid_std_vendor_string(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_cpuid_max_num_vstr_p cache=(noir_svm_cpuid_max_num_vstr_p)vcpu->cpuid_cache.std_leaf[std_max_num_vstr];
	// EAX - Maximum Number of CPUID
	*(u32*)&gpr_state->rax=cache->maximum;
	// EBX-EDX-ECX -> "AuthenticAMD" by default
	*(u32*)&gpr_state->rbx=*(u32*)&cache->vendor_string[0];
	*(u32*)&gpr_state->rcx=*(u32*)&cache->vendor_string[8];
	*(u32*)&gpr_state->rdx=*(u32*)&cache->vendor_string[4];
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

// Function Leaf: 0x80000000 - Maximum Number of Leaves and Vendor String
void static fastcall nvc_svm_cpuid_ext_vendor_string(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_cpuid_max_num_vstr_p cache=(noir_svm_cpuid_max_num_vstr_p)vcpu->cpuid_cache.ext_leaf[ext_max_num_vstr];
	// EAX - Maximum Number of CPUID
	*(u32*)&gpr_state->rax=cache->maximum;
	// EBX-EDX-ECX -> "AuthenticAMD" by default
	*(u32*)&gpr_state->rbx=*(u32*)&cache->vendor_string[0];
	*(u32*)&gpr_state->rcx=*(u32*)&cache->vendor_string[8];
	*(u32*)&gpr_state->rdx=*(u32*)&cache->vendor_string[4];
}

// Function Leaf: 0x80000002 - Processor Name String (Part I)
void static fastcall nvc_svm_cpuid_ext_brand_string_p1(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	char* brand=(char*)vcpu->cpuid_cache.ext_leaf[ext_brand_str_p1];
	// Sorted by EAX-EBX-ECX-EDX
	*(u32*)&gpr_state->rax=*(u32*)brand[0x0];
	*(u32*)&gpr_state->rbx=*(u32*)brand[0x4];
	*(u32*)&gpr_state->rcx=*(u32*)brand[0x8];
	*(u32*)&gpr_state->rdx=*(u32*)brand[0xC];
}

// Function Leaf: 0x80000003 - Processor Name String (Part II)
void static fastcall nvc_svm_cpuid_ext_brand_string_p2(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	char* brand=(char*)vcpu->cpuid_cache.ext_leaf[ext_brand_str_p2];
	// Sorted by EAX-EBX-ECX-EDX
	*(u32*)&gpr_state->rax=*(u32*)brand[0x0];
	*(u32*)&gpr_state->rbx=*(u32*)brand[0x4];
	*(u32*)&gpr_state->rcx=*(u32*)brand[0x8];
	*(u32*)&gpr_state->rdx=*(u32*)brand[0xC];
}

// Function Leaf: 0x80000004 - Processor Name String (Part III)
void static fastcall nvc_svm_cpuid_ext_brand_string_p3(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	char* brand=(char*)vcpu->cpuid_cache.ext_leaf[ext_brand_str_p3];
	// Sorted by EAX-EBX-ECX-EDX
	*(u32*)&gpr_state->rax=*(u32*)brand[0x0];
	*(u32*)&gpr_state->rbx=*(u32*)brand[0x4];
	*(u32*)&gpr_state->rcx=*(u32*)brand[0x8];
	*(u32*)&gpr_state->rdx=*(u32*)brand[0xC];
}

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

bool nvc_svm_build_cpuid_handler(u32 std_count,u32 hvm_count,u32 ext_count,u32 res_count)
{
	svm_cpuid_handlers=noir_alloc_nonpg_memory(sizeof(void*)*4);
	if(svm_cpuid_handlers)
	{
		svm_cpuid_handlers[std_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*std_count);
		svm_cpuid_handlers[ext_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*ext_count);
		if(svm_cpuid_handlers[std_leaf_index] && svm_cpuid_handlers[ext_leaf_index])
		{
			// Initialize CPUID handlers with default handlers.
			// Using stos instruction could accelerate the initialization.
			noir_stosp(svm_cpuid_handlers[std_leaf_index],(ulong_ptr)nvc_svm_default_cpuid_handler,std_count);
			noir_stosp(svm_cpuid_handlers[ext_leaf_index],(ulong_ptr)nvc_svm_default_cpuid_handler,ext_count);
			// Default Handlers are set. Setup the customized handlers here.
			svm_cpuid_handlers[ext_leaf_index][ext_svm_features]=nvc_svm_cpuid_ext_svm_feature_id;
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