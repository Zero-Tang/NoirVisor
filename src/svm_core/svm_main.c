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

void nvc_svm_enable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	efer|=amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
}

void nvc_svm_disable()
{
	u64 efer=noir_rdmsr(amd64_efer);
	efer&=~amd64_efer_svme_bit;
	noir_wrmsr(amd64_efer,efer);
}