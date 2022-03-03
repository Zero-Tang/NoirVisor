/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines some common functions to be used on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/common.c
*/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

typedef UINTN (*Param0Func)();
typedef UINTN (*Param1Func)(IN UINTN Value);

Param0Func ReadDrArray[8]=
{
	AsmReadDr0,
	AsmReadDr1,
	AsmReadDr2,
	AsmReadDr3,
	AsmReadDr4,
	AsmReadDr5,
	AsmReadDr6,
	AsmReadDr7
};

Param1Func WriteDrArray[8]=
{
	AsmWriteDr0,
	AsmWriteDr1,
	AsmWriteDr2,
	AsmWriteDr3,
	AsmWriteDr4,
	AsmWriteDr5,
	AsmWriteDr6,
	AsmWriteDr7
};

// CRT Functions.
int strcmp(const char *str1,const char *str2)
{
	return (int)AsciiStrCmp(str1,str2);
}

void* memset(void *ptr,int value,size_t num)
{
	return SetMem(ptr,num,(UINT8)value);
}

void* memcpy(void* destination,const void* source,size_t num)
{
	return CopyMem(destination,source,num);
}

// MSVC Intrinsics that LLVM clang did not implement them.
void _enable(void)
{
	EnableInterrupts();
}

void _disable(void)
{
	DisableInterrupts();
}

void __wbinvd(void)
{
	AsmWbinvd();
}

void __lidt(void* idtr)
{
	AsmWriteIdtr(idtr);
}

void __lgdt(void* gdtr)
{
	AsmWriteGdtr(gdtr);
}

UINTN __readcr0(void)
{
	return AsmReadCr0();
}

UINTN __readcr2(void)
{
	return AsmReadCr2();
}

UINTN __readcr4(void)
{
	return AsmReadCr4();
}

void __writecr0(UINTN value)
{
	AsmWriteCr0(value);
}

void __writecr4(UINTN value)
{
	AsmWriteCr4(value);
}

UINTN __readdr(unsigned int index)
{
	return index<8?ReadDrArray[index]():0;
}

void __writedr(unsigned int index,UINTN value)
{
	if(index<8)WriteDrArray[index](value);
}

void __writemsr(unsigned long index,unsigned __int64 value)
{
	AsmWriteMsr64(index,value);
}