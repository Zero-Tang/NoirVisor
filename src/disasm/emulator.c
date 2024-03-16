/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the emulator of NoirVisor, based on Zydis Library.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /disasm/emulator.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <Zydis/Zydis.h>
#include "emulator.h"

ZydisDecodedOperand* nvc_emu_get_read_operand(ZydisDecodedInstruction *Instruction,ZydisDecodedOperand *Operands)
{
	for(ZyanU8 i=0;i<Instruction->operand_count;i++)
		if(Operands[i].actions & ZYDIS_OPERAND_ACTION_READ)
			return &Operands[i];
	return null;
}

ZydisDecodedOperand* nvc_emu_get_write_operand(ZydisDecodedInstruction *Instruction,ZydisDecodedOperand *Operands)
{
	for(ZyanU8 i=0;i<Instruction->operand_count;i++)
		if(Operands[i].actions & ZYDIS_OPERAND_ACTION_WRITE)
			return &Operands[i];
	return null;
}

ZydisDecodedOperand* nvc_emu_get_explicit_operand(ZydisDecodedInstruction *Instruction,ZydisDecodedOperand *Operands,u8 index)
{
	u8 acc=0;
	for(ZyanU8 i=0;i<Instruction->operand_count;i++)
		if(Operands[i].visibility==ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
			if(acc++==index)
				return &Operands[i];
	return null;
}

// Please note that "ZydisCalcAbsoluteAddress" can't do the work for us.
u64 static nvc_emu_calculate_absolute_address(noir_cvm_virtual_cpu_p vcpu,ZydisDecodedInstruction *Instruction,ZydisDecodedOperand *Operand)
{
	// Address width may not be full.
	u64 addr_mask=(1ui64<<Instruction->address_width)-1;
	// Generally, an memory operand is referenced by the following format:
	// AbsoluteAddress=SegmentBase+BaseRegister+IndexRegister*ScalingFactor+Displacement
	// None of them are required to be present.
	u64p gpr=(u64p)&vcpu->gpr;
	segment_register_p segs=(segment_register_p)&vcpu->seg;
	u64 abs_addr=segs[Operand->mem.segment-ZYDIS_REGISTER_ES].base;
	// Base Register...
	if(Operand->mem.base>=ZYDIS_REGISTER_AX && Operand->mem.base<=ZYDIS_REGISTER_R15W)
		abs_addr+=(u16)gpr[Operand->mem.base-ZYDIS_REGISTER_AX];		// 16-Bit GPRs
	else if(Operand->mem.base>=ZYDIS_REGISTER_EAX && Operand->mem.base<=ZYDIS_REGISTER_R15D)
		abs_addr+=(u32)gpr[Operand->mem.base-ZYDIS_REGISTER_EAX];		// 32-Bit GPRs
	else if(Operand->mem.base>=ZYDIS_REGISTER_RAX && Operand->mem.base<=ZYDIS_REGISTER_R15)
		abs_addr+=(u64)gpr[Operand->mem.base-ZYDIS_REGISTER_RAX];		// 64-Bit GPRs
	// Note that there could be RIP-relative addressing.
	else if(Operand->mem.base==ZYDIS_REGISTER_EIP)
		abs_addr+=(u32)vcpu->exit_context.rip+Instruction->length;
	else if(Operand->mem.base==ZYDIS_REGISTER_RIP)
		abs_addr+=(u64)vcpu->exit_context.rip+Instruction->length;
	// Index and Scale...
	if(Operand->mem.index>=ZYDIS_REGISTER_AX && Operand->mem.index<=ZYDIS_REGISTER_R15W)
		abs_addr+=(u16)gpr[Operand->mem.index-ZYDIS_REGISTER_AX]*Operand->mem.scale;		// 16-Bit GPRs
	else if(Operand->mem.index>=ZYDIS_REGISTER_EAX && Operand->mem.index<=ZYDIS_REGISTER_R15D)
		abs_addr+=(u32)gpr[Operand->mem.index-ZYDIS_REGISTER_EAX]*Operand->mem.scale;		// 32-Bit GPRs
	else if(Operand->mem.index>=ZYDIS_REGISTER_RAX && Operand->mem.index<=ZYDIS_REGISTER_R15)
		abs_addr+=(u64)gpr[Operand->mem.index-ZYDIS_REGISTER_RAX]*Operand->mem.scale;		// 64-Bit GPRs
	// Displacement
	if(Operand->mem.disp.has_displacement)abs_addr+=Operand->mem.disp.value;
	return abs_addr&addr_mask;
}

noir_status nvc_emu_decode_memory_access(noir_cvm_virtual_cpu_p vcpu)
{
	noir_status st=noir_invalid_parameter;
	noir_cvm_memory_access_context_p mem_ctxt=&vcpu->exit_context.memory_access;
	if(vcpu->exit_context.intercept_code==cv_memory_access && mem_ctxt->access.execute==false)
	{
		ZydisDecodedInstruction ZyIns;
		ZydisDecodedOperand ZyOps[ZYDIS_MAX_OPERAND_COUNT];
		ZydisDecoder *SelectedDecoder;
		ZyanStatus zst;
		st=noir_unsuccessful;
		if(noir_bt(&vcpu->exit_context.cs.attrib,13))		// Long Mode?
			SelectedDecoder=&ZyDec64;
		else if(noir_bt(&vcpu->exit_context.cs.attrib,14))	// Default-Big?
			SelectedDecoder=&ZyDec32;
		else
			SelectedDecoder=&ZyDec16;
		zst=ZydisDecoderDecodeFull(SelectedDecoder,vcpu->exit_context.memory_access.instruction_bytes,15,&ZyIns,ZyOps);
		if(ZYAN_SUCCESS(zst))
		{
			ZydisOperandAction mmio_op_action=mem_ctxt->access.write?ZYDIS_OPERAND_ACTION_WRITE:ZYDIS_OPERAND_ACTION_READ;
			ZydisOperandAction mmio_an_action=mem_ctxt->access.write?ZYDIS_OPERAND_ACTION_READ:ZYDIS_OPERAND_ACTION_WRITE;
			for(u8 i=0;i<ZyIns.operand_count;i++)
			{
				if(ZyOps[i].actions & mmio_op_action)
				{
					// Set the GVA.
					if(ZyOps[i].type==ZYDIS_OPERAND_TYPE_MEMORY)
					{
						u64 address=nvc_emu_calculate_absolute_address(vcpu,&ZyIns,&ZyOps[i]);
						// Does the GVA have the same offset to the GPA?
						if(page_4kb_offset(address)==page_4kb_offset(mem_ctxt->gpa))
						{
							// Actually, I'm not sure if there are potential bugs.
							// Let me know if there could be such an instruction.
							mem_ctxt->gva=address;
							mem_ctxt->flags.operand_size=ZyOps[i].size>>3;
							st=noir_success;
							continue;
						}
					}
					else
					{
						char ins_byte_str[48],mnemonic[64];
						for(u8 j=0;j<ZyIns.length;j++)
							nv_snprintf(&ins_byte_str[j*3],sizeof(ins_byte_str)-(j*3),"%02X ",vcpu->exit_context.memory_access.instruction_bytes[j]);
						ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,ZyOps,ZYDIS_MAX_OPERAND_COUNT,mnemonic,sizeof(mnemonic),vcpu->exit_context.rip,ZYAN_NULL);
						nvd_printf("[CVM MMIO] Operand %u is not memory! (0x%016llX %s\t %s)\n",i,vcpu->exit_context.rip,ins_byte_str,mnemonic);
						noir_int3();
					}
				}
				else if(ZyOps[i].actions & mmio_an_action)
				{
					// Decode the operand intended for MMIO operation.
					switch(ZyOps[i].type)
					{
						case ZYDIS_OPERAND_TYPE_REGISTER:
						{
							ZydisRegister an_reg=ZyOps[i].reg.value;
							if(an_reg>=ZYDIS_REGISTER_AL && an_reg<=ZYDIS_REGISTER_R15)
							{
								// General-Purpose Registers...
								mem_ctxt->flags.operand_class=noir_cvm_operand_class_gpr;
								if(an_reg>=ZYDIS_REGISTER_AL && an_reg<=ZYDIS_REGISTER_BL)
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_AL;
								else if(an_reg>=ZYDIS_REGISTER_AH && an_reg<=ZYDIS_REGISTER_BH)
								{
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_AH;
									mem_ctxt->flags.operand_class=noir_cvm_operand_class_gpr8hi;
								}
								else if(an_reg>=ZYDIS_REGISTER_SPL && an_reg<=ZYDIS_REGISTER_R15B)
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_SPL+4;
								else if(an_reg>=ZYDIS_REGISTER_AX && an_reg<=ZYDIS_REGISTER_R15W)
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_AX;
								else if(an_reg>=ZYDIS_REGISTER_EAX && an_reg<=ZYDIS_REGISTER_R15D)
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_EAX;
								else
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_RAX;
							}
							else if(an_reg>=ZYDIS_REGISTER_ST0 && an_reg<=ZYDIS_REGISTER_ST7)
							{
								// Floating-Point Registers...
								mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_ST0;
								mem_ctxt->flags.operand_class=noir_cvm_operand_class_fpr;
							}
							else if(an_reg>=ZYDIS_REGISTER_MM0 && an_reg<=ZYDIS_REGISTER_MM7)
							{
								// Each Multimedia Register is the lower 64 bits of the 80-bit Floating-Point Register.
								mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_MM0;
								mem_ctxt->flags.operand_class=noir_cvm_operand_class_fpr;
							}
							else if(an_reg>=ZYDIS_REGISTER_XMM0 && an_reg<=ZYDIS_REGISTER_ZMM31)
							{
								// Single-Instruction, Multiple-Data Registers...
								if(an_reg>=ZYDIS_REGISTER_XMM0 && an_reg<=ZYDIS_REGISTER_XMM31)
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_XMM0;
								else if(an_reg>=ZYDIS_REGISTER_YMM0 && an_reg<=ZYDIS_REGISTER_YMM31)
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_YMM0;
								else if(an_reg>=ZYDIS_REGISTER_ZMM0 && an_reg<=ZYDIS_REGISTER_ZMM31)
									mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_ZMM0;
								mem_ctxt->flags.operand_class=noir_cvm_operand_class_simd;
							}
							else if(an_reg>=ZYDIS_REGISTER_ES && an_reg<=ZYDIS_REGISTER_GS)
							{
								// Segment Selectors...
								mem_ctxt->flags.operand_code=an_reg-ZYDIS_REGISTER_ES;
								mem_ctxt->flags.operand_class=noir_cvm_operand_class_segsel;
							}
							else
								mem_ctxt->flags.operand_class=noir_cvm_operand_class_unknown;
							break;
						}
						case ZYDIS_OPERAND_TYPE_MEMORY:
						{
							mem_ctxt->flags.operand_class=noir_cvm_operand_class_memory;
							mem_ctxt->operand.mem=nvc_emu_calculate_absolute_address(vcpu,&ZyIns,&ZyOps[i]);
							break;
						}
						case ZYDIS_OPERAND_TYPE_POINTER:
						{
							mem_ctxt->flags.operand_class=noir_cvm_operand_class_farptr;
							mem_ctxt->operand.far.segment=ZyOps[i].ptr.segment;
							mem_ctxt->operand.far.offset=ZyOps[i].ptr.offset;
							mem_ctxt->operand.far.reserved0=0;
							mem_ctxt->operand.far.reserved1=0;
							break;
						}
						case ZYDIS_OPERAND_TYPE_IMMEDIATE:
						{
							mem_ctxt->flags.operand_class=noir_cvm_operand_class_immediate;
							mem_ctxt->operand.imm.is_signed=ZyOps[i].imm.is_signed;
							mem_ctxt->operand.imm.u=ZyOps[i].imm.value.u;
							break;
						}
						default:
						{
							mem_ctxt->flags.operand_class=noir_cvm_operand_class_unknown;
							break;
						}
					}
					continue;
				}
			}
			// Decode the identity of the MMIO instruction
			switch(ZyIns.mnemonic)
			{
				case ZYDIS_MNEMONIC_MOV:
				{
					mem_ctxt->flags.instruction_code=noir_cvm_instruction_code_mov;
					break;
				}
				default:
				{
					mem_ctxt->flags.instruction_code=noir_cvm_instruction_code_unknown;
					break;
				}
			}
			vcpu->exit_context.vcpu_state.instruction_length=ZyIns.length;
			vcpu->exit_context.next_rip=vcpu->exit_context.rip+ZyIns.length;
			// Reset the higher 32 bits of the advanced rip if the guest is not in long mode.
			if(noir_bt(&vcpu->exit_context.cs.selector,13)==false)vcpu->exit_context.next_rip&=maxu32;
			// Mark the decoder has completed operation.
			mem_ctxt->flags.decoded=true;
		}
		else
		{
			u8 mode=(noir_bt(&vcpu->exit_context.cs.attrib,13)<<2)+(noir_bt(&vcpu->exit_context.cs.attrib,14)<<1);
			char ins_byte_str[48];
			for(u8 j=0;j<15;j++)
				nv_snprintf(&ins_byte_str[j*3],sizeof(ins_byte_str)-(j*3),"%02X ",vcpu->exit_context.memory_access.instruction_bytes[j]);
			if(mode==0)mode=1;
			nvd_printf("[CVM MMIO] Failed to decode %u-bit instruction! Instruction Bytes: %s\n",mode<<4,ins_byte_str);
			noir_int3();
		}
	}
	return st;
}

