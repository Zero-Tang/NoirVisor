/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the customizable VM engine for AMD-V

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_custom.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <nvstatus.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
#include "svm_def.h"
#include "svm_npt.h"

void nvc_svm_release_vcpu(noir_svm_vcpu_p vcpu)
{
	if(vcpu)
	{
		if(vcpu->vmcb.virt)noir_free_contd_memory(vcpu->vmcb.virt);
		if(vcpu->hv_stack)noir_free_nonpg_memory(vcpu->hv_stack);
		if(vcpu->state)noir_free_nonpg_memory(vcpu->state);
		noir_free_nonpg_memory(vcpu);
	}
}

noir_status nvc_svm_create_vcpu(noir_svm_vcpu_p* virtual_cpu)
{
	noir_svm_vcpu_p vcpu=noir_alloc_nonpg_memory(sizeof(noir_svm_vcpu));
	if(vcpu)
	{
		vcpu->vmcb.virt=noir_alloc_contd_memory(page_size);
		if(vcpu->vmcb.virt)
			vcpu->vmcb.phys=noir_get_physical_address(vcpu->vmcb.virt);
		else
			goto alloc_failure;
		vcpu->state=noir_alloc_nonpg_memory(sizeof(noir_vcpu_state));
		if(vcpu->state==null)goto alloc_failure;
	}
	*virtual_cpu=vcpu;
	return noir_success;
alloc_failure:
	return noir_insufficient_resources;
}

noir_status nvc_svm_create_vm(noir_virtual_machine_p* vm)
{
	return noir_not_implemented;
}