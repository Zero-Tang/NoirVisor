/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the MSR Handler of MSHV Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /mshv_core/mshv_msr.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include "mshv_msr.h"

// Return value will be included in rax register.
// Accessing eax will clear higher 32 bits in rax.
u8 nvc_mshv_hypercall_code64[6]=
{
	0xB8,0x02,0x00,0x00,0x00,	// mov eax,HV_STATUS_INVALID_HYPERCALL_CODE
	0xC3						// ret
};

// It is required to process out edx register as return value.
u8 nvc_mshv_hypercall_code32[8]=
{
	0xB8,0x02,0x00,0x00,0x00,	// mov eax,HV_STATUS_INVALID_HYPERCALL_CODE
	0x33,0xD2,					// xor edx,edx
	0xC3						// ret
};

u64 static fastcall nvc_mshv_msr_r40000000_handler(noir_mshv_vcpu_p vcpu,bool write,u64 val)
{
	if(write)
	{
		noir_locked_xchg64(&noir_mshv_guest_os_id,val);
		if(noir_bt64(&val,63))
		{
			noir_mshv_msr_open_source_guest_os_id os_id;
			nvd_printf("Guest OS is Open-Source!\n");
			os_id.value=val;
			if(os_id.os_type<mshv_os_type_maximum)
				nvd_printf("Guest OS is %s!\n",mshv_known_os_types[os_id.os_type]);
			else
				nvd_printf("Guest OS is Unknown (ID: %u)\n",os_id.os_type);
			nvd_printf("Distribution ID: %u, Upstream Kernel Version: 0x%X, Build Number: %u\n",os_id.os_id,os_id.version,os_id.build_number);
		}
		else
		{
			noir_mshv_msr_proprietary_guest_os_id os_id;
			nvd_printf("Guest OS is Proprietary!\n");
			os_id.value=val;
			switch(os_id.vendor_id)
			{
				case mshv_vendor_microsoft:
					nvd_printf("Guest OS Vendor is Microsoft!\n");
					if(os_id.os_id<mshv_microsoft_os_maximum)
						nvd_printf("Guest OS ID is %s\n",mshv_known_microsoft_os_ids[os_id.os_id]);
					else
						nvd_printf("Guest OS ID is Undefined (ID: %u)\n",os_id.os_id);
					break;
				case mshv_vendor_hpe:
					nvd_printf("Guest OS Vendor is HPE!\n");
					break;
				case mshv_vendor_lancom:
					nvd_printf("Guest OS Vendor is LANCOM!\n");
					break;
				default:
					nvd_printf("Guest OS Vendor is Unknown (ID: 0x%X)!\n",os_id.vendor_id);
					break;
			}
			nvd_printf("Guest OS Version: %u.%u.%u (Service Pack %u)\n",os_id.major_version,os_id.minor_version,os_id.build_number,os_id.service_version);
		}
	}
	return noir_mshv_guest_os_id;
}

u64 static fastcall nvc_mshv_msr_r40000001_handler(noir_mshv_vcpu_p vcpu,bool write,u64 val)
{
	if(write && noir_mshv_guest_os_id)
	{
		noir_mshv_msr_hypercall msr;
		msr.value=val;
		if(msr.locked==false)
		{
			noir_locked_xchg(&noir_mshv_hypercall_ctrl,val);
			if(msr.enable)
			{
				// MSHV-TLFS tells us to map a page.
				// However, we may just copy stuff onto it.
				u64 pa=page_mult(msr.hypercall_gpfn);
				void* va=noir_find_virt_by_phys(pa);
				noir_movsb(va,nvc_mshv_hypercall_code64,sizeof(nvc_mshv_hypercall_code64));
				noir_stosb((void*)((ulong_ptr)va+sizeof(nvc_mshv_hypercall_code64)),0,page_size-sizeof(nvc_mshv_hypercall_code64));
			}
		}
	}
	return noir_mshv_hypercall_ctrl;
}

u64 static fastcall nvc_mshv_msr_r40000002_handler(noir_mshv_vcpu_p vcpu,bool write,u64 val)
{
	return vcpu->vp_index;
}

u64 static fastcall nvc_mshv_msr_r40000040_handler(noir_mshv_vcpu_p vcpu,bool write,u64 val)
{
	if(write)
	{
		vcpu->npiep_config=val;
		// Reconfigure the Interceptions.
		if(hvm_p->selected_core==use_svm_core)nvc_svm_reconfigure_npiep_interceptions(vcpu->root_vcpu);
	}
	return vcpu->npiep_config;
}

u64 fastcall nvc_mshv_rdmsr_handler(noir_mshv_vcpu_p vcpu,u32 index)
{
	nvd_printf("Intercepted Microsoft Synthetic MSR-Read! Index=0x%X\n",index);
	switch(index)
	{
		case hv_x64_msr_guest_os_id:
			return nvc_mshv_msr_r40000000_handler(vcpu,false,0);
		case hv_x64_msr_hypercall:
			return nvc_mshv_msr_r40000001_handler(vcpu,false,0);
		case hv_x64_msr_vp_index:
			return nvc_mshv_msr_r40000002_handler(vcpu,false,0);
		case hv_x64_msr_npiep_config:
			return nvc_mshv_msr_r40000040_handler(vcpu,false,0);
	}
	return 0;
}

void fastcall nvc_mshv_wrmsr_handler(noir_mshv_vcpu_p vcpu,u32 index,u64 val)
{
	nvd_printf("Intercepted Microsoft Synthetic MSR-Write! Index=0x%X, Value=0x%016llX\n",index,val);
	switch(index)
	{
		case hv_x64_msr_guest_os_id:
			nvc_mshv_msr_r40000000_handler(vcpu,true,val);
			break;
		case hv_x64_msr_hypercall:
			nvc_mshv_msr_r40000001_handler(vcpu,true,val);
			break;
		case hv_x64_msr_vp_index:
			nvc_mshv_msr_r40000002_handler(vcpu,true,val);
			break;
		case hv_x64_msr_npiep_config:
			nvc_mshv_msr_r40000040_handler(vcpu,true,val);
			break;
	}
}