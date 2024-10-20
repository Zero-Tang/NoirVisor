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

#define LOCK_IS_FREE	(void*)0
#define LOCK_IN_USE		(void*)1

#define ALLOCATION_PAGE_MAXIMUM		64

typedef struct _PAGE_ALLOCATION_INFORMATION
{
	UINTN PhysicalAddress;
	UINTN Length;
}PAGE_ALLOCATION_INFORMATION,*PPAGE_ALLOCATION_INFORMATION;

PAGE_ALLOCATION_INFORMATION AllocatedPages[ALLOCATION_PAGE_MAXIMUM]={0};
UINTN AllocationPageCount=0;