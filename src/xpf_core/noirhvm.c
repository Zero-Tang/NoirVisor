/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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
#include <vt_intrin.h>
#include <svm_intrin.h>

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

u64 nvc_translate_address_64_routine(noir_paging64_general_entry_p pt,u64 gva,bool write,bool* fault,size_t level)
{
	const size_t psize_level=level*page_shift_diff64+page_shift;
	const size_t psize_exp=1ui64<<psize_level;
	const u64 index=(gva>>psize_level)&0x1ff;
	if(write>pt[index].write || pt[index].present==false)
	{
		// Page Fault!
		*fault=true;
		return 0;
	}
	if(pt[index].psize || level==0)
	{
		// Reaching final level.
		u64 base_mask=0xFFFFFFFFFFFFFFFF;
		u64 offset_mask=0xFFFFFFFFFFFFFFFF;
		u64 base=pt[index].value;
		u64 offset=gva;
		// Erase lower property bits for base.
		base_mask<<=psize_level;
		// Erase higher bits for offset.
		offset_mask&=~base_mask;
		// Erase higher property bits for base.
		base_mask&=0xFFFFFFFFFF000;
		// Calculate the base and mask.
		base&=base_mask;
		offset&=offset_mask;
		return base+offset;
	}
	// There are remaining levels to walk over.
	return nvc_translate_address_64_routine((noir_paging64_general_entry_p)page_mult(pt[index].base),gva,write,fault,level-1);
}

u64 nvc_translate_address_l4(u64 cr3,u64 gva,bool write,bool *fault)
{
	noir_paging64_general_entry_p root=(noir_paging64_general_entry_p)cr3;
	return nvc_translate_address_64_routine((noir_paging64_general_entry_p)cr3,gva,write,fault,3);
}

u64 nvc_translate_address_l3(u64 cr3,u32 gva,bool write,bool *fault)
{
	noir_paging64_general_entry_p root=(noir_paging64_general_entry_p)cr3;
	return nvc_translate_address_64_routine((noir_paging64_general_entry_p)cr3,(u64)gva,write,fault,2);
}

u64 nvc_translate_address_l2(u64 cr3,u32 gva,bool write,bool *fault)
{
	// For Non-PAE paging, there is no need to walk page table recursively.
	noir_paging32_general_entry_p pde=(noir_paging32_general_entry_p)cr3;
	noir_addr32_translator trans;
	u64 phys;
	trans.pointer=gva;
	phys=trans.offset;
	if(write>(bool)pde[trans.pde].pde.write || pde[trans.pde].pde.present==false)
		*fault=true;		// Page Fault!
	else if(pde[trans.pde].pde.psize)
	{
		// Large Page.
		phys|=trans.pte<<page_shift;
		phys|=pde[trans.pde].large_pde.base_lo<<(page_shift+page_shift_diff32);
		phys|=pde[trans.pde].large_pde.base_hi<<(page_shift+page_shift_diff32+page_shift_diff32);
	}
	else
	{
		noir_paging32_general_entry_p pte=(noir_paging32_general_entry_p)((u64)pde[trans.pde].pde.pte_base<<page_shift);
		phys|=pte[trans.pte].pte.base<<page_shift;
	}
	return phys;
}

#if !defined(_hv_type1)
noir_status nvc_set_guest_vcpu_options(noir_cvm_virtual_cpu_p vcpu,noir_cvm_vcpu_option_type option_type,u32 data)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		bool valid=true;
		switch(option_type)
		{
			case noir_cvm_guest_vcpu_options:
			{
				vcpu->vcpu_options.value=data;
				break;
			}
			case noir_cvm_exception_bitmap:
			{
				vcpu->exception_bitmap=data;
				break;
			}
			case noir_cvm_vcpu_priority:
			{
				vcpu->scheduling_priority=data;
				break;
			}
			case noir_cvm_msr_interception:
			{
				vcpu->msr_interceptions.value=data;
				break;
			}
			default:
			{
				valid=false;
				break;
			}
		}
		if(valid)
		{
			if(hvm_p->selected_core==use_svm_core)
			{
				st=noir_success;
				noir_svm_vmmcall(noir_cvm_set_vcpu_options,(ulong_ptr)vcpu);
			}
			else if(hvm_p->selected_core==use_vt_core)
			{
				st=noir_success;
				noir_vt_vmcall(noir_cvm_set_vcpu_options,(ulong_ptr)vcpu);
			}
			else
				st=noir_unknown_processor;
		}
		else
			st=noir_invalid_parameter;
	}
	return st;
}

