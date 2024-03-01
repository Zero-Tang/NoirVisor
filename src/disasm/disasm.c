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
	ZydisDecoderInit(&ZyDec64,ZYDIS_MACHINE_MODE_LONG_64,ZYDIS_STACK_WIDTH_64);
	ZydisDecoderInit(&ZyDec32,ZYDIS_MACHINE_MODE_LEGACY_32,ZYDIS_STACK_WIDTH_32);
	ZydisDecoderInit(&ZyDec16,ZYDIS_MACHINE_MODE_REAL_16,ZYDIS_STACK_WIDTH_16);
	ZydisFormatterInit(&ZyFmt,ZYDIS_FORMATTER_STYLE_INTEL_MASM);
}

ZyanU8 NoirDisasmCode16(char* Mnemonic,ZyanUSize MnemonicLength,ZyanU8* Code,ZyanUSize CodeLength,ZyanU64 VirtualAddress)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecodedOperand ZyOps[ZYDIS_MAX_OPERAND_COUNT];
	if(ZYAN_SUCCESS(ZydisDecoderDecodeFull(&ZyDec16,Code,CodeLength,&ZyIns,ZyOps)))
		ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,ZyOps,ZyIns.operand_count_visible,Mnemonic,MnemonicLength,VirtualAddress,ZYAN_NULL);
	return ZyIns.length;
}

ZyanU8 NoirDisasmCode32(char* Mnemonic,ZyanUSize MnemonicLength,ZyanU8* Code,ZyanUSize CodeLength,ZyanU64 VirtualAddress)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecodedOperand ZyOps[ZYDIS_MAX_OPERAND_COUNT];
	if(ZYAN_SUCCESS(ZydisDecoderDecodeFull(&ZyDec32,Code,CodeLength,&ZyIns,ZyOps)))
		ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,ZyOps,ZyIns.operand_count_visible,Mnemonic,MnemonicLength,VirtualAddress,ZYAN_NULL);
	return ZyIns.length;
}

ZyanU8 NoirDisasmCode64(char* Mnemonic,ZyanUSize MnemonicLength,ZyanU8* Code,ZyanUSize CodeLength,ZyanU64 VirtualAddress)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecodedOperand ZyOps[ZYDIS_MAX_OPERAND_COUNT];
	if(ZYAN_SUCCESS(ZydisDecoderDecodeFull(&ZyDec64,Code,CodeLength,&ZyIns,ZyOps)))
		ZydisFormatterFormatInstruction(&ZyFmt,&ZyIns,ZyOps,ZyIns.operand_count_visible,Mnemonic,MnemonicLength,VirtualAddress,ZYAN_NULL);
	return ZyIns.length;
}

ZyanU8 NoirGetInstructionLength16(ZyanU8* Code,ZyanUSize CodeLength)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeInstruction(&ZyDec16,ZYAN_NULL,Code,CodeLength==0?15:CodeLength,&ZyIns);
	return ZyIns.length;
}

ZyanU8 NoirGetInstructionLength32(ZyanU8* Code,ZyanUSize CodeLength)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeInstruction(&ZyDec32,ZYAN_NULL,Code,CodeLength==0?15:CodeLength,&ZyIns);
	return ZyIns.length;
}

ZyanU8 NoirGetInstructionLength64(ZyanU8* Code,ZyanUSize CodeLength)
{
	ZydisDecodedInstruction ZyIns;
	ZydisDecoderDecodeInstruction(&ZyDec64,ZYAN_NULL,Code,CodeLength==0?15:CodeLength,&ZyIns);
	return ZyIns.length;
}