/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file defines customizable VM of NoirVisor for Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/custom_vm.h
*/

#include <ntddk.h>
#include <windef.h>

// Dynamically determine the maximum size of memory-slot.
// Each memory-slot is the size that MDL can describe at maximum.
// Round down to the integer power of 2.
#define MAXIMUM_MEMSLOT_SHIFT_WINXP		24	// For Windows 2000, XP and Server 2003
#define MAXIMUM_MEMSLOT_SHIFT_VISTA		30	// For Windows Vista and Server 2008
#define MAXIMUM_MEMSLOT_SHIFT_WIN7		31	// For Windows 7, Server 2008 R2 and later

BYTE noir_maximum_memslot_shift;

extern PUSHORT NtBuildNumber;

void __cdecl NoirDebugPrint(const char* Format,...);