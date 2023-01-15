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

ZydisDecodedOperand* nvc_emu_get_read_operand(ZydisDecodedInstruction *Instruction)
{
	for(ZyanU8 i=0;i<Instruction->operand_count;i++)
		if(Instruction->operands[i].actions & ZYDIS_OPERAND_ACTION_READ)
			return &Instruction->operands[i];
	return null;
}

ZydisDecodedOperand* nvc_emu_get_write_operand(ZydisDecodedInstruction *Instruction)
{
	for(ZyanU8 i=0;i<Instruction->operand_count;i++)
		if(Instruction->operands[i].actions & ZYDIS_OPERAND_ACTION_WRITE)
			return &Instruction->operands[i];
	return null;
}

ZydisDecodedOperand* nvc_emu_get_explicit_operand(ZydisDecodedInstruction *Instruction,u8 index)
{
	u8 acc=0;
	for(ZyanU8 i=0;i<Instruction->operand_count;i++)
		if(Instruction->operands[i].visibility==ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
			if(acc++==index)
				return &Instruction->operands[i];
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
		abs_addr+=(u32)vcpu->exit_context.rip;
	else if(Operand->mem.base==ZYDIS_REGISTER_RIP)
		abs_addr+=(u64)vcpu->exit_context.rip;
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

void static nvc_emu_write_operand(noir_cvm_virtual_cpu_p vcpu,ZydisDecodedInstruction *Instruction,ZydisDecodedOperand *Operand,void* buffer,u64p address)
{
	switch(Operand->type)
	{
		case ZYDIS_OPERAND_TYPE_REGISTER:
		{
			ulong_ptr *gpr=(ulong_ptr*)&vcpu->gpr;
			// Currently, we consider GPRs only.
			if(Operand->reg.value>=ZYDIS_REGISTER_AL && Operand->reg.value<=ZYDIS_REGISTER_BL)
				*(u8p)&gpr[Operand->reg.value-ZYDIS_REGISTER_AL]=*(u8p)buffer;			// 8-Bit Low GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_AH && Operand->reg.value<=ZYDIS_REGISTER_BH)
				((u8p)&gpr[Operand->reg.value-ZYDIS_REGISTER_AH])[1]=*(u8p)buffer;		// 8-Bit High GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_SPL && Operand->reg.value<=ZYDIS_REGISTER_R15B)
				*(u8p)&gpr[Operand->reg.value-ZYDIS_REGISTER_AH]=*(u8p)buffer;			// 8-Bit Extended GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_AX && Operand->reg.value<=ZYDIS_REGISTER_R15W)
				*(u16p)&gpr[Operand->reg.value-ZYDIS_REGISTER_AX]=*(u16p)buffer;		// 16-Bit GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_EAX && Operand->reg.value<=ZYDIS_REGISTER_R15D)
				// Note that for 32-bit accesses, higher 32 bits of a register are always cleared to zero.
				*(u64p)&gpr[Operand->reg.value-ZYDIS_REGISTER_EAX]=(u64)*(u32p)buffer;	// 32-Bit GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_RAX && Operand->reg.value<=ZYDIS_REGISTER_R15)
				*(u64p)&gpr[Operand->reg.value-ZYDIS_REGISTER_RAX]=*(u64p)buffer;		// 64-Bit GPRs
			break;
		}
		case ZYDIS_OPERAND_TYPE_MEMORY:
		{
			// Memory is the hardest part, because we should read from guest memory.
			// Return the address instead.
			*address=nvc_emu_calculate_absolute_address(vcpu,Instruction,Operand);
			break;
		}
	}
}

void static nvc_emu_read_operand(noir_cvm_virtual_cpu_p vcpu,ZydisDecodedInstruction *Instruction,ZydisDecodedOperand *Operand,void* buffer,u64p address)
{
	switch(Operand->type)
	{
		case ZYDIS_OPERAND_TYPE_REGISTER:
		{
			ulong_ptr *gpr=(ulong_ptr*)&vcpu->gpr;
			// Currently, we consider GPRs only.
			if(Operand->reg.value>=ZYDIS_REGISTER_AL && Operand->reg.value<=ZYDIS_REGISTER_BL)
				*(u8p)buffer=(u8)gpr[Operand->reg.value-ZYDIS_REGISTER_AL];			// 8-Bit Low GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_AH && Operand->reg.value<=ZYDIS_REGISTER_BH)
				*(u8p)buffer=(u8)(gpr[Operand->reg.value-ZYDIS_REGISTER_AH]>>8);	// 8-Bit High GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_SPL && Operand->reg.value<=ZYDIS_REGISTER_R15B)
				*(u8p)buffer=(u8)gpr[Operand->reg.value-ZYDIS_REGISTER_AH];			// 8-Bit Extended GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_AX && Operand->reg.value<=ZYDIS_REGISTER_R15W)
				*(u16p)buffer=(u16)gpr[Operand->reg.value-ZYDIS_REGISTER_AX];		// 16-Bit GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_EAX && Operand->reg.value<=ZYDIS_REGISTER_R15D)
				*(u32p)buffer=(u32)gpr[Operand->reg.value-ZYDIS_REGISTER_EAX];		// 32-Bit GPRs
			else if(Operand->reg.value>=ZYDIS_REGISTER_RAX && Operand->reg.value<=ZYDIS_REGISTER_R15)
				*(u64p)buffer=(u64)gpr[Operand->reg.value-ZYDIS_REGISTER_RAX];		// 64-Bit GPRs
			break;
		}
		case ZYDIS_OPERAND_TYPE_MEMORY:
		{
			// Memory is the hardest part, because we should read from guest memory.
			// Return the address instead.
			*address=nvc_emu_calculate_absolute_address(vcpu,Instruction,Operand);
			break;
		}
		case ZYDIS_OPERAND_TYPE_IMMEDIATE:
		{
			// Immediate is the easiest part of decoding.
			u16 size=Operand->size>>3;
			noir_movsb(buffer,&Operand->imm.value,size);
			break;
		}
	}
}

noir_status nvc_emu_try_cvmmio_emulation(noir_cvm_virtual_cpu_p vcpu,noir_emulated_mmio_instruction_p emulation)
{
	noir_status st=noir_emu_unknown_instruction;
	noir_cvm_memory_access_context_p mem_ctxt=&vcpu->exit_context.memory_access;
	ZydisDecodedInstruction ZyIns;
	ZydisDecoder *SelectedDecoder;
	if(noir_bt(&vcpu->exit_context.cs.attrib,13))		// Is this Long Mode?
		SelectedDecoder=&ZyDec64;
	else if(noir_bt(&vcpu->exit_context.cs.attrib,14))	// Is this 32-bit Mode?
		SelectedDecoder=&ZyDec32;
	else
		SelectedDecoder=&ZyDec16;
	if(ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(SelectedDecoder,vcpu->exit_context.memory_access.instruction_bytes,15,&ZyIns)))
	{
		ZydisDecodedOperand *ZyOp;
		emulation->emulation_property.advancement_length=ZyIns.length;
		emulation->emulation_property.access_size=(u64)(ZyIns.operand_width>>3);
		emulation->emulation_property.direction=mem_ctxt->access.write;
		switch(ZyIns.mnemonic)
		{
			// We don't have to emulate all instructions that reference memory.
			// Currently supported instructions:
			// mov, movzx, movsx, push, pop
			case ZYDIS_MNEMONIC_MOV:
			{
				// Note that the mov instruction won't have two memory operands.
				// We are retrieving non-memory operands.
				if(mem_ctxt->access.write)
				{
					// Get the operand to be written to the memory.
					ZyOp=nvc_emu_get_read_operand(&ZyIns);
					// Place the value of the operand to the buffer.
					nvc_emu_read_operand(vcpu,&ZyIns,ZyOp,emulation->data,null);
				}
				else
				{
					// Get the operand to be overwritten by the memory.
					ZyOp=nvc_emu_get_write_operand(&ZyIns);
					// Write the register from the buffer.
					nvc_emu_write_operand(vcpu,&ZyIns,ZyOp,emulation->data,null);
				}
				// Advance the guest rip.
				vcpu->rip=vcpu->exit_context.next_rip;
				vcpu->state_cache.gprvalid=false;
				st=noir_success;
				break;
			}
			case ZYDIS_MNEMONIC_MOVSX:
			case ZYDIS_MNEMONIC_MOVZX:
			{
				// For MMIO, the movsx and movzx instructions always read from memory.
				u64 ext=0,data=*(u64p)emulation->data;
				ZyOp=nvc_emu_get_write_operand(&ZyIns);
				// The movsx must consider sign-extension. Test the top bit in order to extend the sign.
				if(ZyIns.mnemonic==ZYDIS_MNEMONIC_MOVSX && noir_bt64(&data,ZyOp->size-1))ext=maxu64;
				noir_movsb(&ext,emulation->data,emulation->emulation_property.access_size);
				// The sign-extended value is in "ext" variable now.
				nvc_emu_write_operand(vcpu,&ZyIns,ZyOp,&ext,null);
				// Advance the guest rip.
				vcpu->rip=vcpu->exit_context.next_rip;
				vcpu->state_cache.gprvalid=false;
				st=noir_success;
				break;
			}
			case ZYDIS_MNEMONIC_PUSH:
			{
				// For push, the explicit operand could be memory, register and immediate.
				ZyOp=nvc_emu_get_explicit_operand(&ZyIns,0);
				vcpu->gpr.rsp-=ZyIns.address_width>>3;
				if(ZyOp->type==ZYDIS_OPERAND_TYPE_MEMORY)
				{
					// There are two memory operands. We don't know the GPA for one of them.
					if(mem_ctxt->access.write)	// The source operand's GPA is unknown.
						nvc_emu_read_operand(vcpu,&ZyIns,ZyOp,null,&emulation->address);
					else						// The stack's GPA is unknown.
						emulation->address=vcpu->gpr.rsp&((1ui64<<ZyIns.address_width)-1);
					st=noir_emu_dual_memory_operands;
				}
				else
				{
					// The push instruction writes to stack, so MMIO location is stack.
					nvc_emu_read_operand(vcpu,&ZyIns,ZyOp,emulation->data,null);
					st=noir_success;
				}
				// Advance the guest rip.
				vcpu->rip=vcpu->exit_context.next_rip;
				vcpu->state_cache.gprvalid=false;
				break;
			}
			case ZYDIS_MNEMONIC_POP:
			{
				// For pop, the explicit operand could be memory, register and immediate.
				ZyOp=nvc_emu_get_explicit_operand(&ZyIns,0);
				if(ZyOp->type==ZYDIS_OPERAND_TYPE_MEMORY)
				{
					if(mem_ctxt->access.write)	// The stack's GPA is unknown
						emulation->address=vcpu->gpr.rsp&((1ui64<<ZyIns.address_width)-1);
					else
						nvc_emu_write_operand(vcpu,&ZyIns,ZyOp,null,&emulation->address);
					st=noir_emu_dual_memory_operands;
				}
				else
				{
					// The pop instruction read from stack, so MMIO location is stack.
					nvc_emu_write_operand(vcpu,&ZyIns,ZyOp,emulation->data,null);
					st=noir_success;
				}
				vcpu->gpr.rsp+=ZyIns.address_width>>3;
				// Advance the guest rip.
				vcpu->rip=vcpu->exit_context.next_rip;
				vcpu->state_cache.gprvalid=false;
				break;
			}
			case ZYDIS_MNEMONIC_STOSB:
			case ZYDIS_MNEMONIC_STOSW:
			case ZYDIS_MNEMONIC_STOSD:
			case ZYDIS_MNEMONIC_STOSQ:
			{
				// For stos instructions, the source operand is always al register and writing to memory.
				noir_movsb(emulation->data,&vcpu->gpr.rax,mem_ctxt->flags.operand_size);
				if(noir_bt(&vcpu->rflags,10))
					vcpu->gpr.rdi-=mem_ctxt->flags.operand_size;
				else
					vcpu->gpr.rdi+=mem_ctxt->flags.operand_size;
				if(--vcpu->gpr.rcx)
				{
					// If rcx reaches zero, advance the guest rip.
					vcpu->rip=vcpu->exit_context.next_rip;
					vcpu->state_cache.gprvalid=false;
				}
				break;
			}
			default:
			{
				// For unknown instruction used on MMIO, obtain its mnemonics in order to debug.
				char mnemonic[64];
				ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,mnemonic,sizeof(mnemonic),vcpu->exit_context.rip);
				nv_dprintf("[Emulator] Unknown MMIO Instruction: %s!\n",mnemonic);
				st=noir_emu_unknown_instruction;
				break;
			}
		}
	}
	return st;
}

