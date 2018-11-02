/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file defines intrinsics for SVM instructions.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_intrin.h
*/

#include "nvdef.h"

#if defined(_msvc)
#define noir_svm_vmrun		__svm_vmrun
#define noir_svm_vmload		__svm_vmload
#define noir_svm_vmsave		__svm_vmsave
#define noir_svm_stgi		__svm_stgi
#define noir_svm_clgi		__svm_clgi
#define noir_svm_invlpga	__svm_invlpga
#endif

void stdcall noir_svm_vmmcall(u32 index,ulong_ptr context);