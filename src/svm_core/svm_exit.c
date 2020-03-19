/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the basic Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_exit.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <intrin.h>
#include <amd64.h>
#include <ci.h>
#include "svm_vmcb.h"
#include "svm_npt.h"
#include "svm_exit.h"
#include "svm_def.h"

// Unexpected VM-Exit occured. You may want to debug your code if this function is invoked.
void static fastcall nvc_svm_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	i32 code=noir_svm_vmread32(vmcb,exit_code);
	/*
	  Following conditions might cause the default handler to be invoked:

	  1. You have set an unwanted flag in VMCB of interception.
	  2. You forgot to write a handler for the VM-Exit.
	  3. You forgot to set the handler address to the Exit Handler Group.

	  Use the printed Intercept Code to debug.
	  For example, you received 0x401 as the intercept code.
	  This means you enabled nested paging but did not set a #NPF handler.
	*/
	nv_dprintf("Unhandled VM-Exit! Intercept Code: 0x%X\n",code);
}

// Expected Intercept Code: -1
void static fastcall nvc_svm_invalid_guest_state(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	u64 efer;
	ulong_ptr cr0,cr3,cr4;
	ulong_ptr dr6,dr7;
	u32 list1,list2;
	u32 asid;
	nv_dprintf("[Processor %d] Guest State is Invalid! VMCB: 0x%p\n",vcpu->proc_id,vmcb);
	// Dump State in VMCB and Print them to Debugger.
	efer=noir_svm_vmread64(vmcb,guest_efer);
	nv_dprintf("Guest EFER MSR: 0x%llx\n",efer);
	dr6=noir_svm_vmread(vmcb,guest_dr6);
	dr7=noir_svm_vmread(vmcb,guest_dr7);
	nv_dprintf("Guest DR6: 0x%p\t DR7: 0x%p\n",dr6,dr7);
	cr0=noir_svm_vmread(vmcb,guest_cr0);
	cr3=noir_svm_vmread(vmcb,guest_cr3);
	cr4=noir_svm_vmread(vmcb,guest_cr4);
	nv_dprintf("Guest CR0: 0x%p\t CR3: 0x%p\t CR4: 0x%p\n",cr0,cr3,cr4);
	asid=noir_svm_vmread32(vmcb,guest_asid);
	nv_dprintf("Guest ASID: %d\n",asid);
	list1=noir_svm_vmread32(vmcb,intercept_instruction1);
	list2=noir_svm_vmread32(vmcb,intercept_instruction2);
	nv_dprintf("Control 1: 0x%X\t Control 2: 0x%X\n",list1,list2);
	// Generate a debug-break.
	noir_int3();
}

// Expected Intercept Code: 0x72
void static fastcall nvc_svm_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	u32 ia=(u32)gpr_state->rax;
	u32 ic=(u32)gpr_state->rcx;
	// Here, we implement the cpuid cache to improve performance on nested VM scenario.
	// First, classify the leaf function.
	u32 leaf_class=noir_cpuid_class(ia);
	u32 leaf_func=noir_cpuid_index(ia);
	nv_dprintf("CPUID instruction is intercepted! EAX=0x%X\t ECX=0x%X\n",ia,ic);
	// Second, filter invalid leaves, and invoke these valid.
	if(vcpu->cpuid_cache.max_leaf[leaf_class]>=leaf_func)
		svm_cpuid_handlers[leaf_class][leaf_func](gpr_state,vcpu);	// Invoke if valid.
	else
		nvc_svm_reserved_cpuid_handler(gpr_state,vcpu);
	// Finally, advance the instruction pointer.
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// Expected Intercept Code: 0x7C
void static fastcall nvc_svm_msr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// The index of MSR is saved in ecx register (32-bit).
	u32 index=(u32)gpr_state->rcx;
	// Determine the type of operation.
	bool op_write=noir_svm_vmread8(vmcb,exit_info1);
	large_integer val={0};
	// 
	if(op_write)
	{
		// Get the value to be written.
		val.low=(u32)gpr_state->rax;
		val.high=(u32)gpr_state->rdx;
		switch(index)
		{
			case amd64_efer:
			{
				// This is for future feature of nested virtualization.
				vcpu->nested_hvm.svme=noir_bt(&val.low,amd64_efer_svme);
				val.value|=amd64_efer_svme_bit;
				// Other bits can be ignored, but SVME should be always protected.
				noir_svm_vmwrite64(vmcb,guest_efer,val.value);
				break;
			}
			case amd64_hsave_pa:
			{
				// Store the physical address of Host-Save Area to nested HVM structure.
				vcpu->nested_hvm.hsave_gpa=val.value;
				break;
			}
		}
	}
	else
	{
		switch(index)
		{
			case amd64_efer:
			{
				// Read the EFER value from VMCB.
				val.value=noir_svm_vmread64(vmcb,guest_efer);
				// The SVME bit should be filtered.
				val.value&=(vcpu->nested_hvm.svme<<amd64_efer_svme);
				break;
			}
			case amd64_hsave_pa:
			{
				// Read the physical address of Host-Save Area from nested HVM structure.
				val.value=vcpu->nested_hvm.hsave_gpa=val.value;
				break;
			}
			// To be implemented in future.
#if defined(_amd64)
			case amd64_lstar:
			{
				val.value=(u64)orig_system_call;
				break;
			}
#else
			case amd64_sysenter_eip:
			{
				val.value=(u64)orig_system_call;
				break;
			}
#endif
		}
		*(u32*)&gpr_state->rax=val.low;
		*(u32*)&gpr_state->rdx=val.high;
	}
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// Expected Intercept Code: 0x80
// This is the cornerstone of nesting virtualization.
void static fastcall nvc_svm_vmrun_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	nv_dprintf("VM-Exit occured by vmrun instruction!\n");
	nv_dprintf("Nested Virtualization of SVM is not supported!\n");
}

