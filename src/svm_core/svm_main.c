/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the basic driver of AMD-V.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_main.c
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

bool nvc_is_svm_supported()
{
	u32 a,b,c,d;
	noir_cpuid(0x80000001,0,null,null,&c,null);
	if(noir_bt(&c,amd64_cpuid_svm))
	{
		bool basic_supported=true;
		noir_cpuid(0x8000000A,0,&a,&b,&c,&d);
		// At least one ASID should be available.
		basic_supported&=(b>0);
		// Decode Assists is the required feature.
		basic_supported&=noir_bt(&d,amd64_cpuid_decoder);
		// Next RIP Saving is the required feature.
		basic_supported&=noir_bt(&d,amd64_cpuid_nrips);
		return basic_supported;
	}
	return false;
}

bool nvc_is_npt_supported()
{
	u32 a,b,c,d;
	bool npt_support=true;
	noir_cpuid(0x8000000A,0,&a,&b,&c,&d);
	npt_support&=noir_bt(&d,amd64_cpuid_npt);
	npt_support&=noir_bt(&d,amd64_cpuid_vmcb_clean);
	return npt_support;
}

bool nvc_is_acnested_svm_supported()
{
	u32 a,b,c,d;
	bool acnv_support=true;
	noir_cpuid(0x8000000A,0,&a,&b,&c,&d);
	acnv_support&=noir_bt(&d,amd64_cpuid_vmlsvirt);
	acnv_support&=noir_bt(&d,amd64_cpuid_vgif);
	return acnv_support;
}

bool nvc_is_svm_disabled()
{
	u64 vmcr=noir_rdmsr(amd64_vmcr);
	return noir_bt(&vmcr,amd64_vmcr_svmdis);
}

u8 nvc_svm_enable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	efer|=amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
	efer=noir_rdmsr(amd64_efer);
	if(noir_bt(&efer,amd64_efer_svme)==true)
	{
		u64 vmcr=noir_rdmsr(amd64_vmcr);
		// Block and Disable A20M
		// Not sure about the reason, but
		// Intel blocks A20M in vmxon.
		noir_bts(&vmcr,amd64_vmcr_disa20m);
		noir_wrmsr(amd64_vmcr,vmcr);
		return noir_virt_trans;
	}
	return noir_virt_off;
}

u8 nvc_svm_disable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	// Disable SVM
	efer&=~amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
	efer=noir_rdmsr(amd64_efer);
	if(noir_bt(&efer,amd64_efer_svme)==false)
	{
		u64 vmcr=noir_rdmsr(amd64_vmcr);
		// Unblock and Enable A20M
		// Not sure about the reason, but
		// Intel unblocks A20M in vmxoff.
		noir_btr(&vmcr,amd64_vmcr_disa20m);
		noir_wrmsr(amd64_vmcr,vmcr);
		return noir_virt_off;
	}
	return noir_virt_trans;
}

void static nvc_svm_setup_msr_hook(noir_hypervisor_p hvm_p)
{
	void* bitmap1=(void*)((ulong_ptr)hvm_p->relative_hvm->msrpm.virt+0);
	void* bitmap2=(void*)((ulong_ptr)hvm_p->relative_hvm->msrpm.virt+0x800);
	void* bitmap3=(void*)((ulong_ptr)hvm_p->relative_hvm->msrpm.virt+0x1000);
	// Setup basic MSR-Intercepts that may interfere with SVM normal operations.
	// This is also for nested virtualization.
	noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_efer,0));
	noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_efer,1));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_hsave_pa,0));
	noir_set_bitmap(bitmap3,svm_msrpm_bit(3,amd64_hsave_pa,1));
	// Setup custom MSR-Interception.
#if defined(_amd64)
	unref_var(bitmap1);
	noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_lstar,0));			// Hide MSR Hook
	noir_set_bitmap(bitmap2,svm_msrpm_bit(2,amd64_lstar,1));			// Mask MSR Hook
