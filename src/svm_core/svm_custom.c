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

void nvc_svmc_release_vcpu(noir_svm_custom_vcpu_p vcpu)
{
	if(vcpu)
	{
		if(vcpu->vmcb.virt)noir_free_contd_memory(vcpu->vmcb.virt);
		noir_free_nonpg_memory(vcpu);
	}
}

noir_status nvc_svmc_create_vcpu(noir_svm_custom_vcpu_p* virtual_cpu,noir_svm_custom_vm_p virtual_machine)
{
	noir_svm_custom_vcpu_p vcpu=noir_alloc_nonpg_memory(sizeof(noir_svm_custom_vcpu));
	if(vcpu)
	{
		vcpu->vmcb.virt=noir_alloc_contd_memory(page_size);
		if(vcpu->vmcb.virt)
			vcpu->vmcb.phys=noir_get_physical_address(vcpu->vmcb.virt);
		else
			goto alloc_failure;
		// Insert the vCPU into the VM.
		if(virtual_machine->vcpu.head)
			virtual_machine->vcpu.head=virtual_machine->vcpu.tail=vcpu;
		else
		{
			virtual_machine->vcpu.tail->next=vcpu;
			virtual_machine->vcpu.tail=vcpu;
		}
		// Mark the owner VM of vCPU.
		vcpu->vm=virtual_machine;
	}
	*virtual_cpu=vcpu;
	return noir_success;
alloc_failure:
	return noir_insufficient_resources;
}

void nvc_svmc_release_vm(noir_svm_custom_vm_p vm)
{
	if(vm)
	{
		for(noir_svm_custom_vcpu_p vcpu=vm->vcpu.head;vcpu;vcpu=vcpu->next)
			nvc_svmc_release_vcpu(vcpu);
		if(vm->nptm.ncr3.virt)
			noir_free_contd_memory(vm->nptm.ncr3.virt);
		for(noir_npt_pdpte_descriptor_p pdpte_d=vm->nptm.pdpte.head;pdpte_d;pdpte_d=pdpte_d->next)
		{
			noir_free_contd_memory(pdpte_d->virt);
			noir_free_nonpg_memory(pdpte_d);
		}
		noir_free_nonpg_memory(vm);
	}
}

// Creating a CVM does not create corresponding vCPUs and lower paging structures!
noir_status nvc_svmc_create_vm(noir_svm_custom_vm_p* virtual_machine)
{
	noir_status st=noir_invalid_parameter;
	if(virtual_machine)
	{
		noir_svm_custom_vm_p vm=noir_alloc_nonpg_memory(sizeof(noir_svm_custom_vm));
		st=noir_insufficient_resources;
		if(vm)
		{
			// Create a generic Page Map Level 4 (PML4) Table.
			vm->nptm.ncr3.virt=noir_alloc_contd_memory(page_size);
			if(vm->nptm.ncr3.virt)
			{
				vm->nptm.ncr3.phys=noir_get_physical_address(vm->nptm.ncr3.virt);
				st=noir_success;
			}
		}
	}
	return st;
}