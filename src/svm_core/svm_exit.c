/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the basic Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_main.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
#include "svm_exit.h"
#include "svm_def.h"

void static fastcall nvc_svm_default_handler(noir_gpr_state_p gpr_state,void* vmcb)
{
	nv_dprintf("Unhandled VM-Exit!\n");
}

void nvc_svm_exit_handler(noir_gpr_state_p gpr_state,u32 processor_id)
{
	noir_svm_vcpu_p vcpu=&hvm->virtual_cpu[processor_id];
	void* vmcb_va=vcpu->vmcb.virt;
	i32 intercept_code=noir_svm_vmread32(vmcb_va,exit_code);
	u8 code_group=(u8)((intercept_code&0xC00)>>10);
	u16 code_num=(u16)(intercept_code&0x3FF);
	gpr_state->rax=noir_svm_vmread(vmcb_va,guest_rax);
	if(intercept_code==-1)
		nvc_svm_default_handler(gpr_state,vmcb_va);
	else
		svm_exit_handlers[code_group][code_num](gpr_state,vmcb_va);
	noir_svm_vmwrite(vmcb_va,guest_rax,gpr_state->rax);
	gpr_state->rax=(ulong_ptr)vcpu->vmcb.phys;
}

bool nvc_build_exit_handler()
{
	svm_exit_handlers[0]=noir_alloc_nonpg_memory(noir_svm_maximum_code1*sizeof(void*));
	svm_exit_handlers[1]=noir_alloc_nonpg_memory(noir_svm_maximum_code2*sizeof(void*));
	if(svm_exit_handlers[0] && svm_exit_handlers[1])
	{
		noir_stosp(svm_exit_handlers[0],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code1);
		noir_stosp(svm_exit_handlers[1],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code2);
	}
	else
	{
		if(svm_exit_handlers[0])noir_free_nonpg_memory(svm_exit_handlers[0]);
		if(svm_exit_handlers[1])noir_free_nonpg_memory(svm_exit_handlers[1]);
		svm_exit_handlers[0]=svm_exit_handlers[1]=null;
		return false;
	}
	svm_exit_handlers[2]=svm_exit_handlers[3]=null;
	return true;
}