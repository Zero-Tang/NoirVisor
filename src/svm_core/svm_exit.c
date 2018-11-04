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

void static fastcall nvc_svm_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	nv_dprintf("Unhandled VM-Exit!\n");
}

//Expected Intercept Code: 0x72
void static fastcall nvc_svm_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	u32 ia=(u32)gpr_state->rax;
	u32 ic=(u32)gpr_state->rcx;
	u32 a,b,c,d;
	noir_cpuid(ia,ic,&a,&b,&c,&d);
	//Now we don't support nested virtualization.
	//Thus, mark the processor not supporting AMD-V.
	if(ia==0x80000001)c&=~amd64_cpuid_svm_bit;
	if(ia==0x8000000A)a=b=d=0;
	*(u32*)&gpr_state->rax=a;
	*(u32*)&gpr_state->rbx=b;
	*(u32*)&gpr_state->rcx=c;
	*(u32*)&gpr_state->rdx=d;
}

//Expected Intercept Code: 0x7C
void static fastcall nvc_svm_msr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	u32 index=(u32)gpr_state->rcx;
	bool op_write=noir_svm_vmread8(vmcb,exit_info1);
	large_integer val={0};
	val.low=(u32)gpr_state->rax;
	val.high=(u32)gpr_state->rdx;
	//
	if(op_write)
	{
		switch(index)
		{
			case amd64_efer:
			{
				//This is for future feature of nested virtualization.
				vcpu->nested_hvm.svme=noir_bt(&val.low,amd64_efer_svme);
				val.value|=amd64_efer_svme_bit;
				//Other bits can be ignored, but SVME should be always protected.
				noir_svm_vmwrite64(vmcb,guest_efer,val.value);
				break;
			}
			case amd64_hsave_pa:
			{
				vcpu->nested_hvm.hsave_gpa=val.value;
				break;
			}
		}
	}
	else
	{
		switch(index)
		{
			case amd64_efer:
			{
				val.value=noir_svm_vmread64(vmcb,guest_efer);
				val.value&=(vcpu->nested_hvm.svme<<amd64_efer_svme);
				break;
			}
			case amd64_hsave_pa:
			{
				val.value=vcpu->nested_hvm.hsave_gpa=val.value;
				break;
			}
#if defined(_amd64)
			case amd64_lstar:
			{
				break;
			}
#else
			case amd64_sysenter_eip:
			{
				break;
			}
#endif
		}
	}
	if(!op_write)
	{
		*(u32*)&gpr_state->rax=val.low;
		*(u32*)&gpr_state->rdx=val.high;
	}
}

//Expected Intercept Code: 0x80
void static fastcall nvc_svm_vmrun_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	nv_dprintf("VM-Exit occured by vmrun instruction!\n");
	nv_dprintf("Nested Virtualization of SVM is not supported!\n");
}

//Expected Intercept Code: 0x81
void static fastcall nvc_svm_vmmcall_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	;
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
		nvc_svm_default_handler(gpr_state,vcpu);
	else
		svm_exit_handlers[code_group][code_num](gpr_state,vcpu);
	noir_svm_vmwrite(vmcb_va,guest_rax,gpr_state->rax);
	gpr_state->rax=(ulong_ptr)vcpu->vmcb.phys;
}

bool nvc_build_exit_handler()
{
	//Allocate the array of Exit-Handler Group
	svm_exit_handlers=noir_alloc_nonpg_memory(sizeof(void*)*4);
	if(svm_exit_handlers)
	{
		//Allocate arrays of Exit-Handlers
		svm_exit_handlers[0]=noir_alloc_nonpg_memory(noir_svm_maximum_code1*sizeof(void*));
		svm_exit_handlers[1]=noir_alloc_nonpg_memory(noir_svm_maximum_code2*sizeof(void*));
		if(svm_exit_handlers[0] && svm_exit_handlers[1])
		{
			//Initialize it with default-handler.
			noir_stosp(svm_exit_handlers[0],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code1);
			noir_stosp(svm_exit_handlers[1],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code2);
		}
		else
		{
			//Allocation failed. Perform cleanup.
			if(svm_exit_handlers[0])noir_free_nonpg_memory(svm_exit_handlers[0]);
			if(svm_exit_handlers[1])noir_free_nonpg_memory(svm_exit_handlers[1]);
			noir_free_nonpg_memory(svm_exit_handlers);
			return false;
		}
		//Zero the group if it is unused.
		svm_exit_handlers[2]=svm_exit_handlers[3]=null;
		//Setup Exit-Handlers
		svm_exit_handlers[0][intercepted_cpuid]=nvc_svm_cpuid_handler;
		svm_exit_handlers[0][intercepted_msr]=nvc_svm_msr_handler;
		svm_exit_handlers[0][intercepted_vmrun]=nvc_svm_vmrun_handler;
		svm_exit_handlers[0][intercepted_vmmcall]=nvc_svm_vmmcall_handler;
		return true;
	}
	return false;
}

void nvc_teardown_exit_handler()
{
	if(svm_exit_handlers)
	{
		if(svm_exit_handlers[0])noir_free_nonpg_memory(svm_exit_handlers[0]);
		if(svm_exit_handlers[1])noir_free_nonpg_memory(svm_exit_handlers[1]);
		if(svm_exit_handlers[2])noir_free_nonpg_memory(svm_exit_handlers[2]);
		if(svm_exit_handlers[3])noir_free_nonpg_memory(svm_exit_handlers[3]);
		noir_free_nonpg_memory(svm_exit_handlers);
		svm_exit_handlers=null;
	}
}