void nvc_synchronize_vcpu_state(noir_cvm_virtual_cpu_p vcpu)
{
	if(hvm_p->selected_core==use_svm_core)
		noir_svm_vmmcall(noir_cvm_dump_vcpu_vmcb,(ulong_ptr)vcpu);
	else if(hvm_p->selected_core==use_vt_core)
		noir_vt_vmcall(noir_cvm_dump_vcpu_vmcb,(ulong_ptr)vcpu);
}

noir_status nvc_edit_vcpu_registers(noir_cvm_virtual_cpu_p vcpu,noir_cvm_register_type register_type,void* buffer,u32 buffer_size)
{
	noir_status st=noir_buffer_too_small;
	if(buffer_size>=noir_cvm_register_buffer_limit[register_type])
	{
		st=noir_success;
		// General Rule: If the state is to be cached, invalidate the cache.
		switch(register_type)
		{
			case noir_cvm_general_purpose_register:
			{
				noir_movsp(&vcpu->gpr,buffer,sizeof(void*)*2);
				vcpu->state_cache.gprvalid=0;
				break;
			}
			case noir_cvm_flags_register:
			{
				vcpu->rflags=*(u64*)buffer;
				vcpu->state_cache.gprvalid=0;
				break;
			}
			case noir_cvm_instruction_pointer:
			{
				vcpu->rip=*(u64*)buffer;
				vcpu->state_cache.gprvalid=0;
				break;
			}
			case noir_cvm_control_register:
			{
				u64* cr_list=(u64*)buffer;
				vcpu->crs.cr0=cr_list[0];
				vcpu->crs.cr3=cr_list[1];
				vcpu->crs.cr4=cr_list[2];
				vcpu->state_cache.cr_valid=0;
				break;
			}
			case noir_cvm_cr2_register:
			{
				vcpu->crs.cr2=*(u64*)buffer;
				vcpu->state_cache.cr2valid=0;
				break;
			}
			case noir_cvm_debug_register:
			{
				noir_copy_memory(&vcpu->drs,buffer,sizeof(u64)*4);
				break;
			}
			case noir_cvm_dr67_register:
			{
				noir_copy_memory(&vcpu->drs.dr6,buffer,sizeof(u64)*2);
				vcpu->state_cache.dr_valid=0;
				break;
			}
			case noir_cvm_segment_register:
			{
				segment_register_p sr_list=(segment_register_p)buffer;
				vcpu->seg.es=sr_list[0];
				vcpu->seg.cs=sr_list[1];
				vcpu->seg.ss=sr_list[2];
				vcpu->seg.ds=sr_list[3];
				vcpu->state_cache.sr_valid=0;
				break;
			}
			case noir_cvm_fgseg_register:
			{
				segment_register_p sr_list=(segment_register_p)buffer;
				vcpu->seg.fs=sr_list[0];
				vcpu->seg.gs=sr_list[1];
				// Compatibility purpose for Kernel-GS-Base settings.
				if(buffer_size-noir_cvm_register_buffer_limit[noir_cvm_fgseg_register]>=sizeof(u64))
					vcpu->msrs.gsswap=*(u64p)((ulong_ptr)buffer+32);
				vcpu->state_cache.fg_valid=0;
				break;
			}
			case noir_cvm_descriptor_table:
			{
				segment_register_p dt_list=(segment_register_p)buffer;
				vcpu->seg.gdtr.limit=dt_list[0].limit;
				vcpu->seg.gdtr.base=dt_list[0].base;
				vcpu->seg.idtr.limit=dt_list[1].limit;
				vcpu->seg.idtr.base=dt_list[1].base;
				vcpu->state_cache.dt_valid=0;
				break;
			}
			case noir_cvm_ldtr_task_register:
			{
				segment_register_p sr_list=(segment_register_p)buffer;
				vcpu->seg.tr=sr_list[0];
				vcpu->seg.ldtr=sr_list[1];
				vcpu->state_cache.lt_valid=0;
				break;
			}
			case noir_cvm_syscall_msr_register:
			{
				noir_copy_memory(&vcpu->msrs.star,buffer,sizeof(u64*)*4);
				vcpu->state_cache.sc_valid=0;
				break;
			}
			case noir_cvm_sysenter_msr_register:
			{
				noir_copy_memory(&vcpu->msrs.sysenter_cs,buffer,sizeof(u64*)*3);
				vcpu->state_cache.se_valid=0;
				break;
			}
			case noir_cvm_cr8_register:
			{
				vcpu->crs.cr8=*(u64*)buffer;
				vcpu->state_cache.tp_valid=0;
				break;
			}
			case noir_cvm_fxstate:
			{
				noir_copy_memory(vcpu->xsave_area,buffer,sizeof(noir_fx_state));
				break;
			}
			case noir_cvm_xsave_area:
			{
				if(buffer_size>=hvm_p->xfeat.supported_size_max)
					noir_copy_memory(vcpu->xsave_area,buffer,hvm_p->xfeat.supported_size_max);
				else
					st=noir_buffer_too_small;
				break;
			}
			case noir_cvm_xcr0_register:
			{
				vcpu->xcrs.xcr0=*(u64*)buffer;
				break;
			}
			case noir_cvm_efer_register:
			{
				vcpu->msrs.efer=*(u64*)buffer;
				vcpu->state_cache.ef_valid=0;
				break;
			}
			case noir_cvm_pat_register:
			{
				vcpu->msrs.pat=*(u64*)buffer;
				vcpu->state_cache.pa_valid=0;
				break;
			}
			case noir_cvm_last_branch_record_register:
			{
				noir_copy_memory(&vcpu->msrs.debug_ctrl,buffer,sizeof(u64)*5);
				vcpu->state_cache.lb_valid=0;
				break;
			}
			default:
			{
				st=noir_invalid_parameter;
				break;
			}
		}
	}
	return st;
}

