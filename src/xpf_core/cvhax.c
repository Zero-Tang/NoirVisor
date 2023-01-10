/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the NoirVisor CVM API Core Wrapper for Intel HAXM.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/cvhax.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <nv_intrin.h>

noir_status nvc_hax_get_version_info(void* buffer,u32 size,u32p return_size)
{
	noir_hax_module_version_p ver_info=(noir_hax_module_version_p)buffer;
	if(return_size)*return_size=sizeof(noir_hax_module_version);
	if(size<sizeof(noir_hax_module_version))
		return noir_buffer_too_small;
	ver_info->current_version=noir_hax_current_version;
	ver_info->compat_version=noir_hax_compat_version;
	return noir_success;
}

noir_status nvc_hax_get_capability(void* buffer,u32 size,u32p return_size)
{
	noir_hax_capability_info_p cap_info=(noir_hax_capability_info_p)buffer;
	if(return_size)*return_size=sizeof(noir_hax_capability_info);
	if(size<sizeof(noir_hax_capability_info))
		return noir_buffer_too_small;
	if(hvm_p)
	{
		cap_info->status=noir_hax_cap_status_working;
		cap_info->info=noir_hax_cap_ept;
		cap_info->info|=noir_hax_cap_ug;
		cap_info->info|=noir_hax_cap_64bit_ramblock;
		cap_info->info|=noir_hax_cap_64bit_setram;
	}
	else
	{
		cap_info->status=0;
		cap_info->info=0;
		cap_info->ref_count=0;
		cap_info->mem_quota=0;
	}
	return noir_success;
}

noir_status nvc_hax_set_memlimit(void* buffer,u32 size,u32p return_size)
{
	// Current implementation of NoirVisor CVM does not design a memory limit.
	// Ignore any memlimit setting requests.
	if(return_size)*return_size=0;
	return noir_success;
}

noir_status nvc_hax_set_mapping(noir_cvm_virtual_machine_p vm,noir_hax_set_ram_info_p ram_info)
{
	noir_cvm_address_mapping map_info={0};
	map_info.gpa=ram_info->gpa;
	map_info.hva=ram_info->hva;
	map_info.pages=page_count(ram_info->bytes);
	map_info.attributes.value=0;
	if(!ram_info->flags.invalid)
	{
		map_info.attributes.present=true;
		map_info.attributes.write=ram_info->flags.read_only?false:true;
		map_info.attributes.execute=true;
		map_info.attributes.user=true;
		map_info.attributes.caching=noir_cvm_memory_wb;
	}
	return nvc_set_mapping(vm,&map_info);
}

noir_status nvc_hax_set_tunnel(noir_cvm_virtual_cpu_p vcpu,void* tunnel,void* iobuff)
{
	vcpu->tunnel=tunnel;
	vcpu->iobuff=iobuff;
	vcpu->vcpu_options.use_tunnel=true;
	vcpu->vcpu_options.tunnel_format=noir_cvm_tunnel_format_haxm;
	return noir_success;
}

void static nvc_hax_convert_from_hax_segment(segment_register_p seg,noir_hax_segment_p hax)
{
	seg->selector=hax->selector;
	seg->attrib=(u16)hax->access_rights;
	seg->limit=hax->limit;
	seg->base=hax->base;
}

void static nvc_hax_convert_to_hax_segment(noir_hax_segment_p hax,segment_register_p seg)
{
	hax->selector=seg->selector;
	hax->dummy=0;
	hax->limit=seg->limit;
	hax->base=seg->base;
	hax->access_rights=(u32)seg->attrib;
	hax->pad=0;
}

