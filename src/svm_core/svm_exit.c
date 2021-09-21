/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the basic Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_exit.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include <ci.h>
#include "svm_vmcb.h"
#include "svm_npt.h"
#include "svm_exit.h"
#include "svm_def.h"

/* ----------------------------------------------------------------------------
  General Rule of VM-Exit Handler Designing:

  As a rule of thumb, while in VM-Exit, GIF is set to cleared. In this regard,
  processor enters an atomic state that certain exceptions and most interrupts
  are held pending or even discarded for the sake of atomicity.

  You are not supposed to call system APIs unless such API is quick to complete
  and relies on this core itself to compute only.

  If you would like to debug-print something in host context, invoke the stgi
  instruction to set the GIF so that system won't go to deadlock. Nonetheless,
  please note that atomicity is broken while you make such debug-printing.
-----------------------------------------------------------------------------*/

// Unexpected VM-Exit occured. You may want to debug your code if this function is invoked.
void static fastcall nvc_svm_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	const void* vmcb=vcpu->vmcb.virt;
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
	noir_svm_stgi();
	nv_dprintf("Unhandled VM-Exit! Intercept Code: 0x%X\n",code);
	noir_int3();
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
	noir_svm_stgi();
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

// Expected Intercept Code: 0x14
void static fastcall nvc_svm_cr4_write_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	ulong_ptr *gpr_array=(ulong_ptr*)gpr_state;
	nvc_svm_cr_access_exit_info info;
	ulong_ptr old_cr4=noir_svm_vmread(vmcb,guest_cr4);
	ulong_ptr new_cr4;
	info.value=noir_svm_vmread64(vmcb,exit_info1);
	// Filter CR4.UMIP bit.
	new_cr4=gpr_array[info.gpr];
	// Avoid unnecessary VMCB caching / TLB invalidations.
	if(new_cr4!=old_cr4)
	{
		noir_svm_vmwrite(vmcb,guest_cr4,new_cr4);
		// Check if UMIP is to be switched.
		if(noir_bt((u32*)&old_cr4,amd64_cr4_umip)!=noir_bt((u32*)&new_cr4,amd64_cr4_umip))
			nvc_svm_reconfigure_npiep_interceptions(vcpu);	// Reconfigure NPIEP interceptions.
		// CR4 is to be written. Invalidate the VMCB Cache State.
		noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_control_reg);
		// Writing to CR4 could induce TLB invalidation. Emulate this behavior in VM-Exit handler.
		// Condition #1: Modifying CR4.PAE/PSE/PGE could invalidate all TLB entries.
		if(noir_bt((u32*)&new_cr4,amd64_cr4_pae)!=noir_bt((u32*)&old_cr4,amd64_cr4_pae))goto cr4_flush;
		if(noir_bt((u32*)&new_cr4,amd64_cr4_pse)!=noir_bt((u32*)&old_cr4,amd64_cr4_pse))goto cr4_flush;
		if(noir_bt((u32*)&new_cr4,amd64_cr4_pge)!=noir_bt((u32*)&old_cr4,amd64_cr4_pge))goto cr4_flush;
		// Condition #2: Setting CR4.PKE to 1 could invalidate all TLB entries.
		if(noir_bt((u32*)&new_cr4,amd64_cr4_pke) && !noir_bt((u32*)&old_cr4,amd64_cr4_pke))goto cr4_flush;
		// Condition #3: Clearing CR4.PCIDE to 0 could invalidate all TLB entries.
		if(!noir_bt((u32*)&new_cr4,amd64_cr4_pcide) && noir_bt((u32*)&old_cr4,amd64_cr4_pcide))goto cr4_flush;
		goto cr4_handler_over;
cr4_flush:
		noir_svm_vmwrite8(vmcb,tlb_control,nvc_svm_tlb_control_flush_guest);
	}
cr4_handler_over:
	noir_svm_advance_rip(vmcb);
}