noir_status nvc_view_vcpu_registers(noir_cvm_virtual_cpu_p vcpu,noir_cvm_register_type register_type,void* buffer,u32 buffer_size)
{
	noir_status st=noir_invalid_parameter;
	if(buffer_size>=noir_cvm_register_buffer_limit[register_type])
	{
		st=noir_success;
		switch(register_type)
		{
			// If the state is saved in VMCB, check if the state cache is invalid and synchronized.
			// Perform synchronization if the state cache is both valid and unsynchronized.
			case noir_cvm_general_purpose_register:
			{
				noir_movsp(buffer,&vcpu->gpr,sizeof(void*)*2);
				break;
			}
			case noir_cvm_flags_register:
			{
				*(u64*)buffer=vcpu->rflags;
				break;
			}
			case noir_cvm_instruction_pointer:
			{
				*(u64*)buffer=vcpu->rip;
				break;
			}
			case noir_cvm_control_register:
			{
				u64* cr_list=(u64*)buffer;
				if(vcpu->state_cache.cr_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				cr_list[0]=vcpu->crs.cr0;
				cr_list[1]=vcpu->crs.cr3;
				cr_list[2]=vcpu->crs.cr4;
				break;
			}
			case noir_cvm_cr2_register:
			{
				if(vcpu->state_cache.cr2valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				*(u64*)buffer=vcpu->crs.cr2;
				break;
			}
			case noir_cvm_debug_register:
			{
				noir_copy_memory(buffer,&vcpu->drs,sizeof(u64)*4);
				break;
			}
			case noir_cvm_dr67_register:
			{
				if(vcpu->state_cache.dr_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				noir_copy_memory(buffer,&vcpu->drs.dr6,sizeof(u64)*2);
				break;
			}
			case noir_cvm_segment_register:
			{
				segment_register_p sr_list=(segment_register_p)buffer;
				if(vcpu->state_cache.sr_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				sr_list[0]=vcpu->seg.es;
				sr_list[1]=vcpu->seg.cs;
				sr_list[2]=vcpu->seg.ss;
				sr_list[3]=vcpu->seg.ds;
				break;
			}
			case noir_cvm_fgseg_register:
			{
				segment_register_p sr_list=(segment_register_p)buffer;
				if(vcpu->state_cache.fg_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				sr_list[0]=vcpu->seg.fs;
				sr_list[1]=vcpu->seg.gs;
				*(u64p)((ulong_ptr)buffer+32)=vcpu->msrs.gsswap;
				break;
			}
			case noir_cvm_descriptor_table:
			{
				segment_register_p dt_list=(segment_register_p)buffer;
				if(vcpu->state_cache.dt_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				dt_list[0].limit=vcpu->seg.gdtr.limit;
				dt_list[0].base=vcpu->seg.gdtr.base;
				dt_list[1].limit=vcpu->seg.idtr.limit;
				dt_list[1].base=vcpu->seg.idtr.base;
				break;
			}
			case noir_cvm_ldtr_task_register:
			{
				segment_register_p sr_list=(segment_register_p)buffer;
				if(vcpu->state_cache.lt_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				sr_list[0]=vcpu->seg.tr;
				sr_list[1]=vcpu->seg.ldtr;
				break;
			}
			case noir_cvm_syscall_msr_register:
			{
				if(vcpu->state_cache.sc_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				noir_copy_memory(buffer,&vcpu->msrs.star,sizeof(u64*)*4);
				break;
			}
			case noir_cvm_sysenter_msr_register:
			{
				if(vcpu->state_cache.se_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				noir_copy_memory(buffer,&vcpu->msrs.sysenter_cs,sizeof(u64*)*3);
				break;
			}
			case noir_cvm_cr8_register:
			{
				if(vcpu->state_cache.tp_valid && !vcpu->state_cache.synchronized)
					nvc_synchronize_vcpu_state(vcpu);
				*(u64*)buffer=vcpu->crs.cr8;
				break;
			}
			case noir_cvm_fxstate:
			{
				noir_copy_memory(buffer,vcpu->xsave_area,sizeof(noir_fx_state));
				break;
			}
			case noir_cvm_xsave_area:
			{
				if(buffer_size>=hvm_p->xfeat.supported_size_max)
					noir_copy_memory(buffer,vcpu->xsave_area,hvm_p->xfeat.supported_size_max);
				else
					st=noir_buffer_too_small;
				break;
			}
			case noir_cvm_xcr0_register:
			{
				*(u64*)buffer=vcpu->xcrs.xcr0;
				break;
			}
			case noir_cvm_efer_register:	
			{
				*(u64*)buffer=vcpu->msrs.efer;
				break;
			}
			case noir_cvm_pat_register:
			{
				*(u64*)buffer=vcpu->msrs.pat;
				break;
			}
			default:
			{
				st=noir_invalid_parameter;
				break;
			}
		}
	}
	return st;
}

noir_status nvc_set_event_injection(noir_cvm_virtual_cpu_p vcpu,noir_cvm_event_injection injected_event)
{
	vcpu->injected_event=injected_event;
	return noir_success;
}

noir_status nvc_query_vcpu_statistics(noir_cvm_virtual_cpu_p vcpu,void* buffer,u32 buffer_size)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		const u32 copy_size=buffer_size<sizeof(noir_cvm_vcpu_statistics)?buffer_size:sizeof(noir_cvm_vcpu_statistics);
		if(copy_size<sizeof(noir_cvm_interception_counter))
			st=noir_buffer_too_small;
		else
		{
			noir_copy_memory(buffer,&vcpu->statistics,copy_size);
			noir_stosb(buffer,0,sizeof(noir_cvm_interception_counter));
			st=noir_success;
		}
	}
	return st;
}

bool nvc_validate_vcpu_state(noir_cvm_virtual_cpu_p vcpu)
{
	// Check Extended CRs.
	// Bit 0 of XCR0 (FPU Enabled) must be set.
	if(!noir_bt(&vcpu->xcrs.xcr0,0))
	{
		vcpu->exit_context.intercept_code=cv_invalid_state;
		return false;
	}
	// Bit 1 of XCR0 (SSE Enabled) cannot be reset if Bit 2 of XCR0 (AVX Enabled) is set.
	if(!noir_bt(&vcpu->xcrs.xcr0,1) && noir_bt(&vcpu->xcrs.xcr0,2))
	{
		vcpu->exit_context.intercept_code=cv_invalid_state;
		return false;
	}
	// Reserved bits of XCR0 must be reset.
	if((vcpu->xcrs.xcr0&hvm_p->xfeat.support_mask.value)!=vcpu->xcrs.xcr0)
	{
		vcpu->exit_context.intercept_code=cv_invalid_state;
		return false;
	}
	return true;
}

noir_status nvc_run_vcpu(noir_cvm_virtual_cpu_p vcpu,void* exit_context)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		// Some processor state is not checked and loaded by Intel VT-x/AMD-V. (e.g: x87 FPU State)
		// Check their consistency manually.
		bool valid_state=nvc_validate_vcpu_state(vcpu);
		st=noir_success;
		if(valid_state)
		{
			while(st==noir_success)
			{
				if(hvm_p->selected_core==use_svm_core)
					st=nvc_svmc_run_vcpu(vcpu);
				else if(hvm_p->selected_core==use_vt_core)
					st=nvc_vtc_run_vcpu(vcpu);
				else
					st=noir_unknown_processor;
				if(vcpu->exit_context.intercept_code!=cv_scheduler_exit)break;
			}
		}
		if(st==noir_success && !vcpu->vcpu_options.use_tunnel)noir_copy_memory(exit_context,&vcpu->exit_context,sizeof(noir_cvm_exit_context));
	}
	return st;
}

noir_status nvc_rescind_vcpu(noir_cvm_virtual_cpu_p vcpu)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		st=noir_success;
		if(hvm_p->selected_core==use_vt_core)
			st=nvc_vtc_rescind_vcpu(vcpu);
		else if(hvm_p->selected_core==use_svm_core)
			st=nvc_svmc_rescind_vcpu(vcpu);
		else
			st=noir_unknown_processor;
	}
	return st;
}

noir_cvm_virtual_cpu_p nvc_reference_vcpu(noir_cvm_virtual_machine_p vm,u32 vcpu_id)
{
	noir_cvm_virtual_cpu_p vcpu=null;
	if(hvm_p)
	{
		noir_acquire_reslock_shared(vm->vcpu_list_lock);
		if(hvm_p->selected_core==use_svm_core)
			vcpu=nvc_svmc_reference_vcpu(vm,vcpu_id);
		else if(hvm_p->selected_core==use_vt_core)
			vcpu=nvc_vtc_reference_vcpu(vm,vcpu_id);
		noir_release_reslock(vm->vcpu_list_lock);
	}
	return vcpu;
}

noir_status nvc_release_vcpu(noir_cvm_virtual_cpu_p vcpu)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		st=noir_success;
		if(vcpu->ref_count)
			nv_dprintf("Deleting vCPU 0x%p with uncleared reference (%u)!",vcpu,vcpu->ref_count);
		if(hvm_p->selected_core==use_vt_core)
			nvc_vtc_release_vcpu(vcpu);
		else if(hvm_p->selected_core==use_svm_core)
			nvc_svmc_release_vcpu(vcpu);
		else
			st=noir_unknown_processor;
	}
	return st;
}

noir_status nvc_create_vcpu(noir_cvm_virtual_machine_p vm,noir_cvm_virtual_cpu_p* vcpu,u32 vcpu_id)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		st=noir_success;
		if(hvm_p->selected_core==use_vt_core)
			st=nvc_vtc_create_vcpu(vcpu,vm,vcpu_id);
		else if(hvm_p->selected_core==use_svm_core)
			st=nvc_svmc_create_vcpu(vcpu,vm,vcpu_id);
		else
			st=noir_unknown_processor;
		if(st==noir_success)
		{
			(*vcpu)->ref_count=1;
			// Initialize some registers...
			(*vcpu)->xcrs.xcr0=1;	// HAXM does not know XCR0.
		}
	}
	return st;
}

