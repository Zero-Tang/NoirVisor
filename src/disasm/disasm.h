/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the invoker of Zydis Disassembler Library.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /disasm/disasm.h
*/

typedef struct NoirBasicOperand_
{
	union
	{
		struct
		{
			ZyanU64 Displacement:32;	// Bits 0-31
			ZyanU64 SiValid:1;			// Bit	32
			ZyanU64 Scale:2;			// Bits	33-34
			ZyanU64 Index:4;			// Bits	35-38
			ZyanU64 Base:4;				// Bits	39-42
			ZyanU64 Segment:3;			// Bits	43-45
			ZyanU64 AddressSize:2;		// Bits	46-47
			ZyanU64 Reg1:4;				// Bits	48-51
			ZyanU64 Reg2:4;				// Bits	52-55
			ZyanU64 OperandSize:2;		// Bits	56-57
			ZyanU64 BaseValid:1;		// Bit	58
			ZyanU64 Reserved:1;			// Bit	59
			ZyanU64 MemValid:1;			// Bit	60
			ZyanU64 RegValid:2;			// Bits	61-62
			ZyanU64 ImmValid:1;			// Bit	63
		};
		ZyanU64 Value;
	}Complex;
	union
	{
		ZyanI64 Signed;
		ZyanU64 Unsigned;
	}Immediate;
}NoirBasicOperand;

ZydisDecoder ZyDec16;
ZydisDecoder ZyDec32;
ZydisDecoder ZyDec64;
ZydisFormatter ZyFmt;