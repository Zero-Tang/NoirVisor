/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file operates the nested virtual processor of Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_nvcpu.c
*/

#include <nvdef.h>
#include <nvstatus.h>
#include <nvbdk.h>
#include <vt_intrin.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include "vt_vmcs.h"
#include "vt_def.h"

void nvc_vt_build_nested_vmx_msr(noir_vt_nested_vcpu_p nested_vcpu)
{
	ia32_vmx_basic_msr basic_msr;
	ia32_vmx_pinbased_ctrl_msr pinbased_msr,true_pinbased_msr;
	ia32_vmx_priproc_ctrl_msr priproc_msr,true_priproc_msr;
	ia32_vmx_2ndproc_ctrl_msr secproc_msr;
	ia32_vmx_exit_ctrl_msr exit_msr,true_exit_msr;
	ia32_vmx_entry_ctrl_msr entry_msr,true_entry_msr;
	ia32_vmx_misc_msr misc_msr;
	ia32_vmx_ept_vpid_cap_msr ept_vpid_msr;
	// Process the VMX Basic MSR.
	basic_msr.value=noir_rdmsr(ia32_vmx_basic);
	basic_msr.dual_mon_treat=0;		// Cannot support Dual Monitor Treatment.
	nested_vcpu->vmx_msr[ia32_vmx_basic-ia32_vmx_basic]=basic_msr.value;
	// Process the Pin-Based VM-Execution Control MSR.
	pinbased_msr.value=noir_rdmsr(ia32_vmx_pinbased_ctrl);
	true_pinbased_msr.value=noir_rdmsr(ia32_vmx_true_pinbased_ctrl);
	nested_vcpu->vmx_msr[ia32_vmx_pinbased_ctrl-ia32_vmx_basic]=pinbased_msr.value;
	nested_vcpu->vmx_msr[ia32_vmx_true_pinbased_ctrl-ia32_vmx_basic]=true_pinbased_msr.value;
	// Process the Primary Processor-Based VM-Execution Control MSR.
	priproc_msr.value=noir_rdmsr(ia32_vmx_priproc_ctrl);
	true_priproc_msr.value=noir_rdmsr(ia32_vmx_true_priproc_ctrl);
	nested_vcpu->vmx_msr[ia32_vmx_priproc_ctrl-ia32_vmx_basic]=priproc_msr.value;
	nested_vcpu->vmx_msr[ia32_vmx_true_priproc_ctrl-ia32_vmx_basic]=true_priproc_msr.value;
	// Process the Secondary Processor-Based VM-Execution Control MSR.
	secproc_msr.value=noir_rdmsr(ia32_vmx_2ndproc_ctrl);
	secproc_msr.allowed1_settings.enable_ept=0;	// We cannot emulate Intel EPT.
	nested_vcpu->vmx_msr[ia32_vmx_2ndproc_ctrl-ia32_vmx_basic]=secproc_msr.value;
	// Process the VM-Exit Control MSR
	exit_msr.value=noir_rdmsr(ia32_vmx_exit_ctrl);
	true_exit_msr.value=noir_rdmsr(ia32_vmx_true_exit_ctrl);
	nested_vcpu->vmx_msr[ia32_vmx_exit_ctrl-ia32_vmx_basic]=exit_msr.value;
	nested_vcpu->vmx_msr[ia32_vmx_true_exit_ctrl-ia32_vmx_basic]=true_exit_msr.value;
	// Process the VM-Entry Control MSR
	entry_msr.value=noir_rdmsr(ia32_vmx_entry_ctrl);
	true_entry_msr.value=noir_rdmsr(ia32_vmx_true_entry_ctrl);
	nested_vcpu->vmx_msr[ia32_vmx_entry_ctrl-ia32_vmx_basic]=entry_msr.value;
	nested_vcpu->vmx_msr[ia32_vmx_true_entry_ctrl-ia32_vmx_basic]=true_entry_msr.value;
	// Fixed CR0/CR4
	nested_vcpu->vmx_msr[ia32_vmx_cr0_fixed0-ia32_vmx_basic]=noir_rdmsr(ia32_vmx_cr0_fixed0);
	nested_vcpu->vmx_msr[ia32_vmx_cr0_fixed1-ia32_vmx_basic]=noir_rdmsr(ia32_vmx_cr0_fixed1);
	nested_vcpu->vmx_msr[ia32_vmx_cr4_fixed0-ia32_vmx_basic]=noir_rdmsr(ia32_vmx_cr4_fixed0);
	nested_vcpu->vmx_msr[ia32_vmx_cr4_fixed1-ia32_vmx_basic]=noir_rdmsr(ia32_vmx_cr4_fixed1);
	// Process EPT/VPID Capability
	nested_vcpu->vmx_msr[ia32_vmx_ept_vpid_cap-ia32_vmx_basic]=0;	// No EPT support by now.
	// Process vmfunc Supportability
	nested_vcpu->vmx_msr[ia32_vmx_vmfunc-ia32_vmx_basic]=0;
	// Process VMCS Enumeration
	nested_vcpu->vmx_msr[ia32_vmx_vmcs_enum-ia32_vmx_basic]=0x34;
	// Process Miscellaneous VMX Info
	misc_msr.value=noir_rdmsr(ia32_vmx_misc);
	nested_vcpu->vmx_msr[ia32_vmx_misc-ia32_vmx_basic]=misc_msr.value;
	// Process EPT/VPID Capability - indicate no EPT support, but reserve VPID support.
	ept_vpid_msr.value=noir_rdmsr(ia32_vmx_ept_vpid_cap);
	ept_vpid_msr.support_exec_only_translation=0;
	ept_vpid_msr.page_walk_length_of4=0;
	ept_vpid_msr.support_uc_ept=0;
	ept_vpid_msr.support_wb_ept=0;
	ept_vpid_msr.support_2mb_paging=0;
	ept_vpid_msr.support_1gb_paging=0;
	ept_vpid_msr.support_invept=0;
	ept_vpid_msr.support_accessed_dirty_flags=0;
	ept_vpid_msr.report_advanced_epf_exit_info=0;
	ept_vpid_msr.support_single_context_invept=0;
	ept_vpid_msr.support_all_context_invept=0;
	nested_vcpu->vmx_msr[ia32_vmx_ept_vpid_cap-ia32_vmx_basic]=ept_vpid_msr.value;
}