noir_status nvc_ref_vcpu(noir_cvm_virtual_cpu_p vcpu)
{
	noir_locked_inc(&vcpu->ref_count);
	return noir_success;
}

noir_status nvc_deref_vcpu(noir_cvm_virtual_cpu_p vcpu)
{
	u32 prev_refcnt=noir_locked_dec(&vcpu->ref_count);
	if(prev_refcnt!=1)
		return noir_success;
	else
	{
		nvc_release_vcpu(vcpu);
		return noir_dereference_destroying;
	}
}

noir_status nvc_operate_guest_memory(noir_cvm_virtual_cpu_p vcpu,u64 guest_address,void* buffer,u32 size,bool write,bool virtual_address)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		noir_cvm_gmem_op_context context;
		context.vcpu=vcpu;
		context.guest_address=guest_address;
		context.hva=buffer;
		context.size=(u64)size;
		context.write_op=write;
		context.use_va=virtual_address;
		context.reserved=0;
		context.status=noir_unknown_processor;
		if(hvm_p->selected_core==use_vt_core)
			noir_vt_vmcall(noir_cvm_guest_memory_operation,(ulong_ptr)&context);
		else if(hvm_p->selected_core==use_svm_core)
			noir_svm_vmmcall(noir_cvm_guest_memory_operation,(ulong_ptr)&context);
		st=context.status;
	}
	return st;
}

