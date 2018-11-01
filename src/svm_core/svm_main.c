/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the basic driver of AMD-V.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_main.c
*/

#include <nvdef.h>
#include <noirhvm.h>
#include <nvbdk.h>
#include <intrin.h>
#include <amd64.h>

bool nvc_is_svm_supported()
{
	u32 a,b,c,d;
	char vs[13];
	noir_cpuid(0x80000000,0,&a,(u32*)&vs[0],(u32*)&vs[8],(u32*)&vs[4]);vs[12]=0;
	//Make sure that processor is produced by AMD and
	//maximum supported cpuid leaf is higher than 0x8000000A
	if(strcmp(vs,"AuthenticAMD")==0 && a>=0x8000000A)
	{
		noir_cpuid(0x80000001,0,null,null,&c,null);
		if(noir_bt(&c,amd64_cpuid_svm))
		{
			bool basic_supported=true;
			noir_cpuid(0x8000000A,0,&a,&b,&c,&d);
			//At least one ASID should be available.
			basic_supported&=(b>0);
			//Decode Assists is the required feature.
			basic_supported&=noir_bt(&d,amd64_cpuid_decoder);
			//Next RIP Saving is the required feature.
			basic_supported&=noir_bt(&d,amd64_cpuid_nrips);
			return basic_supported;
		}
	}
	return false;
}

u8 nvc_svm_enable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	efer|=amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
	efer=noir_rdmsr(amd64_efer);
	return noir_bt(efer,amd64_efer_svme)?noir_virt_trans:noir_virt_off;
}

u8 nvc_svm_disable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	efer&=~amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
	efer=noir_rdmsr(amd64_efer);
	return noir_bt(efer,amd64_efer_svme)?noir_virt_trans:noir_virt_off;
}

ulong_ptr nvc_svm_subvert_processor_i(noir_svm_vcpu_p vcpu,ulong_ptr gsp,ulong_ptr gip)
{
	;
	return (ulong_ptr)vcpu->vmcb.phys;
}

void static nvc_svm_subvert_processor(noir_svm_vcpu_p vcpu)
{
	vcpu->status=nvc_svm_enable();
	noir_wrmsr(amd64_hsave_pa,vcpu->hsave.phys);
	vcpu->status=nvc_svm_subvert_processor_a(vcpu);
}

void static nvc_svm_subvert_processor_thunk(void* context,u32 processor_id)
{
	noir_svm_vcpu_p vcpu=(noir_svm_vcpu_p)context;
	nvc_svm_subvert_processor(&vcpu[processor_id]);
}

void nvc_svm_cleanup(noir_hypervisor_p hvm)
{
	if(hvm->virtual_cpu)
	{
		u32 i=0;
		for(;i<hvm->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm->virtual_cpu[i];
			if(vcpu->vmcb.virt)
				noir_free_contd_memory(vcpu->vmcb.virt);
			if(vcpu->hsave.virt)
				noir_free_contd_memory(vcpu->hsave.virt);
			if(vcpu->hv_stack)
				noir_free_nonpg_memory(vcpu->hv_stack);
		}
		noir_free_nonpg_memory(hvm->virtual_cpu);
	}
	if(hvm->relative_hvm->msrpm.virt)
		noir_free_contd_memory(hvm->relative_hvm->msrpm.virt);
	if(hvm->relative_hvm->iopm.virt)
		noir_free_contd_memory(hvm->relative_hvm->iopm.virt);
	if(host_rsp_list)
		noir_free_nonpg_memory(host_rsp_list);
}

void nvc_svm_subvert_system(noir_hypervisor_p hvm)
{
	hvm->cpu_count=noir_get_processor_count();
	host_rsp_list=noir_alloc_nonpg_memory(hvm->cpu_count*sizeof(void*));
	if(host_rsp_list==null)return;
	hvm->virtual_cpu=noir_alloc_nonpg_memory(hvm->cpu_count*sizeof(noir_svm_vcpu));
	if(hvm->virtual_cpu)
	{
		u32 i=0;
		for(;i<hvm->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm->virtual_cpu[i];
			vcpu->vmcb.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->vmcb.virt)
				vcpu->vmcb.phys=noir_get_physical_address(vcpu->vmcb.virt);
			else
				goto alloc_failure;
			vcpu->hsave.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->hsave.virt)
				vcpu->hsave.phys=noir_get_physical_address(vcpu->hsave.virt);
			else
				goto alloc_failure;
			vcpu->hv_stack=noir_alloc_nonpg_memory(nvc_stack_size);
			if(vcpu->hv_stack)
				host_rsp_list[i]=vcpu->hv_stack;
			else
				goto alloc_failure;
		}
	}
	hvm->relative_hvm=(noir_svm_hvm_p)hvm->reserved;
	hvm->relative_hvm->msrpm.virt=noir_alloc_contd_memory(2*page_size);
	if(hvm->relative_hvm->msrpm.virt)
		hvm->relative_hvm->msrpm.phys=noir_get_physical_address(hvm->relative_hvm->msrpm.virt);
	else
		goto alloc_failure;
	hvm->relative_hvm->iopm.virt=noir_alloc_contd_memory(3*page_size);
	if(hvm->relative_hvm->iopm.virt)
		hvm->relative_hvm->iopm.phys=noir_get_physical_address(hvm->relative_hvm->iopm.virt);
	else
		goto alloc_failure;
	if(hvm->virtual_cpu==null)goto alloc_failure;
	noir_generic_call(nvc_svm_subvert_processor_thunk,hvm->virtual_cpu);
alloc_failure:
	nvc_svm_cleanup(hvm);
}