// Intel HAXM's vCPU state management is very cache-unfriendly.
noir_status nvc_hax_set_vcpu_registers(noir_cvm_virtual_cpu_p vcpu,void* buffer,u32 size,u32p return_size)
{
	if(return_size)*return_size=0;
	if(size<sizeof(noir_hax_vcpu_state))
		return noir_buffer_too_small;
	if(hvm_p)
	{
		noir_hax_vcpu_state_p state=(noir_hax_vcpu_state_p)buffer;
		// General-Purpose Registers...
		vcpu->gpr=state->gpr;
		vcpu->state_cache.gprvalid=false;
		// Segment Registers...
		nvc_hax_convert_from_hax_segment(&vcpu->seg.cs,&state->cs);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.ds,&state->ds);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.es,&state->es);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.fs,&state->fs);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.gs,&state->gs);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.ss,&state->ss);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.tr,&state->tr);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.ldtr,&state->ldtr);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.idtr,&state->idtr);
		nvc_hax_convert_from_hax_segment(&vcpu->seg.gdtr,&state->gdtr);
		vcpu->state_cache.sr_valid=false;
		vcpu->state_cache.fg_valid=false;
		vcpu->state_cache.dt_valid=false;
		vcpu->state_cache.lt_valid=false;
		// Control Registers...
		vcpu->crs.cr0=state->cr0;
		vcpu->crs.cr2=state->cr2;
		vcpu->crs.cr3=state->cr3;
		vcpu->crs.cr4=state->cr4;
		vcpu->state_cache.cr_valid=false;
		vcpu->state_cache.cr2valid=false;
		// Debug Registers...
		vcpu->drs.dr0=state->dr0;
		vcpu->drs.dr1=state->dr1;
		vcpu->drs.dr2=state->dr2;
		vcpu->drs.dr3=state->dr3;
		vcpu->drs.dr6=state->dr6;
		vcpu->drs.dr7=state->dr7;
		vcpu->state_cache.dr_valid=false;
		// Model-Specific Registers...
		vcpu->msrs.efer=(u64)state->efer;
		vcpu->state_cache.ef_valid=false;
		vcpu->msrs.sysenter_cs=(u64)state->sysenter_cs;
		vcpu->msrs.sysenter_esp=state->sysenter_esp;
		vcpu->msrs.sysenter_eip=state->sysenter_eip;
		vcpu->state_cache.se_valid=false;
		return noir_success;
	}
	return noir_hypervision_absent;
}

noir_status nvc_hax_get_vcpu_registers(noir_cvm_virtual_cpu_p vcpu,void* buffer,u32 size,u32p return_size)
{
	if(size<sizeof(noir_hax_vcpu_state))
	{
		if(return_size)*return_size=0;
		return noir_buffer_too_small;
	}
	if(hvm_p)
	{
		noir_hax_vcpu_state_p state=(noir_hax_vcpu_state_p)buffer;
		// Make sure the vCPU state is synchronized from VMCB.
		nvc_synchronize_vcpu_state(vcpu);
		// General-Purpose Registers
		state->gpr=vcpu->gpr;
		// Segment Registers...
		nvc_hax_convert_to_hax_segment(&state->cs,&vcpu->seg.cs);
		nvc_hax_convert_to_hax_segment(&state->ds,&vcpu->seg.ds);
		nvc_hax_convert_to_hax_segment(&state->es,&vcpu->seg.es);
		nvc_hax_convert_to_hax_segment(&state->fs,&vcpu->seg.fs);
		nvc_hax_convert_to_hax_segment(&state->gs,&vcpu->seg.cs);
		nvc_hax_convert_to_hax_segment(&state->ss,&vcpu->seg.ss);
		nvc_hax_convert_to_hax_segment(&state->tr,&vcpu->seg.tr);
		nvc_hax_convert_to_hax_segment(&state->ldtr,&vcpu->seg.ldtr);
		nvc_hax_convert_to_hax_segment(&state->idtr,&vcpu->seg.idtr);
		nvc_hax_convert_to_hax_segment(&state->gdtr,&vcpu->seg.gdtr);
		// Control Registers...
		state->cr0=vcpu->crs.cr0;
		state->cr2=vcpu->crs.cr2;
		state->cr3=vcpu->crs.cr3;
		state->cr4=vcpu->crs.cr4;
		// Debug Registers...
		state->dr0=vcpu->drs.dr0;
		state->dr1=vcpu->drs.dr1;
		state->dr2=vcpu->drs.dr2;
		state->dr3=vcpu->drs.dr3;
		state->dr6=vcpu->drs.dr6;
		state->dr7=vcpu->drs.dr7;
		// Model-Specific Registers...
		state->efer=(u32)vcpu->msrs.efer;
		state->sysenter_cs=(u32)vcpu->msrs.sysenter_cs;
		state->sysenter_esp=vcpu->msrs.sysenter_esp;
		state->sysenter_eip=vcpu->msrs.sysenter_eip;
		if(return_size)*return_size=sizeof(noir_hax_vcpu_state);
		return noir_success;
	}
	return noir_hypervision_absent;
}

noir_status nvc_hax_get_fpu_state(noir_cvm_virtual_cpu_p vcpu,void* buffer,u32 size,u32p return_size)
{
	noir_status st=noir_buffer_too_small;
	st=nvc_view_vcpu_registers(vcpu,noir_cvm_fxstate,buffer,size);
	if(st==noir_success)
		if(return_size)
			*return_size=sizeof(noir_fx_state);
	return st;
}

noir_status nvc_hax_set_fpu_state(noir_cvm_virtual_cpu_p vcpu,void* buffer,u32 size,u32p return_size)
{
	noir_status st=nvc_edit_vcpu_registers(vcpu,noir_cvm_fxstate,buffer,size);
	if(st==noir_success)
		if(return_size)
			*return_size=0;
	return st;
}