void nvc_release_lockers(noir_cvm_virtual_machine_p virtual_machine)
{
	// NoirVisor must gain exclusion of VM before releasing the lockers!
	noir_cvm_lockers_list_p cur=virtual_machine->locker_head;
	while(cur)
	{
		noir_cvm_lockers_list_p next=cur->next;
		for(u32 i=0;i<noir_cvm_lockers_per_array;i++)
			if(cur->lockers[i])
				noir_unlock_pages(cur->lockers[i]);
		noir_free_nonpg_memory(cur);
		cur=next;
	}
}

// Warning: this function erases the slot!
// Unlock the page before releasing the slot.
void nvc_free_locker_slot(void** locker_slot)
{
	noir_cvm_lockers_list_p locker_list=(noir_cvm_lockers_list_p)page_4kb_base((ulong_ptr)locker_slot);
	if(locker_list)
	{
#if defined(_amd64)
		u32 index=(u32)page_4kb_offset((u64)locker_slot)>>3;
#else
		u32 index=(u32)page_4kb_offset((u32)locker_slot)>>2;
#endif
		noir_reset_bitmap(locker_list,index);
		*locker_slot=null;
	}
}

void** nvc_alloc_locker_slot(noir_cvm_virtual_machine_p virtual_machine)
{
	for(noir_cvm_lockers_list_p cur=virtual_machine->locker_head;cur;cur=cur->next)
	{
		u32 index=noir_find_clear_bit(cur->bitmap,noir_cvm_lockers_per_array);
		if(index!=0xffffffff)
		{
			noir_set_bitmap(cur->bitmap,index);
			return &cur->lockers[index];
		}
	}
	// At this point, all lockers are allocated.
	virtual_machine->locker_tail->next=noir_alloc_nonpg_memory(page_size);
	if(virtual_machine->locker_tail->next)
	{
		noir_cvm_lockers_list_p locker_list=virtual_machine->locker_tail->next;
		// Return the first entry.
		locker_list->bitmap[0]=1;
		return locker_list->lockers;
	}
	// Insufficient system resources.
	return null;
}

