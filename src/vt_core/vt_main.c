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

bool nvc_is_vt_supported()
{
	u32 c;
	noir_cpuid(1,0,null,null,&c,null);
	if(noir_bt(&c,ia32_cpuid_vmx))
	{
		bool basic_supported=true;
		return basic_supported;
	}
	return false;
}