/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This header file is for NoirVisor's System Function assets of XPF-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/nvsys.h
*/

#include <ntifs.h>
#include <windef.h>

typedef void (*NOIR_PHYSICAL_MEMORY_RANGE_CALLBACK)
(
	IN ULONG64 Start,
	IN ULONG64 Length,
	IN OUT PVOID Context
);

typedef void(*noir_broadcast_worker)(void* context,ULONG ProcessorNumber);

NTKERNELAPI void __fastcall ExfAcquirePushLockExclusive(IN OUT PEX_PUSH_LOCK PushLock);
NTKERNELAPI void __fastcall ExfAcquirePushLockShared(IN OUT PEX_PUSH_LOCK PushLock);
NTKERNELAPI void __fastcall ExfReleasePushLockExclusive(IN OUT PEX_PUSH_LOCK PushLock);
NTKERNELAPI void __fastcall ExfReleasePushLockShared(IN OUT PEX_PUSH_LOCK PushLock);

BYTE NoirGetInstructionLength16(PBYTE Code,SIZE_T CodeLength);
BYTE NoirGetInstructionLength32(PBYTE Code,SIZE_T CodeLength);
BYTE NoirGetInstructionLength64(PBYTE Code,SIZE_T CodeLength);

// Simple Memory Introspection Counters
LONG volatile NoirAllocatedNonPagedPools=0;
LONG volatile NoirAllocatedPagedPools=0;
LONG volatile NoirAllocatedContiguousMemoryCount=0;
LONG volatile NoirAllocatedLargePageCount=0;
LONG64 volatile NoirMappedPhysicalMemorySize=0;