noir_status nvc_register_memblock(noir_cvm_virtual_machine_p vm,u64 start,u64 size)
{
	u32 index;
	if(noir_bsf64(&index,~vm->memblock_bitmap))
	{
		noir_bts64(&vm->memblock_bitmap,index);
		vm->memblock_registrations[index].start=start;
		vm->memblock_registrations[index].size=size;
		return noir_success;
	}
	return noir_insufficient_resources;
}

noir_status nvc_set_mapping(noir_cvm_virtual_machine_p virtual_machine,noir_cvm_address_mapping_p mapping_info)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		// Exclusive acquirement is unnecessary.
		noir_acquire_reslock_shared(virtual_machine->vcpu_list_lock);
		if(mapping_info->attributes.present || mapping_info->attributes.write || mapping_info->attributes.execute)
		{
			void** locker_slot=nvc_alloc_locker_slot(virtual_machine);
			u64p phys_array=noir_alloc_nonpg_memory(mapping_info->pages<<3);
			if(locker_slot && phys_array)
			{
				*locker_slot=noir_lock_pages((void*)mapping_info->hva,page_4kb_mult(mapping_info->pages),phys_array);
				if(!*locker_slot)goto alloc_failure;
				if(hvm_p->selected_core==use_vt_core)
					st=nvc_vtc_set_mapping(virtual_machine,mapping_info);
				else if(hvm_p->selected_core==use_svm_core)
					st=nvc_svmc_set_mapping(virtual_machine,mapping_info,phys_array);
				else
				{
					st=noir_unknown_processor;
					noir_unlock_pages(*locker_slot);
					nvc_free_locker_slot(locker_slot);
				}
				noir_free_nonpg_memory(phys_array);
			}
			else
			{
alloc_failure:
				if(locker_slot)nvc_free_locker_slot(locker_slot);
				if(phys_array)noir_free_nonpg_memory(phys_array);
				st=noir_insufficient_resources;
			}
		}
		else
		{
			if(hvm_p->selected_core==use_vt_core)
				st=nvc_vtc_set_mapping(virtual_machine,mapping_info);
			else if(hvm_p->selected_core==use_svm_core)
				st=nvc_svmc_set_unmapping(virtual_machine,mapping_info->gpa,mapping_info->pages);
			else
				st=noir_unknown_processor;
		}
		nvc_register_memblock(virtual_machine,mapping_info->gpa,page_mult(mapping_info->pages));
		noir_release_reslock(virtual_machine->vcpu_list_lock);
	}
	return st;
}

