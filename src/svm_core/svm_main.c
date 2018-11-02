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
#include <svm_intrin.h>
#include <intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
#include "svm_def.h"

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
	//Save Processor State
	noir_processor_state state;
	nvc_svm_instruction_intercept1 list1;
	nvc_svm_instruction_intercept2 list2;
	noir_save_processor_state(&state);
	//Setup State-Save Area
	//Save Segment State - CS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_cs_selector,state.cs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_cs_attrib,svm_attrib(state.cs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_cs_limit,state.cs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cs_base,state.cs.base);
	//Save Segment State - DS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ds_selector,state.ds.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ds_attrib,svm_attrib(state.ds.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ds_limit,state.ds.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ds_base,state.ds.base);
	//Save Segment State - ES
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_es_selector,state.es.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_es_attrib,svm_attrib(state.es.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_es_limit,state.es.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_es_base,state.es.base);
	//Save Segment State - FS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_fs_selector,state.fs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_fs_attrib,svm_attrib(state.fs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_fs_limit,state.fs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_fs_base,state.fs.base);
	//Save Segment State - GS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_gs_selector,state.gs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_gs_attrib,svm_attrib(state.gs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_gs_limit,state.gs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_gs_base,state.gs.base);
	//Save Segment State - SS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ss_selector,state.ss.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ss_attrib,svm_attrib(state.ss.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ss_limit,state.ss.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ss_base,state.ss.base);
	//Save Segment State - TR
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_tr_selector,state.tr.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_tr_attrib,svm_attrib(state.tr.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_tr_limit,state.tr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_tr_base,state.tr.base);
	//Save GDTR and IDTR
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_gdtr_limit,state.gdtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_gdtr_base,state.gdtr.base);
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_idtr_limit,state.idtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_idtr_base,state.idtr.base);
	//Save Segment State - LDTR
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ldtr_selector,state.ldtr.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ldtr_attrib,svm_attrib(state.ldtr.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ldtr_limit,state.ldtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ldtr_base,state.ldtr.base);
	//Save Control Registers
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr0,state.cr0);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr2,state.cr2);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr3,state.cr3);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr4,state.cr4);
	//Save Debug Registers
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_dr6,state.dr6);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_dr7,state.dr7);
	//Save RFlags, RSP and RIP
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rflags,2);	//Fixed bit should be set.
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rsp,gsp);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rip,gip);
	//Save Model Specific Registers.
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_pat,state.pat);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_efer,state.efer);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_star,state.star);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,state.lstar);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_cstar,state.cstar);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_sfmask,state.sfmask);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_kernel_gs_base,state.gsswap);
	//Setup Control Area
	list1.value=0;
	list1.intercept_msr=1;
	list2.value=0;
	list2.intercept_vmrun=1;
	noir_svm_vmwrite32(vcpu->vmcb.virt,intercept_instruction1,list1.value);
	noir_svm_vmwrite32(vcpu->vmcb.virt,intercept_instruction2,list2.value);
	noir_svm_vmwrite64(vcpu->vmcb.virt,iopm_physical_address,vcpu->relative_hvm->iopm.phys);
	noir_svm_vmwrite64(vcpu->vmcb.virt,msrpm_physical_address,vcpu->relative_hvm->msrpm.phys);
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_asid,1);		//ASID must be non-zero.
	//Load Partial Guest State by vmload and continue subversion.
	noir_svm_vmload((ulong_ptr)vcpu->vmcb.phys);
	return (ulong_ptr)vcpu->vmcb.phys;
}

void static nvc_svm_subvert_processor(noir_svm_vcpu_p vcpu)
{
	vcpu->status=nvc_svm_enable();
	if(vcpu->status==noir_virt_trans)
	{
		noir_wrmsr(amd64_hsave_pa,vcpu->hsave.phys);
		vcpu->status=nvc_svm_subvert_processor_a(vcpu);
	}
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
			vcpu->relative_hvm=(noir_svm_hvm_p)hvm->reserved;
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