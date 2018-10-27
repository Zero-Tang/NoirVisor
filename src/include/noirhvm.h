/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the central HyperVisor of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/noirhvm.h
*/

#include "nvdef.h"
#if defined(_vt_drv)
#include "vt_hvm.h"
#elif defined(_svm_drv)
#include "svm_hvm.h"
#endif

//Known Processor Manufacturer Encoding
#define intel_processor			0
#define amd_processor			1
#define unknown_processor		0xff

typedef struct _noir_hypervisor
{
#if defined(_vt_drv)
	noir_vt_vcpu_p virtual_cpu;
#elif defined(_svm_drv)
	noir_svm_vcpu_p virtual_cpu;
#else
	void* virtual_cpu;
#endif
	struct
	{
		ulong_ptr base;
		u32 size;
	}hv_image;
	u32 cpu_count;
	char vendor_string[13];
	u8 cpu_manuf;
}noir_hypervisor,*noir_hypervisor_p;

noir_hypervisor_p hvm=null;