noir_status nvc_query_gpa_accessing_bitmap(noir_cvm_virtual_machine_p virtual_machine,u64 gpa_start,u32 page_count,void* bitmap,u32 bitmap_size)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		st=noir_invalid_parameter;
		noir_acquire_reslock_shared(virtual_machine->vcpu_list_lock);
		if(hvm_p->selected_core==use_vt_core)
			st=noir_not_implemented;
		else if(hvm_p->selected_core==use_svm_core)
			st=nvc_svmc_query_gpa_accessing_bitmap(virtual_machine,gpa_start,page_count,bitmap,bitmap_size);
		else
			st=noir_unknown_processor;
		noir_release_reslock(virtual_machine->vcpu_list_lock);
	}
	return st;
}

noir_status nvc_clear_gpa_accessing_bits(noir_cvm_virtual_machine_p virtual_machine,u64 gpa_start,u32 page_count)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		// Preventing any vCPUs to be launched is good enough. Exclusive acquirement is unnecessary.
		noir_acquire_reslock_shared(virtual_machine->vcpu_list_lock);
		if(hvm_p->selected_core==use_vt_core)
			st=noir_not_implemented;
		else if(hvm_p->selected_core==use_svm_core)
			st=nvc_svmc_clear_gpa_accessing_bits(virtual_machine,gpa_start,page_count);
		else
			st=noir_unknown_processor;
		noir_release_reslock(virtual_machine->vcpu_list_lock);
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
		if(vm->ref_count)nv_dprintf("Deleting VM 0x%p with uncleared reference (%u)!\n",vm,vm->ref_count);
		noir_remove_list_entry(&vm->active_vm_list);
		// Release the VM structure.
		if(hvm_p->selected_core==use_vt_core)
			nvc_vtc_release_vm(vm);
		else if(hvm_p->selected_core==use_svm_core)
			nvc_svmc_release_vm(vm);
		else
			st=noir_unknown_processor;
		// Remove the vCPU list Resource Lock.
		if(vm->vcpu_list_lock)noir_finalize_reslock(vm->vcpu_list_lock);
		noir_release_reslock(noir_vm_list_lock);
	}
	return st;
}

