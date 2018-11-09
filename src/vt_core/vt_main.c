/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is memory management facility on Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/memory.c
*/

#include <nvdef.h>
#include <intrin.h>
#include <ia32.h>
#include "vmxdef.h"

bool nvc_is_vt_supported()
{
	u32 c;
	noir_cpuid(1,0,null,null,&c,null);
	if(noir_bt(&c,ia32_cpuid_vmx))
	{
		bool basic_supported=true;
		ia32_vmx_basic_msr vt_basic;
		vt_basic.value=noir_rdmsr(ia32_vmx_basic);
		if(vt_basic.memory_type==6)		//Make sure CPU supports Write-Back VMCS.
		{
			ia32_vmx_priproc_ctrl_msr prictrl_msr;
			if(vt_basic.use_true_msr)
				prictrl_msr.value=noir_rdmsr(ia32_vmx_true_priproc_ctrl);
			else
				prictrl_msr.value=noir_rdmsr(ia32_vmx_priproc_ctrl);
			//Support of MSR-Bitmap is essential.
			basic_supported&=prictrl_msr.allowed1_settings.use_msr_bitmap;
		}
		return basic_supported;
	}
	return false;
}