/*
  Emulating Instructions for Subverted Host:
  
  According to Microsoft Hypervisor Top-Level Functionality Specification,
  only a few instructions are supported for Local-APIC memory-accesses.

  89 /r		mov m32,r32
  8B /r		mov r32,m32
  A1 disp	mov eax,moffset32
  A3 disp	mov moffset32,eax
  C7 /0		mov m32,imm32
  FF /6		push m32
*/

// This emulator function works only in subverted host.
u8 noir_hvcode nvc_emu_try_vmexit_write_memory(noir_gpr_state_p gpr_state,noir_seg_state_p seg_state,u8p instruction,void* operand,size_t *size)
{
	ulong_ptr *gpr=(ulong_ptr*)gpr_state;
	ZyanStatus zst;
	ZydisDecoder *SelectedDecoder;
	ZydisDecodedInstruction ZyIns;
	ZydisDecodedOperand ZyOps[ZYDIS_MAX_OPERAND_COUNT];
	if(noir_bt(&seg_state->cs.attrib,9))		// Long Mode?
		SelectedDecoder=&ZyDec64;
	else if(noir_bt(&seg_state->cs.attrib,10))	// Default-Big?
		SelectedDecoder=&ZyDec32;
	else
		SelectedDecoder=&ZyDec16;
	zst=ZydisDecoderDecodeFull(SelectedDecoder,instruction,15,&ZyIns,ZyOps);
	if(ZYAN_SUCCESS(zst))
	{
		ZydisDecodedOperand *ZyOp=nvc_emu_get_read_operand(&ZyIns,ZyOps);
		*size=ZyIns.operand_width>>3;
		switch(ZyIns.mnemonic)
		{
			case ZYDIS_MNEMONIC_MOV:
			{
				switch(ZyOp->type)
				{
					case ZYDIS_OPERAND_TYPE_REGISTER:
					{
						if(ZyOp->reg.value>=ZYDIS_REGISTER_EAX && ZyOp->reg.value<=ZYDIS_REGISTER_R15D)
						{
							*(u32p)operand=(u32)gpr[ZyOp->reg.value-ZYDIS_REGISTER_EAX];
							return ZyIns.length;
						}
						else
						{
							nvd_printf("[Emulator] Only 32-bit source operand is supported!\n");
							return ZyIns.length;
						}
						break;
					}
					case ZYDIS_OPERAND_TYPE_IMMEDIATE:
					{
						nvd_printf("[Emulator] Source operand register is immediate number! Value=0x%llX\n",ZyOp->imm.value);
						noir_movsb(operand,&ZyOp->imm.value,*size);
						return ZyIns.length;
						break;
					}
					default:
					{
						nvd_printf("[Emulator] Source operand register is unknown!\n");
						break;
					}
				}
				break;
			}
			case ZYDIS_MNEMONIC_PUSH:
			{
				nvd_printf("[Emulator] push instruction is unsupported!\n");
				break;
			}
			default:
			{
				char raw_ins_str[48];
				char fmt_ins_str[64];
				ZydisDecodedOperand *ZyOp=nvc_emu_get_read_operand(&ZyIns,ZyOps);
				for(u8 i=0;i<ZyIns.length;i++)
					nv_snprintf(&raw_ins_str[i*3],sizeof(raw_ins_str)-(i*3),"%02X ",instruction[i]);
				ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,ZyOps,ZYDIS_MAX_OPERAND_COUNT,fmt_ins_str,sizeof(fmt_ins_str),ZYDIS_RUNTIME_ADDRESS_NONE,ZYAN_NULL);
				nvd_printf("[Emulator] Unknown Instruction: %s\t%s\n",raw_ins_str,fmt_ins_str);
				break;
			}
		}
	}
	return 0;
}