noir_status nvc_create_vm_ex(noir_cvm_virtual_machine_p* vm,u32 process_id,noir_cvm_vm_properties properties)
{
	noir_status st=noir_hypervision_absent;
	if(hvm_p)
	{
		if(hvm_p->selected_core==use_vt_core)
		{
			if(properties.value)
				st=noir_not_implemented;
			else
				st=nvc_vtc_create_vm(vm);
		}
		else if(hvm_p->selected_core==use_svm_core)
		{
			if(properties.value)
				st=noir_not_implemented;
			else
				st=nvc_svmc_create_vm(vm);
		}
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
			(*vm)->properties=properties;
			(*vm)->pid=process_id;
			(*vm)->ref_count=1;
		}
	}
	return st;
}

noir_status nvc_create_vm(noir_cvm_virtual_machine_p* vm,u32 process_id)
{
	noir_cvm_vm_properties vmprop={0};
	return nvc_create_vm_ex(vm,process_id,vmprop);
}

noir_status nvc_deref_vm(noir_cvm_virtual_machine_p vm)
{
	u32 prev_refcnt=noir_locked_dec(&vm->ref_count);
	if(prev_refcnt!=1)
		return noir_success;
	else
	{
		nvc_release_vm(vm);
		return noir_dereference_destroying;
	}
}

noir_status nvc_ref_vm(noir_cvm_virtual_machine_p vm)
{
	noir_locked_inc(&vm->ref_count);
	return noir_success;
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
		else if(hvm_p->selected_core==use_vt_core)
			return nvc_vtc_get_vm_asid(vm);
		else
			return noir_unknown_processor;
	}
	// Zero indicates failure.
	return 0;
}

noir_status nvc_query_hypervisor_status(u64 status_type,void* result)
{
	noir_status st=noir_invalid_parameter;
	if(result)
	{
		switch(status_type)
		{
			case noir_cvm_hvstatus_presence:
			{
				*(u64p)result=hvm_p==null?0:1;
				st=noir_success;
				break;
			}
			case noir_cvm_hvstatus_capabilities:
			{
				if(hvm_p==null)
					st=noir_hypervision_absent;
				else
				{
					*(u64p)result=hvm_p->cvm_cap.value;
					st=noir_success;
				}
				break;
			}
			case noir_cvm_hvstatus_hypercall_instruction:
			{
				if(hvm_p)
				{
					u8p output=(u8p)result;
					if(hvm_p->selected_core==use_vt_core)
					{
						output[0]=3;
						output[1]=0x0F;
						output[2]=0x01;
						output[3]=0xC1;
						st=noir_success;
					}
					else if(hvm_p->selected_core==use_svm_core)
					{
						output[0]=3;
						output[1]=0x0F;
						output[2]=0x01;
						output[3]=0xD9;
						st=noir_success;
					}
					else
						st=noir_unknown_processor;
				}
				else
					st=noir_hypervision_absent;
				break;
			}
		}
	}
	return st;
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