/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

  This file is the length-disassembler engine of NoirVisor.
  LDE is developped by BeatriX, but I have no idea what its license is.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /disasm/LDE.c
*/

#include <ntddk.h>
#include <windef.h>
#if defined(_WIN64)
#include "LDE64.h"
#endif

ULONG SizeOfCode(IN PVOID Code,IN ULONG Architecture)
{
	if(LDE)return LDE(Code,Architecture);
	return 0;
}

ULONG GetPatchSize(IN PVOID Code,IN ULONG HookLength)
{
	if(LDE)
	{
		ULONG s=0,l=0;
		while(s<HookLength)
		{
			PVOID p=(PVOID)((ULONG_PTR)Code+s);
			s+=SizeOfCode(p,sizeof(void*)*16-64);
		}
		return s;
	}
	return 0;
}

void LDE_Initialize()
{
	LDE=ExAllocatePoolWithTag(NonPagedPool,LDE_SHELLCODE_SIZE,'LDET');
	if(LDE)RtlCopyMemory(LDE,szShellCode,LDE_SHELLCODE_SIZE);
}

void LDE_Finalize()
{
	if(LDE)ExFreePoolWithTag(LDE,'LDET');
}