#else
	noir_set_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_eip,0));		// Hide MSR Hook
	noir_set_bitmap(bitmap1,svm_msrpm_bit(1,amd64_sysenter_eip,1));		// Mask MSR Hook
#endif
}

void static nvc_svm_setup_virtual_msr(noir_svm_vcpu_p vcpu)
{
	noir_svm_virtual_msr_p vmsr=&vcpu->virtual_msr;
#if defined(_amd64)
	vmsr->lstar=(u64)orig_system_call;
#else
	vmsr->sysenter_eip=(u64)orig_system_call;
#endif
}

void static nvc_svm_setup_control_area(noir_svm_vcpu_p vcpu)
{
	nvc_svm_instruction_intercept1 list1;
	nvc_svm_instruction_intercept2 list2;
	u32 a,b,c,d;
	noir_cpuid(0x8000000A,0,&a,&b,&c,&d);
	// Basic features...
	list1.value=0;
	list1.intercept_cpuid=1;
	list1.intercept_msr=1;
	list1.intercept_shutdown=1;
	list2.value=0;
	list2.intercept_vmrun=1;	// The vmrun should always be intercepted as required by AMD.
	list2.intercept_vmmcall=1;
	// Policy: Enable as most features as possible.
	// Save them to vCPU.
	if(d & amd64_cpuid_vmcb_clean_bit)
		vcpu->enabled_feature|=noir_svm_vmcb_caching;
	if(d & amd64_cpuid_npt_bit)
		vcpu->enabled_feature|=noir_svm_nested_paging;
	if(d & amd64_cpuid_flush_asid_bit)
		vcpu->enabled_feature|=noir_svm_flush_by_asid;
	if(d & amd64_cpuid_vgif_bit)
		vcpu->enabled_feature|=noir_svm_virtual_gif;
	if(d & amd64_cpuid_vmlsvirt_bit)
		vcpu->enabled_feature|=noir_svm_virtualized_vmls;
	// Setup Memory Virtualization
	if(vcpu->enabled_feature & noir_svm_nested_paging)
	{
		// Enable NPT
		noir_npt_manager_p nptm=(noir_npt_manager_p)vcpu->relative_hvm->primary_nptm;
		nvc_svm_npt_control npt_ctrl;
		npt_ctrl.value=0;
		npt_ctrl.enable_npt=1;
		noir_svm_vmwrite64(vcpu->vmcb.virt,npt_control,npt_ctrl.value);
		// Write NPT CR3
		noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
	}
	// Write to VMCB.
	noir_svm_vmwrite32(vcpu->vmcb.virt,intercept_instruction1,list1.value);
	noir_svm_vmwrite16(vcpu->vmcb.virt,intercept_instruction2,list2.value);
}

ulong_ptr nvc_svm_subvert_processor_i(noir_svm_vcpu_p vcpu,ulong_ptr gsp,ulong_ptr gip)
{
	// Save Processor State
	noir_processor_state state;
	noir_save_processor_state(&state);
	// Setup State-Save Area
	// Save Segment State - CS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_cs_selector,state.cs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_cs_attrib,svm_attrib(state.cs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_cs_limit,state.cs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cs_base,state.cs.base);
	// Save Segment State - DS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ds_selector,state.ds.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ds_attrib,svm_attrib(state.ds.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ds_limit,state.ds.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ds_base,state.ds.base);
	// Save Segment State - ES
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_es_selector,state.es.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_es_attrib,svm_attrib(state.es.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_es_limit,state.es.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_es_base,state.es.base);
	// Save Segment State - FS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_fs_selector,state.fs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_fs_attrib,svm_attrib(state.fs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_fs_limit,state.fs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_fs_base,state.fs.base);
	// Save Segment State - GS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_gs_selector,state.gs.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_gs_attrib,svm_attrib(state.gs.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_gs_limit,state.gs.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_gs_base,state.gs.base);
	// Save Segment State - SS
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ss_selector,state.ss.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ss_attrib,svm_attrib(state.ss.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ss_limit,state.ss.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ss_base,state.ss.base);
	// Save Segment State - TR
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_tr_selector,state.tr.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_tr_attrib,svm_attrib(state.tr.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_tr_limit,state.tr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_tr_base,state.tr.base);
	// Save GDTR and IDTR
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_gdtr_limit,state.gdtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_gdtr_base,state.gdtr.base);
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_idtr_limit,state.idtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_idtr_base,state.idtr.base);
	// Save Segment State - LDTR
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ldtr_selector,state.ldtr.selector);
	noir_svm_vmwrite16(vcpu->vmcb.virt,guest_ldtr_attrib,svm_attrib(state.ldtr.attrib));
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_ldtr_limit,state.ldtr.limit);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_ldtr_base,state.ldtr.base);
	// Save Control Registers
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr0,state.cr0);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr2,state.cr2);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr3,state.cr3);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_cr4,state.cr4);
#if defined(_amd64)
	// Save Task Priority Register (CR8)
	noir_svm_vmwrite8(vcpu->vmcb.virt,avid_control,(u8)state.cr8);
