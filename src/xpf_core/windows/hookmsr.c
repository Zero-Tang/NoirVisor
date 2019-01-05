/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is auxiliary to MSR-Hook facility to optimize compatibility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/hookmsr.c
*/

#include <ntddk.h>
#include <windef.h>
#include "hookmsr.h"

void NoirGetNtOpenProcessIndex()
{
	UNICODE_STRING uniFuncName=RTL_CONSTANT_STRING(L"ZwOpenProcess");
	PVOID p=MmGetSystemRoutineAddress(&uniFuncName);
	if(p)IndexOf_NtOpenProcess=*(PULONG)((ULONG_PTR)p+INDEX_OFFSET);
}

void NoirSetProtectedPID(IN ULONG NewPID)
{
	ProtPID=NewPID;
	ProtPID&=0xFFFFFFFC;
}