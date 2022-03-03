/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the CI (Code Integrity) Component.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/ci.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include <amd64.h>
#include <ci.h>

// Use CRC32 Castagnoli Algorithm.
u32 static stdcall noir_crc32_page_std(void* page)
{
	u8* buf=(u8*)page;
	u32 crc=0xffffffff;
	for(u32 i=0;i<page_size;i++)
	{
		u8 crc_index=(u8)(crc^(buf[i]&0xff));
        crc=crc32c_table[crc_index]^(crc>>8);
    }
    return crc;
}

// This function checks the basic SLAT capability.
// It is specific for the CI component.
// Returning true, this function does not imply
// that stealth inline-hook feature is supported.
bool static noir_check_slat_paging()
{
	u32 a,b,c,d;
	char vsn[13];
	u8 manuf;
	// Confirm the Processor Manufacturer.
	noir_cpuid(0,0,&a,&b,&c,&d);
	*(u32*)&vsn[0]=b;
	*(u32*)&vsn[4]=d;
	*(u32*)&vsn[8]=c;
	vsn[12]=0;
	manuf=nvc_confirm_cpu_manufacturer(vsn);
	// Confirm the Virtualization Engine.
	switch(manuf)
	{
		case intel_processor:
			goto vmx_check;
		case amd_processor:
			goto svm_check;
		case via_processor:
			goto vmx_check;
		case zhaoxin_processor:
			goto vmx_check;
		case hygon_processor:
			goto svm_check;
		default:
			return false;
	}
	// Check capability through different engine.
vmx_check:
	{
		// Check through VT-Core.
		large_integer proc_ctrl2;
		proc_ctrl2.value=noir_rdmsr(ia32_vmx_2ndproc_ctrl);
		return noir_bt(&proc_ctrl2.high,2);
	}
svm_check:
	{
		// Check through SVM-Core.
		noir_cpuid(0x8000000A,0,&a,&b,&c,&d);
		return noir_bt(&c,amd64_cpuid_npt);
	}
	return false;
}

#if !defined(_hv_type1)
u32 static stdcall noir_ci_enforcement_worker(void* context)
{
	// Retrieve Thread Context
	noir_ci_context_p ncie=(noir_ci_context_p)context;
	// Check exit signal.
	while(noir_locked_cmpxchg(&ncie->signal,1,1)==0)
	{
		// Select a page to enforce CI.
		u32 i=ncie->selected++;
		void* page=ncie->page_ci[i].virt;
		// Perform Enforcement.
		u32 crc=noir_crc32_page(page);
		if(crc!=ncie->page_ci[i].crc)
			nvci_panicf("CI detected corruption in Page 0x%p!\n",page);
		else
			nvci_tracef("Page 0x%p scanned. CRC32C=0x%08X - No Anomaly.\n",page,crc);
		// Restore if exceeded.
		if(ncie->selected==ncie->pages)
			ncie->selected=0;
		// Clock.
		noir_sleep(ci_enforcement_delay);
	}
	// Thread is about to exit.
	noir_exit_thread(0);
	return 0;
}
#endif

i32 static cdecl noir_ci_sorting_comparator(const void* a,const void*b)
{
	noir_ci_page_p ap=(noir_ci_page_p)a;
	noir_ci_page_p bp=(noir_ci_page_p)b;
	if(ap->phys>bp->phys)
		return 1;
	else if(ap->phys<bp->phys)
		return -1;
	return 0;
}

bool noir_initialize_ci(void* section,u32 size,bool soft_ci,bool hard_ci)
{
	bool use_hard=hard_ci;
	// Check Intel EPT/AMD NPT supportability.
	use_hard&=noir_check_slat_paging();
	// Either Hardware-Level or Software-Level CI-Enforcement should be enabled.
	// If both are disabled, fail the Code Integrity initialization.
	if(use_hard || soft_ci)
	{
		// Get total count of pages.
		u32 page_num=size>>12;
		if(size & 0xfff)page_num++;
		// Check supportability of SSE4.2.
		if(noir_check_sse42())
			noir_crc32_page=noir_crc32_page_sse;
		else
			noir_crc32_page=noir_crc32_page_std;
		// Setup CI Enforcement Worker Thread.
		noir_ci=noir_alloc_nonpg_memory(sizeof(noir_ci_context)+sizeof(noir_ci_page)*page_num);
		if(noir_ci)
		{
			noir_ci->pages=page_num;
			noir_ci->base=(ulong_ptr)section;
			// Initialize Code Integrity of Code Pages.
			for(u32 i=0;i<page_num;i++)
			{
				noir_ci->page_ci[i].virt=(void*)((ulong_ptr)noir_ci->base+(i<<12));
				noir_ci->page_ci[i].crc=noir_crc32_page(noir_ci->page_ci[i].virt);
				// Physical Address is intended for SLAT-based real-time enforcement.
				// In other words, we don't have to write anything about EPT/NPT here.
				noir_ci->page_ci[i].phys=noir_get_physical_address(noir_ci->page_ci[i].virt);
			}
			// Sort it to accelerate real-time CI.
			// Do the sort only if we enable Hardware-Level CI. Sorting is unnecessary elsewise.
			if(use_hard)noir_qsort(noir_ci->page_ci,page_num,sizeof(noir_ci_page),noir_ci_sorting_comparator);
#if !defined(_hv_type1)
			// No need to trace-print in Type-I hypervisor.
			for(u32 i=0;i<page_num;i++)
				nvci_tracef("Physical: 0x%llX\t CRC32C: 0x%08X\t Virtual: 0x%p\n",noir_ci->page_ci[i].phys,noir_ci->page_ci[i].crc,noir_ci->page_ci[i].virt);
			// Create Worker Thread.
			if(soft_ci)noir_ci->ci_thread=noir_create_thread(noir_ci_enforcement_worker,noir_ci);
			if(noir_ci->ci_thread || soft_ci==false)
				return true;
			else
				noir_free_nonpg_memory(noir_ci);
#endif
		}
	}
	return false;
}

void noir_finalize_ci()
{
	if(noir_ci)
	{
		// Set the signal.
		noir_locked_inc(&noir_ci->signal);
#if !defined(_hv_type1)
		// Wake up thread if sleeping.
		noir_alert_thread(noir_ci->ci_thread);
		// Wait for exit.
		noir_join_thread(noir_ci->ci_thread);
#endif
		// Finalization.
		noir_free_nonpg_memory(noir_ci);
		noir_ci=null;
	}
}