#endif
	// Save Debug Registers
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_dr6,state.dr6);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_dr7,state.dr7);
	// Save RFlags, RSP and RIP
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rflags,2);	// Fixed bit should be set.
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rsp,gsp);
	noir_svm_vmwrite(vcpu->vmcb.virt,guest_rip,gip);
	// Save Processor Hidden State
	noir_svm_vmsave((ulong_ptr)vcpu->vmcb.phys);
	noir_svm_vmsave((ulong_ptr)vcpu->hvmcb.phys);
	// Save Model Specific Registers.
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_pat,state.pat);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_efer,state.efer);
	vcpu->enabled_feature|=noir_svm_syscall_hook;		// Control of Stealth Syscall-Hook
#if defined(_amd64)
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_star,state.star);
	if(vcpu->enabled_feature & noir_svm_syscall_hook)
		noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,(u64)noir_system_call);
	else
		noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,(u64)orig_system_call);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_cstar,state.cstar);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_sfmask,state.sfmask);
	noir_svm_vmwrite64(vcpu->vmcb.virt,guest_kernel_gs_base,state.gsswap);
#else
	if(vcpu->enabled_feature & noir_svm_syscall_hook)
		noir_svm_vmwrite32(vcpu->vmcb.virt,guest_sysenter_eip,(u32)noir_system_call);
	else
		noir_svm_vmwrite32(vcpu->vmcb.virt,guest_sysenter_eip,(u32)orig_system_call);
#endif
	// Setup Control Area
	nvc_svm_setup_control_area(vcpu);
	// Setup IOPM and MSRPM.
	noir_svm_vmwrite64(vcpu->vmcb.virt,iopm_physical_address,vcpu->relative_hvm->iopm.phys);
	noir_svm_vmwrite64(vcpu->vmcb.virt,msrpm_physical_address,vcpu->relative_hvm->msrpm.phys);
	noir_svm_vmwrite32(vcpu->vmcb.virt,guest_asid,1);		// ASID must be non-zero.
	// We will assign a guest asid other than 1 as we are nesting a hypervisor.
	// Load Partial Guest State by vmload and continue subversion.
	noir_svm_vmload((ulong_ptr)vcpu->vmcb.phys);
	// "Return" puts the value onto rax register.
	return (ulong_ptr)vcpu->vmcb.phys;
}

void static nvc_svm_subvert_processor(noir_svm_vcpu_p vcpu)
{
	vcpu->status=nvc_svm_enable();
	if(vcpu->status==noir_virt_trans)
	{
		noir_svm_initial_stack_p stack=(noir_svm_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_svm_initial_stack));
		stack->guest_vmcb_pa=vcpu->vmcb.phys;
		stack->host_vmcb_pa=vcpu->hvmcb.phys;
		stack->proc_id=vcpu->proc_id;
		stack->vcpu=vcpu;
		noir_wrmsr(amd64_hsave_pa,vcpu->hsave.phys);
		nvc_svm_setup_virtual_msr(vcpu);
		vcpu->status=nvc_svm_subvert_processor_a(stack);
	}
}

