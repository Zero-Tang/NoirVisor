/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

  This file defines cpuid Exit Handler facility.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /svm_core/svm_cpuid.h
*/

#include <nvdef.h>

#define std_leaf_index	0	// CPUID Leaf of Standard	Functions
#define hvm_leaf_index	1	// CPUID Leaf of Hypervisor	Functions
#define ext_leaf_index	2	// CPUID Leaf of Extended	Functions
#define res_leaf_index	3	// CPUID Leaf of Reserved	Functions

typedef void (fastcall *noir_svm_cpuid_exit_handler)
(
 noir_gpr_state_p gpr_state,
 noir_svm_vcpu_p vcpu
);

noir_svm_cpuid_exit_handler** svm_cpuid_handlers=null;