/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is auxiliary to MSR-Hook facility to optimize compatibility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/hookmsr.h
*/

#include <ntddk.h>
#include <windef.h>

#if defined(_WIN64)
#define INDEX_OFFSET		0x15
#else
#define INDEX_OFFSET		0x1
#endif

ULONG IndexOf_NtOpenProcess=0x23;		//This is hard-code on Windows 7 x64.
ULONG ProtPID=0;
extern ULONG_PTR orig_system_call;