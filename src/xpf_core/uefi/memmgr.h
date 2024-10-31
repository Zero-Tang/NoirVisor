#/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file manages memories for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/memmgr.h
*/

#include <Uefi.h>

#define LOCK_IS_FREE	(void*)0
#define LOCK_IN_USE		(void*)1

#define PAGE_ALLOCATION_LIMIT	64

#define EFI_SIZE_TO_LARGE_PAGES(Size)	(((Size)>>21)+(((Size)&(SIZE_2MB-1))?1:0))

#define TestBitmap(b,p)		_bittest64(b[(p)>>6],(p)&63)
#define SetBitmap(b,p)		_bittestandset64(b[(p)>>6],(p)&63)
#define ResetBitmap(b,p)	_bittestandreset64(b[(p)>>6],(p)&63)

void __cdecl NoirDebugPrint(IN CONST CHAR8 *Format,...);

typedef void (*NOIR_PHYSICAL_MEMORY_RANGE_CALLBACK)
(
	IN UINT64 Start,
	IN UINT64 Length,
	IN OUT VOID* Context
);

typedef struct _PAGE_ALLOCATION_INFORMATION
{
	UINT64 PhysicalAddress;
	// Use a bitmap to indicate what pages are allocated.
	// Each large page has 512 (8*64) pages.
	UINT64 Bitmap[8];
}PAGE_ALLOCATION_INFORMATION,*PPAGE_ALLOCATION_INFORMATION;

// Each item contains a 2MiB page.
// At maximum, NoirVisor allows 128MiB for internal memory consumption.
PAGE_ALLOCATION_INFORMATION PageAllocList[PAGE_ALLOCATION_LIMIT]={0};
UINT64 AllocatedLargePages=0;