// Expected Intercept Code: 0x5E
void static fastcall nvc_svm_sx_exception_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	u32 error_code=noir_svm_vmread32(vmcb,exit_info1);
	switch(error_code)
	{
		case amd64_sx_init_redirection:
		{
			// Check if guest has set redirection of INIT.
			if(vcpu->nested_hvm.r_init)
				noir_svm_inject_event(vmcb,amd64_security_exception,amd64_fault_trap_exception,true,true,amd64_sx_init_redirection);
			else
			{
				// The default treatment of INIT interception is absurd in AMD-V
				// in that INIT signal would not disappear on interception!
				// We thereby have to redirect INIT signals into #SX exceptions.
				// Emulate what a real INIT Signal would do.
				// For details of emulation, see Table 14-1 in Vol.2 of AMD64 APM.
				u64 cr0=noir_svm_vmread64(vmcb,guest_cr0);
				cr0&=0x60000000;		// Bits CD & NW of CR0 are unchanged during INIT.
				cr0|=0x00000010;		// Bit ET of CR0 is always set.
				noir_svm_vmwrite64(vmcb,guest_cr0,cr0);
				// CR2, CR3 and CR4 are cleared to zero.
				noir_svm_vmwrite64(vmcb,guest_cr2,0);
				noir_svm_vmwrite64(vmcb,guest_cr3,0);
				noir_svm_vmwrite64(vmcb,guest_cr4,0);
				// Leave SVME set but others reset in EFER.
				noir_svm_vmwrite64(vmcb,guest_efer,amd64_efer_svme_bit);
				// Debug Registers...
				noir_writedr0(0);
				noir_writedr1(0);
				noir_writedr2(0);
				noir_writedr3(0);
				noir_svm_vmwrite64(vmcb,guest_dr6,0xFFFF0FF0);
				noir_svm_vmwrite64(vmcb,guest_dr7,0x400);
				// Segment Register - CS
				noir_svm_vmwrite16(vmcb,guest_cs_selector,0xF000);
				noir_svm_vmwrite16(vmcb,guest_cs_attrib,0x9B);
				noir_svm_vmwrite32(vmcb,guest_cs_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_cs_base,0xFFFF0000);
				// Segment Register - DS
				noir_svm_vmwrite16(vmcb,guest_ds_selector,0);
				noir_svm_vmwrite16(vmcb,guest_ds_attrib,0x93);
				noir_svm_vmwrite32(vmcb,guest_ds_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_ds_base,0);
				// Segment Register - ES
				noir_svm_vmwrite16(vmcb,guest_es_selector,0);
				noir_svm_vmwrite16(vmcb,guest_es_attrib,0x93);
				noir_svm_vmwrite32(vmcb,guest_es_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_es_base,0);
				// Segment Register - FS
				noir_svm_vmwrite16(vmcb,guest_fs_selector,0);
				noir_svm_vmwrite16(vmcb,guest_fs_attrib,0x93);
				noir_svm_vmwrite32(vmcb,guest_fs_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_fs_base,0);
				// Segment Register - GS
				noir_svm_vmwrite16(vmcb,guest_gs_selector,0);
				noir_svm_vmwrite16(vmcb,guest_gs_attrib,0x93);
				noir_svm_vmwrite32(vmcb,guest_gs_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_gs_base,0);
				// Segment Register - SS
				noir_svm_vmwrite16(vmcb,guest_ss_selector,0);
				noir_svm_vmwrite16(vmcb,guest_ss_attrib,0x93);
				noir_svm_vmwrite32(vmcb,guest_ss_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_ss_base,0);
				// Segment Register - GDTR
				noir_svm_vmwrite32(vmcb,guest_gdtr_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_gdtr_base,0);
				// Segment Register - IDTR
				noir_svm_vmwrite32(vmcb,guest_idtr_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_idtr_base,0);
				// Segment Register - LDTR
				noir_svm_vmwrite16(vmcb,guest_ldtr_selector,0);
				noir_svm_vmwrite16(vmcb,guest_ldtr_attrib,0x82);
				noir_svm_vmwrite32(vmcb,guest_ldtr_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_ldtr_base,0);
				// Segment Register - TR
				noir_svm_vmwrite16(vmcb,guest_tr_selector,0);
				noir_svm_vmwrite16(vmcb,guest_tr_attrib,0x8b);
				noir_svm_vmwrite32(vmcb,guest_tr_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_tr_base,0);
				// IDTR & GDTR
				noir_svm_vmwrite16(vmcb,guest_gdtr_limit,0xFFFF);
				noir_svm_vmwrite16(vmcb,guest_idtr_limit,0xFFFF);
				noir_svm_vmwrite64(vmcb,guest_gdtr_base,0);
				noir_svm_vmwrite64(vmcb,guest_idtr_base,0);
				// General Purpose Registers...
				noir_svm_vmwrite64(vmcb,guest_rsp,0);
				noir_svm_vmwrite64(vmcb,guest_rip,0xfff0);
				noir_svm_vmwrite64(vmcb,guest_rflags,2);
				noir_stosp(gpr_state,0,sizeof(void*)*2);	// Clear the GPRs.
				gpr_state->rdx=vcpu->cpuid_fms;				// Use info from cached CPUID.
				// FIXME: Set the vCPU to "Wait-for-SPI" State. AMD-V lacks this feature.
				// Flush all TLBs in that paging in the guest is switched off during INIT signal.
				noir_svm_vmwrite8(vmcb,tlb_control,nvc_svm_tlb_control_flush_guest);
				// Mark certain cached items as dirty in order to invalidate them.
				noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_control_reg);
				noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_debug_reg);
				noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_idt_gdt);
				noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_segment_reg);
				noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_cr2);
			}
			break;
		}
		default:
		{
			// Unrecognized #SX Exception, leave it be for the Guest.
			noir_svm_inject_event(vmcb,amd64_security_exception,amd64_fault_trap_exception,true,true,error_code);
			break;
		}
	}
}

bool static fastcall nvc_svm_parse_npiep_operand(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,ulong_ptr *result)
{
	void* vmcb=vcpu->vmcb.virt;
	u64 grip=noir_svm_vmread64(vmcb,guest_rip),nrip=noir_svm_vmread64(vmcb,next_rip);
	svm_segment_access_rights cs_attrib;
	noir_basic_operand opr_info;
#if defined(_hv_type1)
	u8 instruction[15];
#else
	// It is assumed that NoirVisor is now in the same address space with the guest.
	u8 *instruction=(u8*)grip;
#endif
	cs_attrib.value=noir_svm_vmread16(vmcb,guest_cs_attrib);
	// Determine the mode of guest then decode the instruction.
	// Save the decode information to the bottom of the hypervisor stack.
	if(likely(cs_attrib.long_mode))
		noir_decode_instruction64(instruction,nrip-grip,vcpu->hv_stack);
	else if(noir_svm_vmcb_bt32(vmcb,guest_cr0,amd64_cr0_pe))
		noir_decode_instruction32(instruction,nrip-grip,vcpu->hv_stack);
	else
		noir_decode_instruction16(instruction,nrip-grip,vcpu->hv_stack);
	// Extract needed decoded information for NPIEP.
	noir_decode_basic_operand(vcpu->hv_stack,&opr_info);
	if(opr_info.complex.mem_valid)
	{
		ulong_ptr *gpr_array=(ulong_ptr*)gpr_state;
		ulong_ptr pointer=0;
		i32 disp=(i32)opr_info.complex.displacement;
		// If the base register is rsp, load the register from VMCB.
		if(opr_info.complex.base_valid)
			pointer+=opr_info.complex.base==4?noir_svm_vmread(vmcb,guest_rsp):gpr_array[opr_info.complex.base];
		if(opr_info.complex.si_valid)
			pointer+=(gpr_array[opr_info.complex.index]<<opr_info.complex.scale);
		// Apply displacement to the pointer.
		pointer+=disp;
		// Apply segment base to the pointer.
		// Check if the guest is under protected mode or real mode.
		// Let branch predictor prefer protected mode (and long mode).
		if(likely(noir_svm_vmcb_bt32(vmcb,guest_cr0,amd64_cr0_pe)))
			pointer+=noir_svm_vmread(vmcb,guest_es_base+(opr_info.complex.segment<<4));
		else
			pointer+=(noir_svm_vmread16(vmcb,guest_es_selector+(opr_info.complex.segment<<4))<<4);
		// Check the size of address width of the operand. Truncate the pointer if needed.
		if(opr_info.complex.address_size==1)
			pointer&=0xffffffff;
		else if(opr_info.complex.address_size==0)
			pointer&=0xffff;
		*result=pointer;
	}
	else if(opr_info.complex.reg_valid)
	{
		// Save the operand size and the register index.
		*result=opr_info.complex.reg1;
		*result|=(opr_info.complex.operand_size<<30);
	}
	return opr_info.complex.mem_valid;
}

// Expected Intercept Code: 0x66
void static fastcall nvc_svm_sidt_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	bool superv_instruction=(noir_svm_vmread8(vmcb,guest_cpl)==0);
	ulong_ptr decode_result=0;
	// AMD-V does not provide further decoding assistance regarding operands.
	// We must decode the instruction on our own.
#if !defined(_hv_type1)
	// Step 0. Switch to the guest address space.
	if(!superv_instruction)
	{
		// If NPIEP is taking duty of this instruction, we should switch
		// the page table so that we can gain access in order to decode
		// the instruction and also write the result.
		u64 gcr3=noir_get_current_process_cr3();
		noir_writecr3(gcr3);
	}