// This is required for emulating rsm instruction in Intel VT-x.
// Intel VT-x can only intercept rsm instruction by causing #UD exceptions to exit and decode the triggering exception.
u8 noir_hvcode nvc_emu_is_rsm_instruction(u8p buffer,size_t buffer_limit)
{
	ZydisDecodedInstruction ZyIns;
	// The decoding of the rsm instruction is universal on all modes,
	// so just pick any one of the decoder.
	ZyanStatus zst=ZydisDecoderDecodeInstruction(&ZyDec64,ZYAN_NULL,buffer,buffer_limit,&ZyIns);
	if(ZYAN_SUCCESS(zst))
		if(ZyIns.mnemonic==ZYDIS_MNEMONIC_RSM)
			return ZyIns.length;
	return 0;
}

// This is required for emulating NPIEP-related instructions in AMD-V.
// AMD-V does not decode these instructions upon interceptions.
u8 noir_hvcode nvc_emu_decode_npiep_instruction(noir_cvm_virtual_cpu_p vcpu,u8p buffer,size_t buffer_limit,noir_npiep_operand_p operand)
{
	ulong_ptr *gpr_state=(ulong_ptr*)&vcpu->gpr;
	ZyanStatus zst;
	ZydisDecoder *SelectedDecoder;
	ZydisDecodedInstruction ZyIns;
	ZydisDecodedOperand ZyOps[ZYDIS_MAX_OPERAND_COUNT];
	if(noir_bt(&vcpu->seg.cs.attrib,13))		// Long Mode?
		SelectedDecoder=&ZyDec64;
	else if(noir_bt(&vcpu->seg.cs.attrib,14))	// Default-Big?
		SelectedDecoder=&ZyDec32;
	else
		SelectedDecoder=&ZyDec16;
	zst=ZydisDecoderDecodeFull(SelectedDecoder,buffer,buffer_limit,&ZyIns,ZyOps);
	if(ZYAN_SUCCESS(zst))
	{
		switch(ZyIns.mnemonic)
		{
			case ZYDIS_MNEMONIC_LGDT:
			case ZYDIS_MNEMONIC_LIDT:
			case ZYDIS_MNEMONIC_LLDT:
			case ZYDIS_MNEMONIC_LTR:
			case ZYDIS_MNEMONIC_SGDT:
			case ZYDIS_MNEMONIC_SIDT:
			case ZYDIS_MNEMONIC_SLDT:
			case ZYDIS_MNEMONIC_STR:
			{
				operand->flags.reserved=0;
				if(ZyOps->type==ZYDIS_OPERAND_TYPE_REGISTER)
				{
					ZydisRegister reg=ZyOps->reg.value;
					// We do not expect an 8-bit GPR here.
					if(reg>=ZYDIS_REGISTER_AX && reg<=ZYDIS_REGISTER_R15W)
						operand->flags.reg=reg-ZYDIS_REGISTER_AX;
					else if(reg>=ZYDIS_REGISTER_EAX && reg<=ZYDIS_REGISTER_R15D)
						operand->flags.reg=reg-ZYDIS_REGISTER_EAX;
					else if(reg>=ZYDIS_REGISTER_RAX && reg<=ZYDIS_REGISTER_R15)
						operand->flags.reg=reg-ZYDIS_REGISTER_RAX;
					else
						return 0;
					operand->flags.use_register=true;
				}
				else if(ZyOps->type==ZYDIS_OPERAND_TYPE_MEMORY)
				{
					operand->memory_address=nvc_emu_calculate_absolute_address(vcpu,&ZyIns,ZyOps);
					operand->flags.use_register=false;
				}
				else
					return 0;
				return ZyIns.length;
			}
		}
	}
	return 0;
}