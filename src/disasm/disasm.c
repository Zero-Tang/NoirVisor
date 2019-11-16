/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

  This file is the length-disassembler engine of NoirVisor.
  LDE is developped by BeatriX - Licensed as LGPL v3.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /disasm/disasm.c
*/

#include <ntddk.h>
#include <windef.h>

ULONG LDE(IN PVOID Code,IN ULONG Architecture);

ULONG GetPatchSize(IN PVOID Code,IN ULONG HookLength)
{
	ULONG s=0,l=0;
	while(s<HookLength)
	{
		PVOID p=(PVOID)((ULONG_PTR)Code+s);
		s+=LDE(p,sizeof(void*)*16-64);
	}
	return s;
}