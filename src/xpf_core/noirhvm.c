/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

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
#include <nv_intrin.h>

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

u32 nvc_query_physical_asid_limit(char* vendor_string)
{
	u8 manuf=nvc_confirm_cpu_manufacturer(vendor_string);
	if(manuf==intel_processor || manuf==via_processor || manuf==zhaoxin_processor)
		return nvc_vt_get_avail_vpid();
	else if(manuf==amd_processor || manuf==hygon_processor)
		return nvc_svm_get_avail_asid();
	// Unknown vendor.
	return 0;
}

bool noir_is_virtualization_enabled()
{
	char vstr[13];
	u8 manuf=0;
	noir_get_vendor_string(vstr);
	manuf=nvc_confirm_cpu_manufacturer(vstr);
	if(manuf==intel_processor || manuf==via_processor || manuf==zhaoxin_processor)
		return nvc_is_vt_enabled();
	else if(manuf==amd_processor || manuf==hygon_processor)
		return !nvc_is_svm_disabled();
	// Unknown vendor.
	return false;
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

#if !defined(_hv_type1)
noir_status nvc_run_vcpu(noir_cvm_virtual_cpu_p vcpu)
{
	return noir_not_implemented;
}

noir_status nvc_release_vcpu(noir_cvm_virtual_cpu_p vcpu)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		st=noir_success;
		if(hvm_p->selected_core==use_vt_core)
			st=noir_not_implemented;
		else if(hvm_p->selected_core==use_svm_core)
			nvc_svmc_release_vcpu(vcpu);
		else
			st=noir_unknown_processor;
	}
	return st;
}

noir_status nvc_create_vcpu(noir_cvm_virtual_machine_p vm,noir_cvm_virtual_cpu_p* vcpu)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		st=noir_success;
		if(hvm_p->selected_core==use_vt_core)
			st=noir_not_implemented;
		else if(hvm_p->selected_core==use_svm_core)
			st=nvc_svmc_create_vcpu(vcpu,vm);
		else
			st=noir_unknown_processor;
	}
	return st;
}

noir_status nvc_release_vm(noir_cvm_virtual_machine_p vm)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		st=noir_success;
		// Remove the VM from the list.
		noir_acquire_reslock_exclusive(noir_vm_list_lock);
		noir_remove_list_entry(&vm->active_vm_list);
		// Remove the vCPU list Resource Lock.
		if(vm->vcpu_list_lock)noir_finalize_reslock(vm->vcpu_list_lock);
		// Finally, release the VM structure.
		if(hvm_p->selected_core==use_vt_core)
			st=noir_not_implemented;
		else if(hvm_p->selected_core==use_svm_core)
			nvc_svmc_release_vm(vm);
		else
			st=noir_unknown_processor;
		noir_release_reslock(noir_vm_list_lock);
	}
	return st;
}

noir_status nvc_create_vm(noir_cvm_virtual_machine_p* vm,u32 process_id)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		// Select a core.
		if(hvm_p->selected_core==use_vt_core)
			st=noir_not_implemented;
		else if(hvm_p->selected_core==use_svm_core)
			st=nvc_svmc_create_vm(vm);
		else
			st=noir_unknown_processor;
		if(st==noir_success)
		{
			st=noir_insufficient_resources;
			// Initialize the vCPU list Resource Lock.
			(*vm)->vcpu_list_lock=noir_initialize_reslock();
			if((*vm)->vcpu_list_lock)
			{
				// Insert the VM to the list.
				noir_acquire_reslock_exclusive(noir_vm_list_lock);
				noir_insert_to_prev(&noir_idle_vm.active_vm_list,&(*vm)->active_vm_list);
				noir_release_reslock(noir_vm_list_lock);
				st=noir_success;
			}
			(*vm)->pid=process_id;
		}
	}
	return st;
}

u32 nvc_get_vm_pid(noir_cvm_virtual_machine_p vm)
{
	return vm->pid;
}

u32 nvc_get_vm_asid(noir_cvm_virtual_machine_p vm)
{
	// Idle VM has ASID=1.
	if(vm==&noir_idle_vm)return 1;
	// Check if NoirVisor is present.
	if(hvm_p)
	{
		// Select a core and invoke its function.
		if(hvm_p->selected_core==use_svm_core)
			return nvc_svmc_get_vm_asid(vm);
		else
			return noir_unknown_processor;
	}
	// Zero indicates failure.
	return 0;
}

void nvc_print_vm_list()
{
	noir_cvm_virtual_machine_p head=&noir_idle_vm,cur=null;
	u32 count=0;
	// Acquire the Resource Lock for Shared (Read) Access.
	noir_acquire_reslock_shared(noir_vm_list_lock);
	cur=(noir_cvm_virtual_machine_p)head->active_vm_list.next;
	// Traverse the List and Print to Tracing Log.
	while(cur!=head)
	{
		nv_tracef("[Enum VM] ASID=%u\n",nvc_get_vm_asid(cur));
		cur=(noir_cvm_virtual_machine_p)cur->active_vm_list.next;
		count++;
	}
	nv_tracef("Number of VMs: %u\n",count);
	// Finally, Release the Resource Lock.
	noir_release_reslock(noir_vm_list_lock);
}
#endif

noir_status nvc_build_hypervisor()
{
	hvm_p=noir_alloc_nonpg_memory(sizeof(noir_hypervisor));
	if(hvm_p)
	{
		noir_get_vendor_string(hvm_p->vendor_string);
		hvm_p->cpu_manuf=nvc_confirm_cpu_manufacturer(hvm_p->vendor_string);
		hvm_p->options.value=noir_query_enabled_features_in_system();
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
			case centaur_processor:
				goto vmx_subversion;
			default:
			{
				if(hvm_p->cpu_manuf==unknown_processor)
					nv_dprintf("You are using unknown manufacturer's processor!\n");
				else
					nv_dprintf("Your processor is manufactured by rare known vendor that NoirVisor currently failed to support!\n");
				return noir_unknown_processor;
			}
vmx_subversion:
			hvm_p->selected_core=use_vt_core;
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
			hvm_p->selected_core=use_svm_core;
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
			case centaur_processor:
				goto vmx_restoration;
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