#endif
	// Step 1. Invoke disassembler to decode the instruction.
	if(nvc_svm_parse_npiep_operand(gpr_state,vcpu,&decode_result))
	{
		// Construct the pointer.
		ulong_ptr pointer=decode_result;
#if defined(_hv_type1)
#else
		// FIXME: Check if the page is present and writable.
		// If the page is absent/readonly, throw #PF to the guest.
		if(superv_instruction)
		{
			*(u16*)pointer=noir_svm_vmread16(vmcb,guest_idtr_limit);
			*(ulong_ptr*)(pointer+2)=noir_svm_vmread(vmcb,guest_idtr_base);
		}
		else if(noir_bt((u32*)&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sidt))
		{
			// Change the value to be returned if you would like to.
			*(u16*)pointer=0x2333;
			*(ulong_ptr*)(pointer+2)=0x66666666;
			noir_writecr3(system_cr3);
		}
#endif
	}
	noir_svm_advance_rip(vmcb);
}

// Expected Intercept Code: 0x67
void static fastcall nvc_svm_sgdt_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	bool superv_instruction=(noir_svm_vmread8(vmcb,guest_cpl)==0);
	ulong_ptr decode_result=0;
	// AMD-V does not provide further decoding assistance regarding operands.
	// We must decode the instruction on our own.
#if !defined(_hv_type1)
	// Step 0. Switch to the guest address space.
	if(!superv_instruction)
	{
		// If NPIEP is taking duty of this instruction, we should switch
		// the page table so that we can gain access in order to decode
		// the instruction and also write the result.
		u64 gcr3=noir_get_current_process_cr3();
		noir_writecr3(gcr3);
	}
#endif
	// Step 1. Invoke disassembler to decode the instruction.
	if(nvc_svm_parse_npiep_operand(gpr_state,vcpu,&decode_result))
	{
		// Construct the pointer.
		ulong_ptr pointer=decode_result;
#if defined(_hv_type1)
#else
		// FIXME: Check if the page is present and writable.
		// If the page is absent/readonly, throw #PF to the guest.
		if(superv_instruction)
		{
			*(u16*)pointer=noir_svm_vmread16(vmcb,guest_gdtr_limit);
			*(ulong_ptr*)(pointer+2)=noir_svm_vmread(vmcb,guest_gdtr_base);
		}
		else if(noir_bt((u32*)&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sgdt))
		{
			// Change the value to be returned if you would like to.
			*(u16*)pointer=0x2333;
			*(ulong_ptr*)(pointer+2)=0x66666666;
			noir_writecr3(system_cr3);
		}
#endif
	}
	noir_svm_advance_rip(vmcb);
}

// Expected Intercept Code: 0x68
void static fastcall nvc_svm_sldt_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	bool superv_instruction=(noir_svm_vmread8(vmcb,guest_cpl)==0);
	ulong_ptr decode_result=0;
	// AMD-V does not provide further decoding assistance regarding operands.
	// We must decode the instruction on our own.
#if !defined(_hv_type1)
	// Step 0. Switch to the guest address space.
	if(!superv_instruction)
	{
		// If NPIEP is taking duty of this instruction, we should switch
		// the page table so that we can gain access in order to decode
		// the instruction and also write the result.
		u64 gcr3=noir_get_current_process_cr3();
		noir_writecr3(gcr3);
	}
#endif
	// Step 1. Invoke disassembler to decode the instruction.
	if(nvc_svm_parse_npiep_operand(gpr_state,vcpu,&decode_result))
	{
		// Construct the pointer.
		ulong_ptr pointer=decode_result;
#if defined(_hv_type1)
#else
		// FIXME: Check if the page is present and writable.
		// If the page is absent/readonly, throw #PF to the guest.
		if(superv_instruction)
			*(u16*)pointer=noir_svm_vmread16(vmcb,guest_ldtr_selector);
		else if(noir_bt((u32*)&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sgdt))
		{
			// Change the value to be returned if you would like to.
			*(u16*)pointer=0x2333;
			noir_writecr3(system_cr3);
		}
#endif
	}
	else
	{
		ulong_ptr* gpr_array=(ulong_ptr*)gpr_state;
		u16 ldtr=noir_svm_vmread16(vmcb,guest_ldtr_selector);
		u8 operand_size=(u8)(decode_result>>30);
		u8 register_index=(u8)(decode_result&0xf);
		if(register_index==4)
		{
			if(operand_size==1)
				noir_svm_vmwrite16(vmcb,guest_rsp,ldtr);
			else if(operand_size==2)
				noir_svm_vmwrite32(vmcb,guest_rsp,ldtr);
			else
				noir_svm_vmwrite64(vmcb,guest_rsp,ldtr);
		}
		else
		{
			if(operand_size==2)
				*(u32*)&gpr_array[register_index]=0;
			else if(operand_size==3)
				*(u64*)&gpr_array[register_index]=0;
			*(u16*)&gpr_array[register_index]=ldtr;
		}
	}
	noir_svm_advance_rip(vmcb);
}

// Expected Intercept Code: 0x69
void static fastcall nvc_svm_str_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	bool superv_instruction=(noir_svm_vmread8(vmcb,guest_cpl)==0);
	ulong_ptr decode_result=0;
	// AMD-V does not provide further decoding assistance regarding operands.
	// We must decode the instruction on our own.
#if !defined(_hv_type1)
	// Step 0. Switch to the guest address space.
	if(!superv_instruction)
	{
		// If NPIEP is taking duty of this instruction, we should switch
		// the page table so that we can gain access in order to decode
		// the instruction and also write the result.
		u64 gcr3=noir_get_current_process_cr3();
		noir_writecr3(gcr3);
	}
#endif
	// Step 1. Invoke disassembler to decode the instruction.
	if(nvc_svm_parse_npiep_operand(gpr_state,vcpu,&decode_result))
	{
		// Construct the pointer.
		ulong_ptr pointer=decode_result;
#if defined(_hv_type1)
#else
		// FIXME: Check if the page is present and writable.
		// If the page is absent/readonly, throw #PF to the guest.
		if(superv_instruction)
			*(u16*)pointer=noir_svm_vmread16(vmcb,guest_ldtr_selector);
		else if(noir_bt((u32*)&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sgdt))
		{
			// Change the value to be returned if you would like to.
			*(u16*)pointer=0x2333;
			noir_writecr3(system_cr3);
		}
#endif
	}
	else
	{
		ulong_ptr* gpr_array=(ulong_ptr*)gpr_state;
		u16 tr=noir_svm_vmread16(vmcb,guest_tr_selector);
		u8 operand_size=(u8)(decode_result>>30);
		u8 register_index=(u8)(decode_result&0xf);
		if(register_index==4)
		{
			if(operand_size==1)
				noir_svm_vmwrite16(vmcb,guest_rsp,tr);
			else if(operand_size==2)
				noir_svm_vmwrite32(vmcb,guest_rsp,tr);
			else
				noir_svm_vmwrite64(vmcb,guest_rsp,tr);
		}
		else
		{
			if(operand_size==2)
				*(u32*)&gpr_array[register_index]=0;
			else if(operand_size==3)
				*(u64*)&gpr_array[register_index]=0;
			*(u16*)&gpr_array[register_index]=tr;
		}
	}
	noir_svm_advance_rip(vmcb);
}

