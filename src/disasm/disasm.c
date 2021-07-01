/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the invoker of Zydis Disassembler Library.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /disasm/disasm.c
*/

#include <Zydis/Zydis.h>

ZydisDecoder ZyDec16;
ZydisDecoder ZyDec32;
ZydisDecoder ZyDec64;
ZydisFormatter ZyFmt;

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