void static nvc_svm_subvert_processor_thunk(void* context,u32 processor_id)
{
	noir_svm_vcpu_p vcpu=(noir_svm_vcpu_p)context;
	vcpu[processor_id].proc_id=processor_id;
	nvc_svm_subvert_processor(&vcpu[processor_id]);
}

void nvc_svm_cleanup(noir_hypervisor_p hvm_p)
{
	if(hvm_p->virtual_cpu)
	{
		u32 i=0;
		for(;i<hvm_p->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm_p->virtual_cpu[i];
			if(vcpu->vmcb.virt)
				noir_free_contd_memory(vcpu->vmcb.virt);
			if(vcpu->hsave.virt)
				noir_free_contd_memory(vcpu->hsave.virt);
			if(vcpu->hvmcb.virt)
				noir_free_contd_memory(vcpu->hvmcb.virt);
			if(vcpu->hv_stack)
				noir_free_nonpg_memory(vcpu->hv_stack);
		}
		noir_free_nonpg_memory(hvm_p->virtual_cpu);
	}
	if(hvm_p->relative_hvm->msrpm.virt)
		noir_free_contd_memory(hvm_p->relative_hvm->msrpm.virt);
	if(hvm_p->relative_hvm->iopm.virt)
		noir_free_contd_memory(hvm_p->relative_hvm->iopm.virt);
	if(hvm_p->relative_hvm->blank_page.virt)
		noir_free_contd_memory(hvm_p->relative_hvm->blank_page.virt);
	if(hvm_p->relative_hvm->primary_nptm)
		nvc_npt_cleanup(hvm_p->relative_hvm->primary_nptm);
	if(hvm_p->relative_hvm->secondary_nptm)
		nvc_npt_cleanup(hvm_p->relative_hvm->secondary_nptm);
	nvc_svm_teardown_exit_handler();
}

// Calculate N*2MiB-4KiB size of allocation.
u32 nvc_svm_get_allocation_size(noir_hypervisor_p hvm_p)
{
	u32 discrete_size,cumulate_pages;
	// Accumulate small pages...
	u32 size_total=hvm_p->cpu_count*nvc_stack_size;		// Size of all vCPUs.
	size_total+=14*page_size;							// Hypervisor + NPT Managers.
	discrete_size=page_2mb_offset(size_total);
	cumulate_pages=page_2mb_count(size_total);
	if(discrete_size)cumulate_pages++;
	size_total=page_2mb_mult(cumulate_pages)-page_size;
	// Add PDE Pages...
	size_total+=3*page_2mb_size;						// 2 PDEs and 1 fail-safe 2MiB page.
	return size_total;
}