// Expected Intercept Code: 0x81
void static fastcall nvc_svm_vmmcall_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	u32 vmmcall_func=(u32)gpr_state->rcx;
	ulong_ptr context=gpr_state->rdx;
	ulong_ptr gsp=noir_svm_vmread(vcpu->vmcb.virt,guest_rsp);
	ulong_ptr gip=noir_svm_vmread(vcpu->vmcb.virt,guest_rip);
	ulong_ptr gcr3=noir_svm_vmread(vcpu->vmcb.virt,guest_cr3);
	switch(vmmcall_func)
	{
		case noir_svm_callexit:
		{
			// Validate the caller to prevent malicious unloading request.
			if(gip>=hvm_p->hv_image.base && gip<hvm_p->hv_image.base+hvm_p->hv_image.size)
			{
				// Directly use space from the starting stack position.
				// Normally it is unused.
				noir_gpr_state_p saved_state=(noir_gpr_state_p)vcpu->hv_stack;
				nv_dprintf("VMM-Call for Restoration is intercepted. Exiting...\n");
				// Copy state.
				noir_movsp(saved_state,gpr_state,sizeof(void*)*2);
				saved_state->rax=noir_svm_vmread(vcpu->vmcb.virt,next_rip);
				saved_state->rcx=noir_svm_vmread(vcpu->vmcb.virt,guest_rflags);
				saved_state->rdx=gsp;
				// Restore processor's hidden state.
				noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,(u64)orig_system_call);
				noir_svm_stgi();
				noir_svm_vmload((ulong_ptr)vcpu->vmcb.phys);
				// Switch to Restored CR3
				noir_writecr3(gcr3);
				// Mark the processor is in transition mode.
				vcpu->status=noir_virt_trans;
				// Return to the caller at Host Mode.
				nvc_svm_return(saved_state);
			}
			// If execution goes here, then the invoker is malicious.
			nv_dprintf("Malicious call of exit!\n");
			break;
		}
		default:
		{
			nv_dprintf("Unknown vmmcall function!\n");
			break;
		}
	}
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// Expected Intercept Code: 0x400
// Do not output to debugger since this may seriously degrade performance.
void static fastcall nvc_svm_nested_pf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	bool advance=true;
	i32 lo=0,hi=noir_hook_pages_count;
	// Necessary Information for #NPF VM-Exit.
	u64 gpa=noir_svm_vmread64(vcpu->vmcb.virt,exit_info2);
	amd64_npt_fault_code fault;
	fault.value=noir_svm_vmread64(vcpu->vmcb.virt,exit_info1);
	if(fault.execute)
	{
		// Check if we should switch to secondary.
		while(hi>=lo)
		{
			i32 mid=(lo+hi)>>1;
			noir_hook_page_p nhp=&noir_hook_pages[mid];
			if(gpa>=nhp->orig.phys+page_size)
				lo=mid+1;
			else if(gpa<nhp->orig.phys)
				hi=mid-1;
			else
			{
				noir_npt_manager_p nptm=(noir_npt_manager_p)hvm_p->relative_hvm->secondary_nptm;
				noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
				advance=false;
				break;
			}
		}
		if(advance)
		{
			// Execution is outside hooked page.
			// We should switch to primary.
			noir_npt_manager_p nptm=(noir_npt_manager_p)hvm_p->relative_hvm->primary_nptm;
			noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
			advance=false;
		}
		// We switched NPT. Thus we should clean VMCB cache state.
		noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_npt);
		// It is necessary to flush TLB.
		noir_svm_vmwrite32(vcpu->vmcb.virt,tlb_control,nvc_svm_tlb_control_flush_guest);
	}
	/*
	  There are three inspections in #NPF handler of NoirVisor.
	  Inspection I:		Stealth Inline Hook Concealment
	  Inspection II:	Real-Time Code Integrity Enforcement
	  Inspection III:	Critical Hypervisor Protection
	  
	  We simply have to make inspection I.
	  Inspection II & III does not matter as we initialized
	  the "advance" variable to true.
	*/
	// Fix ME: Complete the Inspection I - Stealth Inline Hook.
	// Inspection I completed...
	if(advance)
	{
		// Note that SVM won't save the next rip in #NPF.
		// Hence we should advance rip by software analysis.
		void* instruction=(void*)((ulong_ptr)vcpu->vmcb.virt+guest_instruction_bytes);
		// Determine Long-Mode through CS.L bit.
		u16* cs_attrib=(u16*)((ulong_ptr)vcpu->vmcb.virt+guest_cs_attrib);
		u32 increment=noir_get_instruction_length(instruction,noir_bt(cs_attrib,9));
		ulong_ptr gip=noir_svm_vmread(vcpu->vmcb.virt,guest_rip);
		gip+=increment;
		noir_svm_vmwrite(vcpu->vmcb.virt,guest_rip,gip);
	}
}

