/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file defines structures and constants for SVM Driver of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/svm_hvm.h
*/

#include <nvdef.h>

#define noir_svm_callexit		1

typedef struct _memory_descriptor
{
	void* virt;
	u64 phys;
}memory_descriptor,*memory_descriptor_p;

//Optimize Memory Usage of MSRPM and IOPM.
typedef struct _noir_svm_hvm
{
	memory_descriptor msrpm;
	memory_descriptor iopm;
}noir_svm_hvm,*noir_svm_hvm_p;

typedef struct _noir_svm_vcpu
{
	memory_descriptor vmcb;
	memory_descriptor hsave;
	void* hv_stack;
	noir_svm_hvm_p relative_hvm;
	u8 status;
	struct
	{
		u64 hsave_gpa;
		bool svme;
	}nested_hvm;
}noir_svm_vcpu,*noir_svm_vcpu_p;

u8 nvc_svm_subvert_processor_a(noir_svm_vcpu_p vcpu);
bool nvc_svm_build_exit_handler();
void nvc_svm_teardown_exit_handler();
void nvc_svm_return(ulong_ptr gsp);

#if defined(_svm_drv)
void** host_rsp_list=null;
#endif