/*
  Memory Layout per vCPU: (Total: 64KiB)
  +0x0000:	Reserved for "Saved State" while restoring system.
  +0x1000:	Guest VMCB.
  +0x2000:	Host VMCB.
  +0x3000:	Host Save Page.
  +0x4000:	8 Nested L2T VMCBs.
  +0xC000:	Stack Bottom Address.
  (Remaining space are for stack. Stack Size: 16KiB)
  +0xFFE0:	Initial Stack Pointer for Hypervisor.

  Memory Layout for NPT Managers: (Base: 24KiB)
  +0x0000:	Primary NPT Manager
  +0x0040:	Secondary NPT Manager
  +0x0080:	PTE Descriptors Array.
  (Remaining size toward 4KiB is for PTE Descriptors)
  +0x1000:	Primary PML4E Page.
  +0x2000:	Primary PDPTE Page.
  +0x3000:	Secondary PML4E Page.
  +0x4000:	Secondary PDPTE Page.
  +0x5000:	Blank Page
  +0x6000:	PTE Pages...
  ......
  (Aligned at 2MiB) PDE Pages...

  Memory Layout for Hypervisor: (Base: 32KiB)
  +0x0000:	MSRPM
  +0x2000:	IOPM
  +0x5000:	Virtual Processors
  +0x6000:	NPT Managers & vCPUs

  Overall Allocation Size Formula:
  vCPU Size + Hypervisor Size + NPTM Size + PDE Pages + Alignment
*/
/*
noir_status nvc_svm_subvert_system(noir_hypervisor_p hvm_p)
{
	u32 alloc_size;
	hvm_p->cpu_count=noir_get_processor_count();
	alloc_size=nvc_svm_get_allocation_size(hvm_p);
	hvm_p->address_start=noir_alloc_contd_memory(alloc_size);
	if(hvm_p->address_start)
	{
		// Initialize Page Allocator
		ulong_ptr lo=(ulong_ptr)hvm_p->address_start,hi=lo+alloc_size;
		u32 lob=(u32)page_2mb_offset(lo),hib=(u32)page_2mb_offset(hi);
		hvm_p->alloc_pos_lo=lo;
		if(lob)hvm_p->lower_remainder=page_2mb_size-lob;
		hvm_p->alloc_pos_hi=hvm_p->alloc_pos_lo+hvm_p->lower_remainder+2*page_2mb_size;
		// Start allocation - Relative HVM
		hvm_p->relative_hvm->msrpm.virt=nvc_hv_alloc_page(hvm_p,2);
		if(hvm_p->relative_hvm->msrpm.virt)
			hvm_p->relative_hvm->msrpm.phys=noir_get_physical_address(hvm_p->relative_hvm->msrpm.virt);
		else
			goto alloc_failure;
		hvm_p->relative_hvm->iopm.virt=nvc_hv_alloc_page(hvm_p,3);
		if(hvm_p->relative_hvm->iopm.virt)
			hvm_p->relative_hvm->iopm.phys=noir_get_physical_address(hvm_p->relative_hvm->iopm.virt);
		else
			goto alloc_failure;
		hvm_p->relative_hvm->blank_page.virt=nvc_hv_alloc_page(hvm_p,1);
		if(hvm_p->relative_hvm->blank_page.virt)
			hvm_p->relative_hvm->blank_page.phys=noir_get_physical_address(hvm_p->relative_hvm->blank_page.virt);
		else
			goto alloc_failure;
		// Start allocation - vCPU
		hvm_p->virtual_cpu=nvc_hv_alloc_page(hvm_p,2);
		if(hvm_p->virtual_cpu)
		{
			noir_svm_vcpu_p vcpu=hvm_p->virtual_cpu;
			u32 i=0;
			for(;i<hvm_p->cpu_count;vcpu=&hvm_p->virtual_cpu[++i])
			{
				vcpu->hv_stack=nvc_hv_alloc_page(nvc_stack_pages);
				if(vcpu->hv_stack==null)goto alloc_failure;
				vcpu->vmcb.virt=(void*)((ulong_ptr)vcpu->hv_stack+0x2000);
				vcpu->hvmcb.virt=(void*)((ulong_ptr)vcpu->hv_stack+0x3000);
				vcpu->hsave.virt=(void*)((ulong_ptr)vcpu->hv_stack+0x4000);
				vcpu->cpuid_cache.cache_base=(void*)((ulong_ptr)vcpu->hv_stack+sizeof(noir_gpr_state));
			}
		}
	}
alloc_failure:
	return noir_insufficient_resources;
}
*/