// Special CPUID Handler for Nested Virtualization Feature Identification
void static fastcall nvc_svm_cpuid_nested_virtualization_handler(noir_cpuid_general_info_p info)
{
	info->eax=0;		// Let Revision ID=0
	info->ebx=hvm_p->tlb_tagging.limit-1;	// ASID Limit for Nested VMM.
	// ECX is reserved by AMD. Leave it be.
	info->edx=0;		// Make sure that only bits corresponding to supported features are set.
	// Set the bits that NoirVisor supports.
	noir_bts(&info->edx,amd64_cpuid_npt);
	noir_bts(&info->edx,amd64_cpuid_svm_lock);
	noir_bts(&info->edx,amd64_cpuid_nrips);
	noir_bts(&info->edx,amd64_cpuid_vmcb_clean);
	noir_bts(&info->edx,amd64_cpuid_flush_asid);
	noir_bts(&info->edx,amd64_cpuid_decoder);
	noir_bts(&info->edx,amd64_cpuid_vmlsvirt);
	noir_bts(&info->edx,amd64_cpuid_vgif);
}

// Hypervisor-Present CPUID Handler
void static fastcall nvc_svm_cpuid_hvp_handler(u32 leaf,u32 subleaf,noir_cpuid_general_info_p info)
{
	// First, classify the leaf function.
	u32 leaf_class=noir_cpuid_class(leaf);
	u32 leaf_func=noir_cpuid_index(leaf);
	if(leaf_class==hvm_leaf_index)
	{
		if(leaf_func<hvm_p->relative_hvm->hvm_cpuid_leaf_max)
			hvm_cpuid_handlers[leaf_func](info);
		else
			noir_stosd((u32*)&info,0,4);
	}
	else
	{
		noir_cpuid(leaf,subleaf,&info->eax,&info->ebx,&info->ecx,&info->edx);
		switch(leaf)
		{
			case amd64_cpuid_std_proc_feature:
			{
				noir_bts(&info->ecx,amd64_cpuid_hv_presence);
				break;
			}
			case amd64_cpuid_ext_proc_feature:
			{
				// noir_btr(&info->ecx,amd64_cpuid_svm);
				break;
			}
			case amd64_cpuid_ext_svm_features:
			{
				nvc_svm_cpuid_nested_virtualization_handler(info);
				break;
			}
			case amd64_cpuid_ext_mem_crypting:
			{
				noir_stosd((u32*)info,0,4);
				break;
			}
		}
	}
}

// Hypervisor-Stealthy CPUID Handler
void static fastcall nvc_svm_cpuid_hvs_handler(u32 leaf,u32 subleaf,noir_cpuid_general_info_p info)
{
	noir_cpuid(leaf,subleaf,&info->eax,&info->ebx,&info->ecx,&info->edx);
	switch(leaf)
	{
		case amd64_cpuid_ext_proc_feature:
		{
			// noir_btr(&info->ecx,amd64_cpuid_svm);
			break;
		}
		case amd64_cpuid_ext_svm_features:
		{
			nvc_svm_cpuid_nested_virtualization_handler(info);
			break;
		}
		case amd64_cpuid_ext_mem_crypting:
		{
			noir_stosd((u32*)&info,0,4);
			break;
		}
	}
}

// Expected Intercept Code: 0x72
void static fastcall nvc_svm_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	u32 ia=(u32)gpr_state->rax;
	u32 ic=(u32)gpr_state->rcx;
	noir_cpuid_general_info info;
	nvcp_svm_cpuid_handler(ia,ic,&info);
	*(u32*)&gpr_state->rax=info.eax;
	*(u32*)&gpr_state->rbx=info.ebx;
	*(u32*)&gpr_state->rcx=info.ecx;
	*(u32*)&gpr_state->rdx=info.edx;
	// Finally, advance the instruction pointer.
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// Expected Intercept Code: 0x7A
void static fastcall nvc_svm_invlpga_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// To virtualize the invlpga instruction is just to virtualize the ASID.
		void* addr=(void*)gpr_state->rax;
		u32 asid=(u32)gpr_state->rcx;
		// Perform ASID Range Check then Invalidate the TLB.
		// Beyond the range are the ASIDs reserved for Customizable VM.
		if(asid>0 && asid<vcpu->nested_hvm.asid_max)
			noir_svm_invlpga(addr,asid+1);
		noir_svm_advance_rip(vmcb);
	}
	else
	{
		// SVM is disabled in guest EFER. Inject #UD to guest.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// This is a branch of MSR-Exit. DO NOT ADVANCE RIP HERE!
void static fastcall nvc_svm_rdmsr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// The index of MSR is saved in ecx register (32-bit).
	u32 index=(u32)gpr_state->rcx;
	large_integer val={0};
	if(noir_is_synthetic_msr(index))
	{
		if(hvm_p->options.cpuid_hv_presence)
			val.value=nvc_mshv_rdmsr_handler(&vcpu->mshvcpu,index);
		else
			// Synthetic MSR is not allowed, inject #GP to Guest.
			noir_svm_inject_event(vmcb,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
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
				if(vcpu->nested_hvm.svme==0)val.value&=~amd64_efer_svme_bit;
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
				val.value=vcpu->virtual_msr.lstar;
				break;
			}
#else
			case amd64_sysenter_eip:
			{
				val.value=vcpu->virtual_msr.sysenter_eip;
				break;
			}
#endif
		}
	}
	// Higher 32 bits of rax and rdx will be cleared.
	gpr_state->rax=(ulong_ptr)val.low;
	gpr_state->rdx=(ulong_ptr)val.high;
}