noir_status nvc_hax_set_msrs(noir_cvm_virtual_cpu_p vcpu,void* buffer,u32 size,u32p return_size)
{
	noir_status st=noir_buffer_too_small;
	if(sizeof(noir_hax_msr_data)<=size)
	{
		st=noir_hypervision_absent;
		if(hvm_p)
		{
			noir_hax_msr_data_p msr_info=(noir_hax_msr_data_p)buffer;
			if(msr_info->nr_msr>=hax_max_msr_array)
				st=noir_invalid_parameter;
			else
			{
				for(u16 i=0;i<msr_info->nr_msr;i++)
				{
					switch(msr_info->entries[i].entry)
					{
						case hax_msr_tsc:
						{
							vcpu->tsc_offset=msr_info->entries[i].value-noir_rdtsc();
							vcpu->state_cache.ts_valid=false;
							break;
						}
						case hax_msr_sysenter_cs:
						{
							vcpu->msrs.sysenter_cs=msr_info->entries[i].value;
							vcpu->state_cache.se_valid=false;
							break;
						}
						case hax_msr_sysenter_esp:
						{
							vcpu->msrs.sysenter_esp=msr_info->entries[i].value;
							vcpu->state_cache.se_valid=false;
							break;
						}
						case hax_msr_sysenter_eip:
						{
							vcpu->msrs.sysenter_eip=msr_info->entries[i].value;
							vcpu->state_cache.se_valid=false;
							break;
						}
						case hax_msr_efer:
						{
							vcpu->msrs.efer=msr_info->entries[i].value;
							vcpu->state_cache.ef_valid=false;
							break;
						}
						case hax_msr_star:
						{
							vcpu->msrs.star=msr_info->entries[i].value;
							vcpu->state_cache.sc_valid=false;
							break;
						}
						case hax_msr_lstar:
						{
							vcpu->msrs.lstar=msr_info->entries[i].value;
							vcpu->state_cache.sc_valid=false;
							break;
						}
						case hax_msr_cstar:
						{
							vcpu->msrs.cstar=msr_info->entries[i].value;
							vcpu->state_cache.sc_valid=false;
							break;
						}
						case hax_msr_fmask:
						{
							vcpu->msrs.sfmask=msr_info->entries[i].value;
							vcpu->state_cache.sc_valid=false;
							break;
						}
						case hax_msr_kernel_gs_base:
						{
							vcpu->msrs.gsswap=msr_info->entries[i].value;
							vcpu->state_cache.fg_valid=false;
							break;
						}
						default:
						{
							nv_dprintf("[HAXM] Setting unknown MSR (0x%llX)!",msr_info->entries[i].entry);
							return noir_invalid_parameter;
						}
					}
				}
				st=noir_success;
			}
		}
	}
	return st;
}

noir_status nvc_hax_get_msrs(noir_cvm_virtual_cpu_p vcpu,void* buffer,u32 size,u32p return_size)
{
	noir_status st=noir_buffer_too_small;
	if(sizeof(noir_hax_msr_data)<=size)
	{
		st=noir_hypervision_absent;
		if(hvm_p)
		{
			noir_hax_msr_data_p msr_info=(noir_hax_msr_data_p)buffer;
			if(msr_info->nr_msr>=hax_max_msr_array)
				st=noir_invalid_parameter;
			else
			{
				nvc_synchronize_vcpu_state(vcpu);
				for(u16 i=0;i<msr_info->nr_msr;i++)
				{
					switch(msr_info->entries[i].entry)
					{
						case hax_msr_tsc:
						{
							msr_info->entries[i].value=vcpu->tsc_offset+noir_rdtsc();
							break;
						}
						case hax_msr_sysenter_cs:
						{
							msr_info->entries[i].value=vcpu->msrs.sysenter_cs;
							break;
						}
						case hax_msr_sysenter_esp:
						{
							msr_info->entries[i].value=vcpu->msrs.sysenter_esp;
							break;
						}
						case hax_msr_sysenter_eip:
						{
							msr_info->entries[i].value=vcpu->msrs.sysenter_eip;
							break;
						}
						case hax_msr_efer:
						{
							msr_info->entries[i].value=vcpu->msrs.efer;
							break;
						}
						case hax_msr_star:
						{
							msr_info->entries[i].value=vcpu->msrs.star;
							break;
						}
						case hax_msr_lstar:
						{
							msr_info->entries[i].value=vcpu->msrs.lstar;
							break;
						}
						case hax_msr_cstar:
						{
							msr_info->entries[i].value=vcpu->msrs.cstar;
							break;
						}
						case hax_msr_fmask:
						{
							msr_info->entries[i].value=vcpu->msrs.sfmask;
							break;
						}
						case hax_msr_kernel_gs_base:
						{
							msr_info->entries[i].value=vcpu->msrs.gsswap;
							break;
						}
						default:
						{
							nv_dprintf("[HAXM] Getting unknown MSR (0x%llX)!",msr_info->entries[i].entry);
							return noir_invalid_parameter;
						}
					}
				}
				st=noir_success;
			}
		}
	}
	return st;
}

