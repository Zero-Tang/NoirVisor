/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the central HyperVisor of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/noirhvm.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <intrin.h>

void nvc_build_hypervisor()
{
	hvm=noir_alloc_nonpg_memory(sizeof(noir_hypervisor));
	if(hvm)
	{
		u32 m=0;
		noir_cpuid(0,0,&m,(u32*)&hvm->vendor_string[0],(u32*)&hvm->vendor_string[8],(u32*)&hvm->vendor_string[4]);
		hvm->vendor_string[12]=0;
		if(strcmp(hvm->vendor_string,"GenuineIntel")==0)
			hvm->cpu_manuf=intel_processor;
		else if(strcmp(hvm->vendor_string,"AuthenticAMD")==0)
			hvm->cpu_manuf=amd_processor;
		else
			hvm->cpu_manuf=unknown_processor;
		switch(hvm->cpu_manuf)
		{
			case intel_processor:
			{
				break;
			}
			case amd_processor:
			{
				break;
			}
			default:
			{
				nv_dprintf("Unknown Processor!\n");
				break;
			}
		}
	}
}