// This is a branch of MSR-Exit. DO NOT ADVANCE RIP HERE!
void static fastcall nvc_svm_wrmsr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// The index of MSR is saved in ecx register (32-bit).
	u32 index=(u32)gpr_state->rcx;
	large_integer val;
	// Get the value to be written.
	val.low=(u32)gpr_state->rax;
	val.high=(u32)gpr_state->rdx;
	if(noir_is_synthetic_msr(index))
	{
		if(hvm_p->options.cpuid_hv_presence)
			nvc_mshv_wrmsr_handler(&vcpu->mshvcpu,index,val.value);
		else
			// Synthetic MSR is not allowed, inject #GP to Guest.
			noir_svm_inject_event(vmcb,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
	}
	else
	{
		switch(index)
		{
			case amd64_efer:
			{
				// This is for future feature of nested virtualization.
				vcpu->nested_hvm.svme=noir_bt(&val.low,amd64_efer_svme);
				val.value|=amd64_efer_svme_bit;
				// Other bits can be ignored, but SVME should be always protected.
				noir_svm_vmwrite64(vmcb,guest_efer,val.value);
				// We updated EFER. Therefore, CRx fields should be invalidated.
				noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_control_reg);
				// Enable Acceleration of Virtualization if available.
				if(vcpu->nested_hvm.svme)
				{
					nvc_svm_instruction_intercept2 list2;
					list2.value=noir_svm_vmread16(vcpu->vmcb.virt,intercept_instruction2);
					if(vcpu->enabled_feature & noir_svm_virtual_gif)
					{
						nvc_svm_avic_control avic_ctrl;
						// Enable vGIF and set GIF for guest.
						avic_ctrl.value=noir_svm_vmread64(vcpu->vmcb.virt,avic_control);
						avic_ctrl.virtual_gif=true;
						avic_ctrl.enable_virtual_gif=true;
						noir_svm_vmwrite64(vcpu->vmcb.virt,avic_control,avic_ctrl.value);
						// We don't have to intercept stgi/clgi anymore.
						list2.intercept_stgi=0;
						list2.intercept_clgi=0;
						// Because TPR-related field is changed in VMCB,
						// Clear the cached state of VMCB.
						noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_tpr);
					}
					if(vcpu->enabled_feature & noir_svm_virtualized_vmls)
					{
						nvc_svm_lbr_virtualization_control lbr_virt_ctrl;
						// Enable Virtualization of vmload/vmsave instructions.
						lbr_virt_ctrl.value=noir_svm_vmread64(vcpu->vmcb.virt,lbr_virtualization_control);
						lbr_virt_ctrl.virtualize_vmsave_vmload=true;
						noir_svm_vmwrite64(vcpu->vmcb.virt,lbr_virtualization_control,lbr_virt_ctrl.value);
						// We don't have to intercept vmload/vmsave anymore.
						list2.intercept_vmload=0;
						list2.intercept_vmsave=0;
					}
					noir_svm_vmwrite16(vcpu->vmcb.virt,intercept_instruction2,list2.value);
					// Clear the cached state of Interceptions in VMCB.
					noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_interception);
				}
				else
				{
					nvc_svm_instruction_intercept2 list2;
					list2.value=noir_svm_vmread16(vcpu->vmcb.virt,intercept_instruction2);
					if(vcpu->enabled_feature & noir_svm_virtual_gif)
					{
						nvc_svm_avic_control avic_ctrl;
						// Disable vGIF for guest.
						avic_ctrl.value=noir_svm_vmread64(vcpu->vmcb.virt,avic_control);
						avic_ctrl.enable_virtual_gif=false;
						noir_svm_vmwrite64(vcpu->vmcb.virt,avic_control,avic_ctrl.value);
						// Because SVME is disabled, we have to intercept stgi/clgi.
						list2.intercept_stgi=1;
						list2.intercept_clgi=1;
						// Because TPR-related field is changed in VMCB,
						// Clear the cached state of VMCB.
						noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_tpr);
					}
					if(vcpu->enabled_feature & noir_svm_virtualized_vmls)
					{
						nvc_svm_lbr_virtualization_control lbr_virt_ctrl;
						// Disable Virtualization of vmload/vmsave instructions.
						lbr_virt_ctrl.value=noir_svm_vmread64(vcpu->vmcb.virt,lbr_virtualization_control);
						lbr_virt_ctrl.virtualize_vmsave_vmload=false;
						noir_svm_vmwrite64(vcpu->vmcb.virt,lbr_virtualization_control,lbr_virt_ctrl.value);
						// We don't have to intercept vmload/vmsave anymore.
						list2.intercept_vmload=1;
						list2.intercept_vmsave=1;
					}
					noir_svm_vmwrite16(vcpu->vmcb.virt,intercept_instruction2,list2.value);
					// Clear the cached state of Interceptions in VMCB.
					noir_svm_vmcb_btr32(vcpu->vmcb.virt,vmcb_clean_bits,noir_svm_clean_interception);
				}
				break;
			}
			case amd64_vmcr:
			{
				vcpu->nested_hvm.r_init=noir_bt(&val.low,amd64_vmcr_r_init);
				break;
			}
			case amd64_hsave_pa:
			{
				// Store the physical address of Host-Save Area to nested HVM structure.
				if(page_4kb_offset(val.value))		// Unaligned address will trigger #GP exception.
					noir_svm_inject_event(vmcb,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
				else
					vcpu->nested_hvm.hsave_gpa=val.value;
				break;
			}
#if defined(_amd64)
			case amd64_lstar:
			{
				vcpu->virtual_msr.lstar=val.value;
				break;
			}
#else
			case amd64_sysenter_eip:
			{
				vcpu->virtual_msr.sysenter_eip=val.value;
				break;
			}
#endif
		}
	}
}

// Expected Intercept Code: 0x7C
void static fastcall nvc_svm_msr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// Determine the type of operation.
	bool op_write=noir_svm_vmread8(vmcb,exit_info1);
	// 
	if(op_write)
		nvc_svm_wrmsr_handler(gpr_state,vcpu);
	else
		nvc_svm_rdmsr_handler(gpr_state,vcpu);
	noir_svm_advance_rip(vmcb);
}

// Expected Intercept Code: 0x7F
// If this VM-Exit occurs, it may indicate a triple fault.
void static fastcall nvc_svm_shutdown_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_stgi();	// Enable GIF for Debug-Printing
	nv_dprintf("Shutdown (Triple-Fault) is Intercepted! System is unusable!\n");
	noir_int3();
}

// Expected Intercept Code: 0x80
// This is the cornerstone of nesting virtualization.
void static fastcall nvc_svm_vmrun_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// Get the Nested VMCB.
		const ulong_ptr nested_vmcb_pa=gpr_state->rax;
		const void* nested_vmcb=noir_find_virt_by_phys(nested_vmcb_pa);
		// Some essential information for hypervisor-specific consistency check.
		const u32 nested_asid=noir_svm_vmread32(nested_vmcb,guest_asid);
		if(nested_asid==0)goto invalid_nested_vmcb;
		// There is absolutely no SVM instructions since we don't support nested virtualization at this point.