noir_status nvc_emu_try_cvexit_emulation(noir_cvm_virtual_cpu_p vcpu,noir_emulated_instruction_p emulation)
{
	noir_status st=noir_emu_not_emulatable;
	switch(vcpu->exit_context.intercept_code)
	{
		case cv_memory_access:
		{
			st=nvc_emu_try_cvmmio_emulation(vcpu,(noir_emulated_mmio_instruction_p)emulation);
			break;
		}
		default:
		{
			nv_dprintf("Non-emulatable VM-Exit for CVM! Code=0x%d\n",vcpu->exit_context.intercept_code);
			break;
		}
	}
	return st;
}

noir_status nvc_emu_decode_memory_access(noir_cvm_virtual_cpu_p vcpu)
{
	noir_status st=noir_invalid_parameter;
	noir_cvm_memory_access_context_p mem_ctxt=&vcpu->exit_context.memory_access;
	if(vcpu->exit_context.intercept_code==cv_memory_access && mem_ctxt->access.execute==false)
	{
		ZydisDecodedInstruction ZyIns;
		ZydisDecoder *SelectedDecoder;
		ZyanStatus zst;
		st=noir_unsuccessful;
		if(noir_bt(&vcpu->exit_context.cs.attrib,13))		// Long Mode?
			SelectedDecoder=&ZyDec64;
		else if(noir_bt(&vcpu->exit_context.cs.attrib,14))	// Default-Big?
			SelectedDecoder=&ZyDec32;
		else
			SelectedDecoder=&ZyDec16;
		zst=ZydisDecoderDecodeBuffer(SelectedDecoder,vcpu->exit_context.memory_access.instruction_bytes,15,&ZyIns);
		if(ZYAN_SUCCESS(zst))
		{
			for(u8 i=0;i<ZyIns.operand_count;i++)
			{
				bool match=false;
				if(ZyIns.operands[i].type==ZYDIS_OPERAND_TYPE_MEMORY)
				{
					u64 address=nvc_emu_calculate_absolute_address(vcpu,&ZyIns,&ZyIns.operands[i]);
					// Do additional filtering about this operand...
					if(mem_ctxt->access.write)	// Is this operand indicating a write?
						match=(ZyIns.operands[i].actions&ZYDIS_OPERAND_ACTION_WRITE)==ZYDIS_OPERAND_ACTION_WRITE;
					else						// Is this operand indicating a read?
						match=(ZyIns.operands[i].actions&ZYDIS_OPERAND_ACTION_READ)==ZYDIS_OPERAND_ACTION_READ;
					// Does the GVA have the same offset to the GPA?
					match&=page_4kb_offset(address)==page_4kb_offset(mem_ctxt->gpa);
					// Actually, I'm not sure if there are potential bugs.
					// Let me know if there could be such an instruction.
					if(match)
					{
						mem_ctxt->gva=address;
						mem_ctxt->flags.operand_size=ZyIns.operands[i].size>>3;
						mem_ctxt->flags.decoded=true;
						st=noir_success;
						break;
					}
				}
			}
			vcpu->exit_context.vcpu_state.instruction_length=ZyIns.length;
			vcpu->exit_context.next_rip=vcpu->exit_context.rip+ZyIns.length;
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
	if(noir_bt(&seg_state->cs.attrib,13))		// Long Mode?
		SelectedDecoder=&ZyDec64;
	else if(noir_bt(&seg_state->cs.attrib,14))	// Default-Big?
		SelectedDecoder=&ZyDec32;
	else
		SelectedDecoder=&ZyDec16;
	zst=ZydisDecoderDecodeBuffer(SelectedDecoder,instruction,15,&ZyIns);
	if(ZYAN_SUCCESS(zst))
	{
		ZydisDecodedOperand *ZyOp=nvc_emu_get_read_operand(&ZyIns);
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
						break;
					}
					case ZYDIS_OPERAND_TYPE_IMMEDIATE:
					{
						noir_movsb(operand,&ZyOp->imm.value,*size);
						return ZyIns.length;
						break;
					}
				}
				break;
			}
			case ZYDIS_MNEMONIC_PUSH:
			{
				break;
			}
		}
	}
	return 0;
}