bool noir_vt_build_nested_vcpu(noir_vt_nested_vcpu_p nested_vcpu)
{
	nvc_vt_build_nested_vmx_msr(nested_vcpu);
	if(noir_vt_vmclear(&nested_vcpu->vmcs_t.phys)==0)
	{
		if(noir_vt_vmptrld(&nested_vcpu->vmcs_t.phys)==0)
		{
			/*
			  Fix ME: Followings are steps in future commits:

			  Fill out the control fields and host state.
			  Leave the guest state blank for a while since there
			  won't be a nested hypervisor immediately - Fill the
			  guest state after a nested hypervisor was built.

			  The host state must always be consistent with the
			  current configuration of NoirVisor's host context.

			  Host State in "Abstracted-to-CPU VMCS" should be a
			  context ready for Nested VM-Exit.

			  Control Fields - "VM-Exit Controls" - should lead
			  the VM-Exit to NoirVisor's context correctly.
			*/
			return true;
		}
	}
	return false;
}

void noir_vt_vmsuccess()
{
	ulong_ptr gflags;
	noir_vt_vmread(guest_rflags,&gflags);
	gflags&=0x3f7702;			// Clear CF PF AF ZF SF OF bits.
	noir_vt_vmwrite(guest_rflags,gflags);
}

void noir_vt_vmfail_invalid()
{
	ulong_ptr gflags;
	noir_vt_vmread(guest_rflags,&gflags);
	gflags&=0x3f7702;			// Clear CF PF AF ZF SF OF bits.
	noir_bts((u32*)&gflags,0);	// Set CF
	noir_vt_vmwrite(guest_rflags,gflags);
}

