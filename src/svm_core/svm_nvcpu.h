/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This header file defines nested virtualization infracture of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_nvcpu.h
*/

#include <nvdef.h>

#define noir_svm_maximum_clean_state_bits		13

typedef void (fastcall *noir_svm_clean_vmcb_routine)
(
 void* vmcb_c,
 void* vmcb_t
);

void static fastcall nvc_svm_clean_vmcb_interceptions(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_iomsrpm(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_asid(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_tpr(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_np(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_crx(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_drx(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_dt(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_seg(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_cr2(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_lbr(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_avic(void* vmcb_c,void* vmcb_t);
void static fastcall nvc_svm_clean_vmcb_cet(void* vmcb_c,void* vmcb_t);

extern noir_svm_cpuid_exit_handler nvcp_svm_cpuid_handler;

noir_hvdata noir_svm_clean_vmcb_routine svm_clean_vmcb[noir_svm_maximum_clean_state_bits]=
{
	nvc_svm_clean_vmcb_interceptions,	// Bit 0
	nvc_svm_clean_vmcb_iomsrpm,			// Bit 1
	nvc_svm_clean_vmcb_asid,			// Bit 2
	nvc_svm_clean_vmcb_tpr,				// Bit 3
	nvc_svm_clean_vmcb_np,				// Bit 4
	nvc_svm_clean_vmcb_crx,				// Bit 5
	nvc_svm_clean_vmcb_drx,				// Bit 6
	nvc_svm_clean_vmcb_dt,				// Bit 7
	nvc_svm_clean_vmcb_seg,				// Bit 8
	nvc_svm_clean_vmcb_cr2,				// Bit 9
	nvc_svm_clean_vmcb_lbr,				// Bit 10
	nvc_svm_clean_vmcb_avic,			// Bit 11
	nvc_svm_clean_vmcb_cet,				// Bit 12
};