/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the central HyperVisor of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/noirhvm.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <intrin.h>

u32 noir_visor_version()
{
	u16 major=1;
	u16 minor=0;
	return major<<16|minor;
}

void noir_get_vendor_string(char* vendor_string)
{
	noir_cpuid(0,0,null,(u32*)&vendor_string[0],(u32*)&vendor_string[8],(u32*)&vendor_string[4]);
	vendor_string[12]=0;
}

void noir_get_processor_name(char* processor_name)
{
	noir_cpuid(0x80000002,0,(u32*)&processor_name[0x00],(u32*)&processor_name[0x04],(u32*)&processor_name[0x08],(u32*)&processor_name[0x0C]);
	noir_cpuid(0x80000003,0,(u32*)&processor_name[0x10],(u32*)&processor_name[0x14],(u32*)&processor_name[0x18],(u32*)&processor_name[0x1C]);
	noir_cpuid(0x80000004,0,(u32*)&processor_name[0x20],(u32*)&processor_name[0x24],(u32*)&processor_name[0x28],(u32*)&processor_name[0x2C]);
	processor_name[0x30]=0;
}

u8 nvc_confirm_cpu_manufacturer(char* vendor_string)
{
	u32 min=0,max=known_vendor_strings;
	while(max>=min)
	{
		u32 mid=(min+max)>>1;
		char* vsn=vendor_string_list[mid];
		i32 cmp=strcmp(vendor_string,vsn);
		if(cmp>0)
			min=mid+1;
		else if(cmp<0)
			max=mid-1;
		else
			return cpu_manuf_list[mid];
	}
	return unknown_processor;
}

u32 noir_get_virtualization_supportability()
{
	char vstr[13];
	bool basic=false;		// Basic Intel VT-x/AMD-V
	bool slat=false;		// Basic Intel EPT/AMD NPT
	bool acnest=false;		// Accelerated Virtualization Nesting
	u32 result=0;
	u8 manuf=0;
	noir_get_vendor_string(vstr);
	manuf=nvc_confirm_cpu_manufacturer(vstr);
	if(manuf==intel_processor || manuf==via_processor || manuf==zhaoxin_processor)
	{
		basic=nvc_is_vt_supported();
		slat=nvc_is_ept_supported();
		acnest=nvc_is_vmcs_shadowing_supported();
	}
	else if(manuf==amd_processor || manuf==hygon_processor)
	{
		basic=nvc_is_svm_supported();
		slat=nvc_is_npt_supported();
		acnest=nvc_is_acnested_svm_supported();
	}
	result|=basic<<0;
	result|=slat<<1;
	result|=acnest<<2;
	return result;
}

bool noir_is_under_hvm()
{
	u32 c=0;
	// cpuid[eax=1].ecx[bit 31] could indicate hypervisor's presence.
	noir_cpuid(1,0,null,null,&c,null);
	// If it is read as one. There must be a hypervisor.
	// Otherwise, it is inconclusive to determine the presence.
	return noir_bt(&c,31);
}

noir_status nvc_build_hypervisor()
{
	hvm_p=noir_alloc_nonpg_memory(sizeof(noir_hypervisor));
	if(hvm_p)
	{
		noir_get_vendor_string(hvm_p->vendor_string);
		hvm_p->cpu_manuf=nvc_confirm_cpu_manufacturer(hvm_p->vendor_string);
		nvc_store_image_info(&hvm_p->hv_image.base,&hvm_p->hv_image.size);
		switch(hvm_p->cpu_manuf)
		{
			case intel_processor:
				goto vmx_subversion;
			case amd_processor:
				goto svm_subversion;
			case via_processor:
				goto vmx_subversion;
			case zhaoxin_processor:
				goto vmx_subversion;
			case hygon_processor:
				goto svm_subversion;
			default:
			{
				if(hvm_p->cpu_manuf==unknown_processor)
					nv_dprintf("You are using unknown manufacturer's processor!\n");
				else
					nv_dprintf("Your processor is manufactured by rare known vendor that NoirVisor currently failed to support!\n");
				return noir_unknown_processor;
			}
vmx_subversion:
			if(nvc_is_vt_supported())
			{
				nv_dprintf("Starting subversion with VMX Engine!\n");
				return nvc_vt_subvert_system(hvm_p);
			}
			else
			{
				nv_dprintf("Your processor does not support Intel VT-x!\n");
				return noir_vmx_not_supported;
			}
svm_subversion:
			if(nvc_is_svm_supported())
			{
				nv_dprintf("Starting subversion with SVM Engine!\n");
				return nvc_svm_subvert_system(hvm_p);
			}
			else
			{
				nv_dprintf("Your processor does not support AMD-V!\n");
				return noir_svm_not_supported;
			}
		}
	}
	return noir_insufficient_resources;
}

void nvc_teardown_hypervisor()
{
	if(hvm_p)
	{
		switch(hvm_p->cpu_manuf)
		{
			case intel_processor:
				goto vmx_restoration;
			case amd_processor:
				goto svm_restoration;
			case via_processor:
				goto vmx_restoration;
			case zhaoxin_processor:
				goto vmx_restoration;
			case hygon_processor:
				goto svm_restoration;
			default:
			{
				nv_dprintf("Unknown Processor!\n");
				goto end_restoration;
			}
vmx_restoration:
			nvc_vt_restore_system(hvm_p);
			goto end_restoration;
svm_restoration:
			nvc_svm_restore_system(hvm_p);
			goto end_restoration;
end_restoration:
			nv_dprintf("Restoration Complete...\n");
		}
		noir_free_nonpg_memory(hvm_p);
	}
}