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

void static fastcall nvc_svm_default_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	;
}

bool nvc_svm_build_cpuid_handler(u32 std_count,u32 ext_count)
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
			return true;
		}
	}
	return false;
}