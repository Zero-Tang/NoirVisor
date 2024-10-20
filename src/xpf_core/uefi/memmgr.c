#/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file manages memories for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/memmgr.c
*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include "memmgr.h"

VOID* custom_mmap(IN UINTN Length)
{
	// In baremetal environments like UEFI, it should be simple to allocate contiguous memory.
	void* p=AllocateAlignedRuntimePages(Length>>EFI_PAGE_SHIFT,0x200000);
	Print(L"[mmap] ptr: 0x%p, size: 0x%X\n",p,Length);
	return p?p:(void*)-1;
}

INT32 custom_munmap(IN VOID* Pointer,IN UINTN Length)
{
	FreePages(Pointer,Length>>EFI_PAGE_SHIFT);
	Print(L"[munmap] ptr: 0x%p, size: 0x%X\n",Pointer,Length);
	return 0;
}

VOID* custom_direct_mmap(IN UINTN Length)
{
	return (void*)-1;
}

void custom_abort()
{
	Print(L"[dlmalloc] Abort happened!\n");
}

VOID* memcpy(OUT VOID* dest,IN VOID* src,IN UINTN cch)
{
	return CopyMem(dest,src,cch);
}

VOID* memset(OUT VOID* dest,IN UINT8 val,IN UINTN cch)
{
	return SetMem(dest,cch,val);
}

INTN memcmp(IN VOID* dest,IN VOID* src,IN UINTN cch)
{
	return CompareMem(dest,src,cch);
}

void init_lock(void* *lock)
{
	*lock=NULL;
}

void final_lock(void* *lock)
{
	;
}

void acquire_lock(void* *lock)
{
	// In real machines, "pause" instruction has less power consumption than "nop".
	// In virtual machines, "pause" instruction can hint hypervisor a spinlock so that the hypervisor will schedule out the vCPU.
	while(_InterlockedCompareExchangePointer(lock,LOCK_IN_USE,LOCK_IS_FREE))_mm_pause();
}

void release_lock(void* *lock)
{
	_InterlockedExchangePointer(lock,LOCK_IS_FREE);
}