void noir_vt_vmfail_valid()
{
	ulong_ptr gflags;
	noir_vt_vmread(guest_rflags,&gflags);
	gflags&=0x3f7702;			// Clear CF PF AF ZF SF OF bits.
	noir_bts((u32*)&gflags,6);	// Set ZF
	noir_vt_vmwrite(guest_rflags,gflags);
}

bool noir_vt_nested_vmread(void* vmcs,u32 encoding,ulong_ptr* data)
{
	ia32_vmx_vmcs_encoding translator;
	u8 w,t;
	translator.value=encoding;
	w=(u8)translator.width;
	t=(u8)translator.type;
	if(translator.field<noir_vt_vmcs_limit[w][t])
	{
		u16 offset=noir_vt_vmcs_redirection[w][t];
		offset+=noir_vt_vmcs_field_size[w]*translator.field;
		switch(w)
		{
			case 0:
			{
				if(translator.hi)goto invalid_field;
				*(u16*)data=*(u16*)((ulong_ptr)vmcs+offset);
				break;
			}
			case 1:
			{
#if defined(_amd64)
				if(translator.hi)
					*(u32*)data=*(u32*)((u64)vmcs+offset+4);
				else
					*data=*(u64*)((u64)vmcs+offset);
#else
				offset+=translator.hi<<2;
				*data=*(u32*)((u32)vmcs+offset);
#endif
				break;
			}
			case 2:
			{
				if(translator.hi)goto invalid_field;
				*(u32*)data=*(u32*)((ulong_ptr)vmcs+offset);
				break;
			}
			case 3:
			{
				if(translator.hi)goto invalid_field;
				*data=*(ulong_ptr*)((ulong_ptr)vmcs+offset);
				break;
			}
		}
		noir_vt_vmsuccess();
		return true;
	}
invalid_field:
	noir_vt_vmfail_valid();
	noir_vt_nested_vmwrite(vmcs,vm_instruction_error,vmrw_unsupported_field);
	return false;
}

bool noir_vt_nested_vmwrite(void* vmcs,u32 encoding,ulong_ptr data)
{
	ia32_vmx_vmcs_encoding translator;
	u8 w,t;
	translator.value=encoding;
	w=(u8)translator.width;
	t=(u8)translator.type;
	if(translator.field<noir_vt_vmcs_limit[w][t])
	{
		u16 offset=noir_vt_vmcs_redirection[w][t];
		offset+=noir_vt_vmcs_field_size[w]*translator.field;
		switch(w)
		{
			case 0:
			{
				if(translator.hi)goto invalid_field;
				*(u16*)((ulong_ptr)vmcs+offset)=(u16)data;
				break;
			}
			case 1:
			{
#if defined(_amd64)
				if(translator.hi)
					*(u32*)((u64)vmcs+offset+4)=(u32)data;
				else
					*(u64*)((u64)vmcs+offset)=data;
#else
				offset+=translator.hi<<2;
				*(u32*)((u32)vmcs+offset)=data;
#endif
				break;
			}
			case 2:
			{
				if(translator.hi)goto invalid_field;
				*(u32*)((ulong_ptr)vmcs+offset)=(u32)data;
				break;
			}
			case 3:
			{
				if(translator.hi)goto invalid_field;
				*(ulong_ptr*)((ulong_ptr)vmcs+offset)=data;
				break;
			}
		}
		noir_vt_vmsuccess();
		return true;
	}
invalid_field:
	noir_vt_vmfail_valid();
	// This won't cause a dead recursive call.
	noir_vt_nested_vmwrite(vmcs,vm_instruction_error,vmrw_unsupported_field);
	return false;
}

void noir_vt_vmfail(noir_vt_nested_vcpu_p nested_vcpu,u32 message)
{
	if(nested_vcpu->vmcs_c.phys!=0xffffffffffffffff)
	{
		noir_vt_nested_vmcs_header_p header=(noir_vt_nested_vmcs_header_p)nested_vcpu->vmcs_c.virt;
		unref_var(header);
		noir_vt_nested_vmwrite(nested_vcpu->vmcs_c.virt,vm_instruction_error,message);
		noir_vt_vmfail_valid();
		return;
	}
	noir_vt_vmfail_invalid();
}