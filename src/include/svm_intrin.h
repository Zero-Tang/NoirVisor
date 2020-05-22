/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines intrinsics for SVM instructions.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_intrin.h
*/

#include "nvdef.h"

u32 inline svm_msrpm_bit(u8 bitmap,u32 index,u8 operation)
{
	if(bitmap==1)
		return index*2+operation;
	else if(bitmap==2)
		return (index-0xC0000000)*2+operation;
	else if(bitmap==3)
		return (index-0xC0010000)*2+operation;
	return 0;
}

#if defined(_msvc)
#define noir_svm_vmrun		__svm_vmrun
#define noir_svm_vmload		__svm_vmload
#define noir_svm_vmsave		__svm_vmsave
#define noir_svm_stgi		__svm_stgi
#define noir_svm_clgi		__svm_clgi
#define noir_svm_invlpga	__svm_invlpga
#elif defined(_llvm)
#define noir_svm_vmrun(v)		__asm__ __volatile__("vmrun %%rax" : : "a"(v))
#define noir_svm_vmload(v)		__asm__ __volatile__("vmload %%rax" : : "a"(v))
#define noir_svm_vmsave(v)		__asm__ __volatile__("vmsave %%rax" : : "a"(v))
#define noir_svm_stgi()			__asm__ __volatile__("stgi")
#define noir_svm_clgi()			__asm__ __volatile__("clgi")
#define noir_svm_invlpga(a,i)	__asm__	__volatile__("invlpga %%rax,%%rcx" : : "a"(a),"c"(i))
#endif

void fastcall noir_svm_vmmcall(u32 index,ulong_ptr context);