noir_status nvc_hax_run_vcpu(noir_cvm_virtual_cpu_p vcpu)
{
	noir_status st;
	noir_hax_tunnel_p htun=(noir_hax_tunnel_p)vcpu->tunnel;
	if(htun->exit_status==hax_exit_io && htun->io.direction)
	{
		// Input operation has post-processing procedures...
		if(htun->io.flags.string)
		{
			// FIXME: For ins instructions, I/O buffer must be copied to input buffer.
			;
		}
		else
		{
			// For out instructions, I/O buffer must be copied to register.
			noir_copy_memory(&vcpu->gpr.rax,vcpu->iobuff,htun->io.size);
		}
	}
	st=nvc_run_vcpu(vcpu,null);
	while(st==noir_success)
	{
		bool resumption=false;
		// NoirVisor CVM VM-Exit happened.
		nv_dprintf("CVM VM-Exit in Haxm! Intercept Code=0x%X\n",vcpu->exit_context.intercept_code);
		// Translate CVM Exit Context into HAXM Tunnel.
		switch(vcpu->exit_context.intercept_code)
		{
			case cv_invalid_state:
			case cv_shutdown_condition:
			{
				// For invalid states and shutdown conditions,
				// Intel HAXM interprets them as state changes.
				htun->exit_status=hax_exit_state_change;
				noir_int3();
				break;
			}
			case cv_memory_access:
			{
				// Memory Access Interceptions can be either Fast MMIO or Page Fault.
				htun->exit_status=hax_exit_page_fault;
				noir_int3();
				break;
			}
			case cv_hlt_instruction:
			{
				htun->exit_status=hax_exit_hlt;
				htun->ready_for_event_injection=true;
				// Intel HAXM automatically advance rip.
				nvc_edit_vcpu_registers(vcpu,noir_cvm_instruction_pointer,&vcpu->exit_context.next_rip,sizeof(u64));
				break;
			}
			case cv_io_instruction:
			{
				htun->exit_status=hax_exit_io;
				htun->io.direction=(u8)vcpu->exit_context.io.access.io_type;
				htun->io.port=vcpu->exit_context.io.port;
				htun->io.size=(u8)vcpu->exit_context.io.access.operand_size;
				htun->io.flags.string=(u8)vcpu->exit_context.io.access.string;
				htun->io.count=1;
				nv_dprintf("PIO on Port=0x%04X! Size=%u, (%s, %s)\n",htun->io.port,htun->io.size,htun->io.direction?"in":"out",htun->io.flags.string?"string":"value");
				if(htun->io.flags.string)
				{
					u64 mask_a=(u64)-1;
					u64 mask_b=0;
					if(htun->io.direction)	// The ins instruction uses rdi.
						htun->io.gva=vcpu->exit_context.io.rdi;
					else					// The outs instruction uses rsi.
						htun->io.gva=vcpu->exit_context.io.rsi;
					// Append the base address.
					htun->io.gva+=vcpu->exit_context.io.segment.base;
					// Truncate the linear address by the width.
					noir_copy_memory(&mask_b,&mask_a,vcpu->exit_context.io.access.address_width);
					htun->io.gva&=mask_b;
					if(!htun->io.direction)
					{
						u16 size=htun->io.size;
						// FIXME: For outs instruction. Output buffer must be copied to I/O Buffer.
						if(vcpu->exit_context.io.access.repeat)size*=(u16)vcpu->exit_context.io.rcx;
					}
					nv_dprintf("String I/O is unsupported!\n");
					noir_int3();
				}
				else if(!htun->io.direction)
				{
					// For out instruction, Output register must be copied to I/O Buffer.
					noir_copy_memory(vcpu->iobuff,&vcpu->exit_context.io.rax,vcpu->exit_context.io.access.operand_size);
				}
				// For in instruction, there is post processing mechanism.
				// Intel HAXM automatically advance rip.
				nvc_edit_vcpu_registers(vcpu,noir_cvm_instruction_pointer,&vcpu->exit_context.next_rip,sizeof(u64));
				break;
			}
			case cv_scheduler_exit:
			{
				htun->exit_status=hax_exit_interrupt;
				break;
			}
			default:
			{
				// Intel HAXM does not know what this CVM interception is.
				htun->exit_status=hax_exit_unknown;
				break;
			}
		}
		htun->exit_reason=(u32)vcpu->exit_context.intercept_code;
		if(!resumption)break;
		st=nvc_run_vcpu(vcpu,null);
	}
	return st;
}