invalid_nested_vmcb:
		// Inconsistency found by Hypervisor. Issue VM-Exit immediately.
		noir_svm_vmwrite64(nested_vmcb,exit_code,invalid_guest_state);
		noir_svm_advance_rip(vmcb);
	}
	else
	{
		// SVM is disabled in guest EFER. Inject #UD to guest.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x81
void static fastcall nvc_svm_vmmcall_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	u32 vmmcall_func=(u32)gpr_state->rcx;
	ulong_ptr context=gpr_state->rdx;
	ulong_ptr gsp=noir_svm_vmread(vcpu->vmcb.virt,guest_rsp);
	ulong_ptr gip=noir_svm_vmread(vcpu->vmcb.virt,guest_rip);
	ulong_ptr gcr3=noir_svm_vmread(vcpu->vmcb.virt,guest_cr3);
	unref_var(context);
	switch(vmmcall_func)
	{
		case noir_svm_callexit:
		{
			// Validate the caller to prevent malicious unloading request.
			// Layered hypervisor cannot unload NoirVisor.
			if(gip>=hvm_p->hv_image.base && gip<hvm_p->hv_image.base+hvm_p->hv_image.size)
			{
				// Directly use space from the starting stack position.
				// Normally it is unused.
				noir_gpr_state_p saved_state=(noir_gpr_state_p)vcpu->hv_stack;
				noir_svm_stgi();
				// Before Debug-Print, GIF should be set because Debug-Printing requires IPI not to be blocked.
				nv_dprintf("VMM-Call for Restoration is intercepted. Exiting...\n");
				// Copy state.
				noir_movsp(saved_state,gpr_state,sizeof(void*)*2);
				saved_state->rax=noir_svm_vmread(vcpu->vmcb.virt,next_rip);
				saved_state->rcx=noir_svm_vmread(vcpu->vmcb.virt,guest_rflags);
				saved_state->rdx=gsp;
				// Restore processor's hidden state.
				noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,(u64)orig_system_call);
				noir_svm_vmload((ulong_ptr)vcpu->vmcb.phys);
				// Switch to Restored CR3
				noir_writecr3(gcr3);
				// Mark the processor is in transition mode.
				vcpu->status=noir_virt_trans;
				// Return to the caller at Host Mode.
				nvc_svm_return(saved_state);
				// Never reaches here.
			}
			// If execution goes here, then the invoker is malicious.
			// Just let the Guest know this is an invalid instruction.
			else
				noir_svm_inject_event(vcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
			break;
		}
		case noir_svm_disasm_length:
		{
#if defined(_hv_type1)
#else
			noir_disasm_request_p disasm=(noir_disasm_request_p)context;
			// Get the page table base that maps both kernel mode address space
			// and the user mode address space for the invoker's process.
			// DO NOT USE THE GUEST CR3 FIELD IN VMCB!
			u64 gcr3k=noir_get_current_process_cr3();
			noir_writecr3(gcr3k);			// Switch to the Guest Address Space.
			disasm->instruction_length=noir_get_instruction_length_ex(disasm->buffer,disasm->bits);
			noir_writecr3(system_cr3);		// Switch back to Host Address Space.
#endif
			break;
		}
		case noir_svm_disasm_mnemonic:
		{
#if defined(_hv_type1)
#else
			noir_disasm_request_p disasm=(noir_disasm_request_p)context;
			// Get the page table base that maps both kernel mode address space
			// and the user mode address space for the invoker's process.
			// DO NOT USE THE GUEST CR3 FIELD IN VMCB!
			u64 gcr3k=noir_get_current_process_cr3();
			noir_writecr3(gcr3k);			// Switch to the Guest Address Space.
			disasm->instruction_length=noir_disasm_instruction(disasm->buffer,disasm->mnemonic,disasm->mnemonic_limit,disasm->bits,disasm->va);
			noir_writecr3(system_cr3);		// Switch back to Host Address Space.
#endif
			break;
		}
		case noir_svm_init_custom_vmcb:
		{
			// Validate the caller. Only Layered Hypervisor is authorized to invoke CVM hypercalls.
			if(gip>=hvm_p->layered_hv_image.base && gip<hvm_p->layered_hv_image.base+hvm_p->layered_hv_image.size)
			{
#if defined(_hv_type1)
				// FIXME: Translate GVAs in the structure .
				noir_svm_custom_vcpu_p cvcpu=null;
#else
				noir_svm_custom_vcpu_p cvcpu=(noir_svm_custom_vcpu_p)context;
#endif
				nvc_svm_initialize_cvm_vmcb(cvcpu);
			}
			else
				noir_svm_inject_event(vcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
			break;
		}
		case noir_svm_run_custom_vcpu:
		{

			// Validate the caller. Only Layered Hypervisor is authorized to invoke CVM hypercalls.
			if(gip>=hvm_p->layered_hv_image.base && gip<hvm_p->layered_hv_image.base+hvm_p->layered_hv_image.size)
			{
#if defined(_hv_type1)
				// FIXME: Translate GVAs in the structure .
				noir_svm_custom_vcpu_p cvcpu=null;
#else
				noir_svm_custom_vcpu_p cvcpu=(noir_svm_custom_vcpu_p)context;
#endif
				nvc_svm_switch_to_guest_vcpu(gpr_state,vcpu,cvcpu);
			}
			else
				noir_svm_inject_event(vcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
			break;
		}
		default:
		{
			// This function leaf is unknown. Treat it as an invalid instruction.
			noir_svm_inject_event(vcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
			break;
		}
	}
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// Expected Intercept Code: 0x82
void static fastcall nvc_svm_vmload_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// Get Address of Nested VMCB.
		const ulong_ptr nested_vmcb_pa=gpr_state->rax;
		void* nested_vmcb=noir_find_virt_by_phys(nested_vmcb_pa);
		// Load to Current VMCB - FS.
		noir_svm_vmwrite16(vmcb,guest_fs_selector,noir_svm_vmread16(nested_vmcb,guest_fs_selector));
		noir_svm_vmwrite16(vmcb,guest_fs_attrib,noir_svm_vmread16(nested_vmcb,guest_fs_attrib));
		noir_svm_vmwrite32(vmcb,guest_fs_limit,noir_svm_vmread32(nested_vmcb,guest_fs_limit));
		noir_svm_vmwrite64(vmcb,guest_fs_base,noir_svm_vmread64(nested_vmcb,guest_fs_base));
		// Load to Current VMCB - GS.
		noir_svm_vmwrite16(vmcb,guest_gs_selector,noir_svm_vmread16(nested_vmcb,guest_gs_selector));
		noir_svm_vmwrite16(vmcb,guest_gs_attrib,noir_svm_vmread16(nested_vmcb,guest_gs_attrib));
		noir_svm_vmwrite32(vmcb,guest_gs_limit,noir_svm_vmread32(nested_vmcb,guest_gs_limit));
		noir_svm_vmwrite64(vmcb,guest_gs_base,noir_svm_vmread64(nested_vmcb,guest_gs_base));
		// Load to Current VMCB - TR.
		noir_svm_vmwrite16(vmcb,guest_tr_selector,noir_svm_vmread16(nested_vmcb,guest_tr_selector));
		noir_svm_vmwrite16(vmcb,guest_tr_attrib,noir_svm_vmread16(nested_vmcb,guest_tr_attrib));
		noir_svm_vmwrite32(vmcb,guest_tr_limit,noir_svm_vmread32(nested_vmcb,guest_tr_limit));
		noir_svm_vmwrite64(vmcb,guest_tr_base,noir_svm_vmread64(nested_vmcb,guest_tr_base));
		// Load to Current VMCB - LDTR.
		noir_svm_vmwrite16(vmcb,guest_ldtr_selector,noir_svm_vmread16(nested_vmcb,guest_ldtr_selector));
		noir_svm_vmwrite16(vmcb,guest_ldtr_attrib,noir_svm_vmread16(nested_vmcb,guest_ldtr_attrib));
		noir_svm_vmwrite32(vmcb,guest_ldtr_limit,noir_svm_vmread32(nested_vmcb,guest_ldtr_limit));
		noir_svm_vmwrite64(vmcb,guest_ldtr_base,noir_svm_vmread64(nested_vmcb,guest_ldtr_base));
		// Load to Current VMCB - MSR.
		noir_svm_vmwrite64(vmcb,guest_sysenter_cs,noir_svm_vmread64(nested_vmcb,guest_sysenter_cs));
		noir_svm_vmwrite64(vmcb,guest_sysenter_esp,noir_svm_vmread64(nested_vmcb,guest_sysenter_esp));
		noir_svm_vmwrite64(vmcb,guest_sysenter_eip,noir_svm_vmread64(nested_vmcb,guest_sysenter_eip));
		noir_svm_vmwrite64(vmcb,guest_star,noir_svm_vmread64(nested_vmcb,guest_star));
		noir_svm_vmwrite64(vmcb,guest_lstar,noir_svm_vmread64(nested_vmcb,guest_lstar));
		noir_svm_vmwrite64(vmcb,guest_cstar,noir_svm_vmread64(nested_vmcb,guest_cstar));
		noir_svm_vmwrite64(vmcb,guest_sfmask,noir_svm_vmread64(nested_vmcb,guest_sfmask));
		noir_svm_vmwrite64(vmcb,guest_kernel_gs_base,noir_svm_vmread64(nested_vmcb,guest_kernel_gs_base));
		// Everything are loaded to Current VMCB. Return to guest.
		noir_svm_advance_rip(vmcb);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x83
void static fastcall nvc_svm_vmsave_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// Get Address of Nested VMCB.
		const ulong_ptr nested_vmcb_pa=gpr_state->rax;
		void* nested_vmcb=noir_find_virt_by_phys(nested_vmcb_pa);
		// Save to Nested VMCB - FS.
		noir_svm_vmwrite16(nested_vmcb,guest_fs_selector,noir_svm_vmread16(vmcb,guest_fs_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_fs_attrib,noir_svm_vmread16(vmcb,guest_fs_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_fs_limit,noir_svm_vmread32(vmcb,guest_fs_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_fs_base,noir_svm_vmread64(vmcb,guest_fs_base));
		// Save to Nested VMCB - GS.
		noir_svm_vmwrite16(nested_vmcb,guest_gs_selector,noir_svm_vmread16(vmcb,guest_gs_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_gs_attrib,noir_svm_vmread16(vmcb,guest_gs_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_gs_limit,noir_svm_vmread32(vmcb,guest_gs_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_gs_base,noir_svm_vmread64(vmcb,guest_gs_base));
		// Save to Nested VMCB - TR.
		noir_svm_vmwrite16(nested_vmcb,guest_tr_selector,noir_svm_vmread16(vmcb,guest_tr_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_tr_attrib,noir_svm_vmread16(vmcb,guest_tr_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_tr_limit,noir_svm_vmread32(vmcb,guest_tr_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_tr_base,noir_svm_vmread64(vmcb,guest_tr_base));
		// Save to Nested VMCB - LDTR.
		noir_svm_vmwrite16(nested_vmcb,guest_ldtr_selector,noir_svm_vmread16(vmcb,guest_ldtr_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_ldtr_attrib,noir_svm_vmread16(vmcb,guest_ldtr_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_ldtr_limit,noir_svm_vmread32(vmcb,guest_ldtr_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_ldtr_base,noir_svm_vmread64(vmcb,guest_ldtr_base));
		// Save to Nested VMCB - MSR.
		noir_svm_vmwrite64(nested_vmcb,guest_sysenter_cs,noir_svm_vmread64(vmcb,guest_sysenter_cs));
		noir_svm_vmwrite64(nested_vmcb,guest_sysenter_esp,noir_svm_vmread64(vmcb,guest_sysenter_esp));
		noir_svm_vmwrite64(nested_vmcb,guest_sysenter_eip,noir_svm_vmread64(vmcb,guest_sysenter_eip));
		noir_svm_vmwrite64(nested_vmcb,guest_star,noir_svm_vmread64(vmcb,guest_star));
		noir_svm_vmwrite64(nested_vmcb,guest_lstar,noir_svm_vmread64(vmcb,guest_lstar));
		noir_svm_vmwrite64(nested_vmcb,guest_cstar,noir_svm_vmread64(vmcb,guest_cstar));
		noir_svm_vmwrite64(nested_vmcb,guest_sfmask,noir_svm_vmread64(vmcb,guest_sfmask));
		noir_svm_vmwrite64(nested_vmcb,guest_kernel_gs_base,noir_svm_vmread64(vmcb,guest_kernel_gs_base));
		// Everything are saved to Nested VMCB. Return to guest.
		noir_svm_advance_rip(vmcb);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x84
void static fastcall nvc_svm_stgi_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// If the code reaches here, there is no hardware support for vGIF.
		vcpu->nested_hvm.gif=1;		// Marks that GIF is set in Guest.
		// FIXME: Inject pending interrupts held due to cleared GIF, and clear interceptions on certain interrupts.
		noir_svm_advance_rip(vmcb);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x85
void static fastcall nvc_svm_clgi_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// If the code reaches here, there is no hardware support for vGIF.
		vcpu->nested_hvm.gif=0;		// Marks that GIF is reset in Guest.
		// FIXME: Setup interceptions on certain interrupts.
		noir_svm_advance_rip(vmcb);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x86
void static fastcall nvc_svm_skinit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// No skinit support in NoirVisor Guest.
	noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
}

// Expected Intercept Code: 0x400
// Do not output to debugger since this may seriously degrade performance.
void static fastcall nvc_svm_nested_pf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	bool advance=true;
	// Necessary Information for #NPF VM-Exit.
	amd64_npt_fault_code fault;
	fault.value=noir_svm_vmread64(vcpu->vmcb.virt,exit_info1);
#if !defined(_hv_type1)
	if(fault.execute)
	{
		i32 lo=0,hi=noir_hook_pages_count;
		u64 gpa=noir_svm_vmread64(vcpu->vmcb.virt,exit_info2);
		// Check if we should switch to secondary.
		// Use binary search to reduce searching time complexity.
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
				noir_npt_manager_p nptm=(noir_npt_manager_p)vcpu->secondary_nptm;
				noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
				advance=false;
				break;
			}
		}
		if(advance)
		{
			// Execution is outside hooked page.
			// We should switch to primary.
			noir_npt_manager_p nptm=(noir_npt_manager_p)vcpu->primary_nptm;
			noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
			advance=false;
		}
		// We switched NPT. Thus we should clean VMCB cache state.
		noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_npt);
		// It is necessary to flush TLB.
		noir_svm_vmwrite8(vcpu->vmcb.virt,tlb_control,nvc_svm_tlb_control_flush_entire);
	}
#endif
	if(advance)
	{
		// Note that SVM won't save the next rip in #NPF.
		// Hence we should advance rip by software analysis.
		// Usually, if #NPF handler goes here, it might be induced by Hardware-Enforced CI.
		// In this regard, we assume this instruction is writing protected page.
		void* instruction=(void*)((ulong_ptr)vcpu->vmcb.virt+guest_instruction_bytes);
		// Determine Long-Mode through CS.L bit.
		u16* cs_attrib=(u16*)((ulong_ptr)vcpu->vmcb.virt+guest_cs_attrib);
		u32 increment=noir_get_instruction_length(instruction,noir_bt(cs_attrib,9));
		// Just increment the rip. Don't emulate a read/write for guest.
		ulong_ptr gip=noir_svm_vmread(vcpu->vmcb.virt,guest_rip);
		gip+=increment;
		noir_svm_vmwrite(vcpu->vmcb.virt,guest_rip,gip);
	}
}

void fastcall nvc_svm_exit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	// Get the linear address of VMCB.
	noir_svm_initial_stack_p loader_stack=(noir_svm_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_svm_initial_stack));
	// Confirm which vCPU is exiting so that the correct handler is to be invoked...
	if(likely(gpr_state->rax==vcpu->vmcb.phys))		// Let branch predictor favor subverted host.
	{
		// Subverted Host is exiting...
		const void* vmcb_va=vcpu->vmcb.virt;
		// Read the Intercept Code.
		i64 intercept_code=noir_svm_vmread64(vmcb_va,exit_code);
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
		if(unlikely(intercept_code<0))		// Rare circumstance.
			svm_exit_handler_negative[-intercept_code-1](gpr_state,vcpu);
		else
			svm_exit_handlers[code_group][code_num](gpr_state,vcpu);
		// Since rax register is operated, save to VMCB.
		noir_svm_vmwrite(vmcb_va,guest_rax,gpr_state->rax);
	}
	else if(gpr_state->rax==loader_stack->custom_vcpu->vmcb.phys)
	{
		// Customizable VM is exiting...
		noir_svm_custom_vcpu_p cvcpu=loader_stack->custom_vcpu;
		const void* vmcb_va=cvcpu->vmcb.virt;
		// Read the Intercept Code.
		i64 intercept_code=noir_svm_vmread64(vmcb_va,exit_code);
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
		// Set General vCPU Exit Context.
		cvcpu->header.exit_context.vcpu_state.instruction_length=(noir_svm_vmread64(vmcb_va,next_rip)-noir_svm_vmread64(vmcb_va,guest_rip))&0xf;
		cvcpu->header.exit_context.vcpu_state.cpl=noir_svm_vmread8(vmcb_va,guest_cpl)&0x3;
		cvcpu->header.exit_context.vcpu_state.int_shadow=noir_svm_vmcb_bt32(vmcb_va,guest_interrupt,0);
		cvcpu->header.exit_context.vcpu_state.pe=noir_svm_vmcb_bt32(vmcb_va,guest_cr0,amd64_cr0_pe);
		cvcpu->header.exit_context.vcpu_state.lm=noir_svm_vmcb_bt32(vmcb_va,guest_efer,amd64_efer_lma);
		// Check if the interception is due to invalid guest state.
		// Invoke the handler accordingly.
		if(unlikely(intercept_code<0))		// Rare circumstance.
			svm_cvexit_handler_negative[-intercept_code-1](gpr_state,vcpu,loader_stack->custom_vcpu);
		else
			svm_cvexit_handlers[code_group][code_num](gpr_state,vcpu,loader_stack->custom_vcpu);
		// Since rax register is operated, save to VMCB.
		noir_svm_vmwrite(vmcb_va,guest_rax,gpr_state->rax);
	}
	else
	{
		// Nested VM is exiting...
		noir_svm_stgi();
		nv_dprintf("VM-Exit from Nested VM is intercepted!\n");
		noir_int3();
	}
	// The rax in GPR state should be the physical address of VMCB
	// in order to execute the vmrun instruction properly.
	// Reading/Writing the rax is like the vmptrst/vmptrld instruction in Intel VT-x.
	gpr_state->rax=(ulong_ptr)loader_stack->guest_vmcb_pa;
}

void nvc_svm_set_mshv_handler(bool option)
{
	nvcp_svm_cpuid_handler=option?nvc_svm_cpuid_hvp_handler:nvc_svm_cpuid_hvs_handler;
}

// Prior to calling this function, it is required to setup guest state fields.
void nvc_svm_reconfigure_npiep_interceptions(noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	ulong_ptr gcr4=noir_svm_vmread(vmcb,guest_cr4);
	nvc_svm_instruction_intercept1 list1;
	list1.value=noir_svm_vmread32(vmcb,intercept_instruction1);
	if(noir_bt(&gcr4,amd64_cr4_umip))
	{
		// If UMIP is to be enabled, stop any NPIEP-related interceptions.
		list1.intercept_sidt=0;
		list1.intercept_sgdt=0;
		list1.intercept_sldt=0;
		list1.intercept_str=0;
	}
	else
	{
		// Otherwise, configure the interception according to NPIEP configuration.
		list1.intercept_sidt=noir_bt64(&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sidt);
		list1.intercept_sgdt=noir_bt64(&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sgdt);
		list1.intercept_sldt=noir_bt64(&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_sldt);
		list1.intercept_str=noir_bt64(&vcpu->mshvcpu.npiep_config,noir_mshv_npiep_prevent_str);
	}
	noir_svm_vmwrite32(vmcb,intercept_instruction1,list1.value);
	// Finally, invalidate VMCB Cache regarding interceptions.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_interception);
}