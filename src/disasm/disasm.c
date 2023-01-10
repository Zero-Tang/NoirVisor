/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the invoker of Zydis Disassembler Library.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /disasm/disasm.c
*/

#include <Zydis/Zydis.h>
#ifdef __clang__
#include <intrin.h>
#endif
#include "disasm.h"

void NoirInitializeDisassembler()
{
	ZydisDecoderInit(&ZyDec64,ZYDIS_MACHINE_MODE_LONG_64,ZYDIS_ADDRESS_WIDTH_64);
	ZydisDecoderInit(&ZyDec32,ZYDIS_MACHINE_MODE_LEGACY_32,ZYDIS_ADDRESS_WIDTH_32);
	ZydisDecoderInit(&ZyDec16,ZYDIS_MACHINE_MODE_REAL_16,ZYDIS_ADDRESS_WIDTH_16);
	ZydisFormatterInit(&ZyFmt,ZYDIS_FORMATTER_STYLE_INTEL);
}

ZyanU8 NoirDisasmCode16(char* Mnemonic,ZyanUSize MnemonicLength,ZyanU8* Code,ZyanUSize CodeLength,ZyanU64 VirtualAddress)
{
	ZydisDecodedInstruction ZyIns;
	if(ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&ZyDec16,Code,CodeLength,&ZyIns)))
		ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,Mnemonic,MnemonicLength,VirtualAddress);
	return ZyIns.length;
}

ZyanU8 NoirDisasmCode32(char* Mnemonic,ZyanUSize MnemonicLength,ZyanU8* Code,ZyanUSize CodeLength,ZyanU64 VirtualAddress)
{
	ZydisDecodedInstruction ZyIns;
	if(ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&ZyDec32,Code,CodeLength,&ZyIns)))
		ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,Mnemonic,MnemonicLength,VirtualAddress);
	return ZyIns.length;
}

ZyanU8 NoirDisasmCode64(char* Mnemonic,ZyanUSize MnemonicLength,ZyanU8* Code,ZyanUSize CodeLength,ZyanU64 VirtualAddress)
{
	ZydisDecodedInstruction ZyIns;
	if(ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&ZyDec64,Code,CodeLength,&ZyIns)))
		ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,Mnemonic,MnemonicLength,VirtualAddress);
	return ZyIns.length;
}

ZyanU8 NoirGetInstructionLength16(ZyanU8* Code,ZyanUSize CodeLength)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeBuffer(&ZyDec16,Code,CodeLength==0?15:CodeLength,&ZyIns);
	return ZyIns.length;
}

ZyanU8 NoirGetInstructionLength32(ZyanU8* Code,ZyanUSize CodeLength)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeBuffer(&ZyDec32,Code,CodeLength==0?15:CodeLength,&ZyIns);
	return ZyIns.length;
}

ZyanU8 NoirGetInstructionLength64(ZyanU8* Code,ZyanUSize CodeLength)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeBuffer(&ZyDec64,Code,CodeLength==0?15:CodeLength,&ZyIns);
	return ZyIns.length;
}

void noir_decode_instruction16(ZyanU8* Code,ZyanUSize CodeLength,ZydisDecodedInstruction *DecodeResult)
{
	ZydisDecoderDecodeBuffer(&ZyDec16,Code,CodeLength==0?15:CodeLength,DecodeResult);
}

void noir_decode_instruction32(ZyanU8* Code,ZyanUSize CodeLength,ZydisDecodedInstruction *DecodeResult)
{
	ZydisDecoderDecodeBuffer(&ZyDec32,Code,CodeLength==0?15:CodeLength,DecodeResult);
}

void noir_decode_instruction64(ZyanU8* Code,ZyanUSize CodeLength,ZydisDecodedInstruction *DecodeResult)
{
	ZydisDecoderDecodeBuffer(&ZyDec64,Code,CodeLength==0?15:CodeLength,DecodeResult);
}

void noir_decode_basic_operand(ZydisDecodedInstruction* DecodeResult,NoirBasicOperand *BasicOperand)
{
	BasicOperand->Complex.Value=0;
	BasicOperand->Immediate.Unsigned=0;
	for(ZyanU8 i=0;i<DecodeResult->operand_count;i++)
	{
		// Only explicit operands are considered.
		if(DecodeResult->operands[i].visibility==ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
		{
			BasicOperand->Complex.OperandSize=DecodeResult->operand_width>>4;
			BasicOperand->Complex.AddressSize=DecodeResult->address_width>>5;
			switch(DecodeResult->operands[i].type)
			{
				case ZYDIS_OPERAND_TYPE_REGISTER:
				{
					ZydisRegister Subtraction[4]={ZYDIS_REGISTER_AL,ZYDIS_REGISTER_AX,ZYDIS_REGISTER_EAX,ZYDIS_REGISTER_RAX};
					// FIXME: Cannot correctly decode 8-bit registers in light of the mess of ah,spl,r8b etc.
					ZydisRegister Value=DecodeResult->operands[i].reg.value-Subtraction[BasicOperand->Complex.OperandSize];
					BasicOperand->Complex.RegValid++;
					switch(BasicOperand->Complex.RegValid)
					{
						case 1:
							BasicOperand->Complex.Reg1=Value;
							break;
						case 2:
							BasicOperand->Complex.Reg2=Value;
							break;
					}
					break;
				}
				case ZYDIS_OPERAND_TYPE_MEMORY:
				{
					unsigned long Scale=0;
					ZydisRegister Subtraction[3]={ZYDIS_REGISTER_AX,ZYDIS_REGISTER_EAX,ZYDIS_REGISTER_RAX};
					BasicOperand->Complex.MemValid=1;
					if(DecodeResult->operands[i].mem.disp.has_displacement)
						BasicOperand->Complex.Displacement=DecodeResult->operands[i].mem.disp.value;
					BasicOperand->Complex.Segment=DecodeResult->operands[i].mem.segment-ZYDIS_REGISTER_ES;
					BasicOperand->Complex.Index=DecodeResult->operands[i].mem.index-Subtraction[BasicOperand->Complex.OperandSize];
					BasicOperand->Complex.Base=DecodeResult->operands[i].mem.base-Subtraction[BasicOperand->Complex.OperandSize];
					_BitScanForward(&Scale,DecodeResult->operands[i].mem.scale);
					BasicOperand->Complex.Scale=Scale;
					if(DecodeResult->operands[i].mem.index!=ZYDIS_REGISTER_NONE && DecodeResult->operands[i].mem.scale)
						BasicOperand->Complex.SiValid=1;
					BasicOperand->Complex.BaseValid=DecodeResult->operands[i].mem.base!=ZYDIS_REGISTER_NONE;
					break;
				}
				case ZYDIS_OPERAND_TYPE_IMMEDIATE:
				{
					BasicOperand->Complex.ImmValid=1;
					if(DecodeResult->operands[i].imm.is_signed)
						BasicOperand->Immediate.Signed=DecodeResult->operands[i].imm.value.s;
					else
						BasicOperand->Immediate.Unsigned=DecodeResult->operands[i].imm.value.u;
					break;
				}
				default:
				{
					// For the sake of compiler warning.
					break;
				}
			}
		}
	}
}