noir_status nvc_svm_subvert_system(noir_hypervisor_p hvm_p)
{
	hvm_p->cpu_count=noir_get_processor_count();
	if(nvc_svm_build_exit_handler()==false)goto alloc_failure;
	hvm_p->virtual_cpu=noir_alloc_nonpg_memory(hvm_p->cpu_count*sizeof(noir_svm_vcpu));
	// Implementation of Generic Call might differ.
	// In subversion routine, it might not be allowed to allocate memory.
	// Thus allocate everything at this moment, even if it costs more on single processor core.
	if(hvm_p->virtual_cpu)
	{
		u32 i=0;
		for(;i<hvm_p->cpu_count;i++)
		{
			noir_svm_vcpu_p vcpu=&hvm_p->virtual_cpu[i];
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
			vcpu->hvmcb.virt=noir_alloc_contd_memory(page_size);
			if(vcpu->hvmcb.virt)
				vcpu->hvmcb.phys=noir_get_physical_address(vcpu->hvmcb.virt);
			else
				goto alloc_failure;
			vcpu->hv_stack=noir_alloc_nonpg_memory(nvc_stack_size);
			if(vcpu->hv_stack==null)return noir_insufficient_resources;
			vcpu->relative_hvm=(noir_svm_hvm_p)hvm_p->reserved;
		}
	}
	hvm_p->relative_hvm=(noir_svm_hvm_p)hvm_p->reserved;
	hvm_p->relative_hvm->hvm_cpuid_leaf_max=nvc_mshv_build_cpuid_handlers();
	if(hvm_p->relative_hvm->hvm_cpuid_leaf_max==0)goto alloc_failure;
	hvm_p->relative_hvm->msrpm.virt=noir_alloc_contd_memory(2*page_size);
	if(hvm_p->relative_hvm->msrpm.virt)
		hvm_p->relative_hvm->msrpm.phys=noir_get_physical_address(hvm_p->relative_hvm->msrpm.virt);
	else
		goto alloc_failure;
	hvm_p->relative_hvm->iopm.virt=noir_alloc_contd_memory(3*page_size);
	if(hvm_p->relative_hvm->iopm.virt)
		hvm_p->relative_hvm->iopm.phys=noir_get_physical_address(hvm_p->relative_hvm->iopm.virt);
	else
		goto alloc_failure;
	hvm_p->relative_hvm->primary_nptm=(void*)nvc_npt_build_identity_map();
	if(hvm_p->relative_hvm->primary_nptm==null)goto alloc_failure;
	if(nvc_npt_initialize_ci(hvm_p->relative_hvm->primary_nptm)==false)goto alloc_failure;
	hvm_p->relative_hvm->secondary_nptm=(void*)nvc_npt_build_identity_map();
	if(hvm_p->relative_hvm->secondary_nptm==null)goto alloc_failure;
	if(hvm_p->virtual_cpu==null)goto alloc_failure;
	nvc_svm_setup_msr_hook(hvm_p);
	// nvc_npt_build_hook_mapping(hvm_p);
	if(nvc_npt_protect_critical_hypervisor(hvm_p)==false)goto alloc_failure;
	nv_dprintf("All allocations are done, start subversion!\n");
	noir_generic_call(nvc_svm_subvert_processor_thunk,hvm_p->virtual_cpu);
	nv_dprintf("Subversion completed!\n");
	return noir_success;
alloc_failure:
	nv_dprintf("Allocation failure!\n");
	noir_int3();
	nvc_svm_cleanup(hvm_p);
	return noir_insufficient_resources;
}

void static nvc_svm_restore_processor(noir_svm_vcpu_p vcpu)
{
	// Leave Guest Mode by vmmcall if we are in Guest Mode.
	if(vcpu->status==noir_virt_on)
		noir_svm_vmmcall(noir_svm_callexit,(ulong_ptr)vcpu);
	// Mark the processor is in "off" status as we are in Host Mode now.
	if(vcpu->status==noir_virt_trans)
		vcpu->status=noir_virt_off;
	nvc_svm_disable();
}

void static nvc_svm_restore_processor_thunk(void* context,u32 processor_id)
{
	noir_svm_vcpu_p vcpu=(noir_svm_vcpu_p)context;
	nv_dprintf("Processor %d entered restoration routine!\n",processor_id);
	nvc_svm_restore_processor(&vcpu[processor_id]);
}

void nvc_svm_restore_system(noir_hypervisor_p hvm_p)
{
	if(hvm_p->virtual_cpu)
	{
		noir_generic_call(nvc_svm_restore_processor_thunk,hvm_p->virtual_cpu);
		nvc_svm_cleanup(hvm_p);
		nvc_mshv_teardown_cpuid_handlers();
	}
}