void fastcall nvc_svm_exit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	// Get the linear address of VMCB.
	void* vmcb_va=vcpu->vmcb.virt;
	// Read the Intercept Code.
	i32 intercept_code=noir_svm_vmread32(vmcb_va,exit_code);
	// Determine the group and number of interception.
	u8 code_group=(u8)((intercept_code&0xC00)>>10);
	u16 code_num=(u16)(intercept_code&0x3FF);
	// rax is saved to VMCB, not GPR state.
	gpr_state->rax=noir_svm_vmread(vmcb_va,guest_rax);
	// Set VMCB Cache State as all to be cached.
	if(vcpu->enabled_feature & noir_svm_vmcb_caching)
		noir_svm_vmwrite32(vmcb_va,vmcb_clean_bits,0xffffffff);
	// Set TLB Control to Do-not-Flush
	noir_svm_vmwrite32(vmcb_va,tlb_control,nvc_svm_tlb_control_do_nothing);
	// Check if the interception is due to invalid guest state.
	// Invoke the handler accordingly.
	if(intercept_code==-1)
		nvc_svm_invalid_guest_state(gpr_state,vcpu);
	else
		svm_exit_handlers[code_group][code_num](gpr_state,vcpu);
	// Since rax register is operated, save to VMCB.
	noir_svm_vmwrite(vmcb_va,guest_rax,gpr_state->rax);
	// The rax in GPR state should be the physical address of VMCB
	// in order to execute the vmrun instruction properly.
	gpr_state->rax=(ulong_ptr)vcpu->vmcb.phys;
	// After VM-Exit, Global Interrupt is always disabled. So enable it before vmrun.
	noir_svm_stgi();
}

bool nvc_svm_build_exit_handler()
{
	// Allocate the array of Exit-Handler Group
	svm_exit_handlers=noir_alloc_nonpg_memory(sizeof(void*)*4);
	if(svm_exit_handlers)
	{
		// Allocate arrays of Exit-Handlers
		svm_exit_handlers[0]=noir_alloc_nonpg_memory(noir_svm_maximum_code1*sizeof(void*));
		svm_exit_handlers[1]=noir_alloc_nonpg_memory(noir_svm_maximum_code2*sizeof(void*));
		if(svm_exit_handlers[0] && svm_exit_handlers[1])
		{
			// Initialize it with default-handler.
			// Using stos instruction could accelerate the initialization.
			noir_stosp(svm_exit_handlers[0],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code1);
			noir_stosp(svm_exit_handlers[1],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code2);
		}
		else
		{
			// Allocation failed. Perform cleanup.
			if(svm_exit_handlers[0])noir_free_nonpg_memory(svm_exit_handlers[0]);
			if(svm_exit_handlers[1])noir_free_nonpg_memory(svm_exit_handlers[1]);
			noir_free_nonpg_memory(svm_exit_handlers);
			return false;
		}
		// Zero the group if it is unused.
		svm_exit_handlers[2]=svm_exit_handlers[3]=null;
		// Setup Exit-Handlers
		svm_exit_handlers[0][intercepted_cpuid]=nvc_svm_cpuid_handler;
		svm_exit_handlers[0][intercepted_msr]=nvc_svm_msr_handler;
		svm_exit_handlers[0][intercepted_vmrun]=nvc_svm_vmrun_handler;
		svm_exit_handlers[0][intercepted_vmmcall]=nvc_svm_vmmcall_handler;
		svm_exit_handlers[1][nested_page_fault-0x400]=nvc_svm_nested_pf_handler;
		return true;
	}
	return false;
}

void nvc_svm_teardown_exit_handler()
{
	// Check if Exit Handler Group array is allocated.
	if(svm_exit_handlers)
	{
		// Check if Exit Handler Groups are allocated.
		// Free them accordingly.
		if(svm_exit_handlers[0])noir_free_nonpg_memory(svm_exit_handlers[0]);
		if(svm_exit_handlers[1])noir_free_nonpg_memory(svm_exit_handlers[1]);
		if(svm_exit_handlers[2])noir_free_nonpg_memory(svm_exit_handlers[2]);
		if(svm_exit_handlers[3])noir_free_nonpg_memory(svm_exit_handlers[3]);
		// Free the Exit Handler Group array.
		noir_free_nonpg_memory(svm_exit_handlers);
		// Mark as released.
		svm_exit_handlers=null;
	}
}