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

/*
  Memory Management of NoirVisor

  After refactoring NoirVisor with Rust, NoirVisor will use an internal memory allocator
  so that all allocated memories can be protected by NoirVisor. If using the default
  system memory pool allocator, not all contents on the pages are allocated by NoirVisor,
  so such pages cannot be exclusively assigned for protection.

  NoirVisor uses 128MiB of dynamic memory for internal use at most. The memories for
  internal usage are protected by NoirVisor.
*/

void noir_enum_allocated_large_pages(NOIR_PHYSICAL_MEMORY_RANGE_CALLBACK CallbackRoutine,IN OUT VOID* Context)
{
	for(UINT64 i=0;i<AllocatedLargePages;i++)
		if(CallbackRoutine)
			CallbackRoutine(PageAllocList[i].PhysicalAddress,SIZE_2MB,Context);
		else
			NoirDebugPrint("Start: 0x%llX, Length: 0x%llX\n",PageAllocList[i].PhysicalAddress,SIZE_2MB);
}

// This is the root function that allocates NoirVisor's runtime memory!
static void* NoirAllocateAlignedLargePages(IN UINTN Length,OUT UINTN *AllocIndex)
{
	// First of all, check if we have remaining slots.
	UINTN Pages=EFI_SIZE_TO_LARGE_PAGES(Length);
	if(AllocatedLargePages+Pages>=PAGE_ALLOCATION_LIMIT)
	{
		NoirDebugPrint("No slots are available! NoirVisor has exceeded 128MiB internal memory limit!\n");
		return NULL;
	}
	else
	{
		// Next, allocate from system memory.
		VOID *p=AllocateAlignedRuntimePages(Pages<<9,SIZE_2MB);
		if(p)
		{
			// Find an insertion point. The allocation list is required to be sorted.
			UINTN InsertionPoint=0;
			NoirDebugPrint("New 2MiB is allocated at 0x%p!\n",p);
			for(UINTN i=0;i<AllocatedLargePages;i++)
			{
				if(PageAllocList[i].PhysicalAddress>(UINT64)p)
				{
					InsertionPoint=i;
					break;
				}
			}
			NoirDebugPrint("Insertion Point: %p\n",InsertionPoint);
			// Reserve slots in the middle.
			if(AllocatedLargePages>InsertionPoint)
				for(UINTN i=AllocatedLargePages;i>InsertionPoint;i--)
					PageAllocList[i]=PageAllocList[i-Pages];
			// Insert.
			for(UINTN i=InsertionPoint;i<InsertionPoint+Pages;i++)
			{
				PageAllocList[i].PhysicalAddress=(UINT64)p+(i<<21);
				ZeroMem(PageAllocList[i].Bitmap,sizeof(PageAllocList[i].Bitmap));
			}
			*AllocIndex=InsertionPoint;
			// Initialize the pages.
			ZeroMem(p,Length);
			// Increase the counter.
			AllocatedLargePages+=Pages;
			noir_enum_allocated_large_pages(NULL,NULL);
		}
		return p;
	}
}

void* noir_alloc_contd_memory(IN UINTN Length)
{
	UINTN Pages=EFI_SIZE_TO_PAGES(Length);
	if(Length>SIZE_2MB)
	{
		NoirDebugPrint("Do not use noir_alloc_contd_memory to allocate 2MiB or larger!\n");
		return NULL;
	}
	// Search within allocated large pages from the system.
	for(UINTN i=0;i<AllocatedLargePages;i++)
	{
		for(UINTN j=0;j<512;j++)
		{
			if(!TestBitmap(&PageAllocList[i].Bitmap,j))
			{
				BOOLEAN IsFree=TRUE;
				for(UINTN k=j+1;k<j+Pages;k++)
				{
					if(k==512)
					{
						IsFree=FALSE;
						break;
					}
					if(TestBitmap(&PageAllocList[i].Bitmap,k))
					{
						IsFree=FALSE;
						break;
					}
				}
				if(IsFree)
				{
					void* p=(void*)(PageAllocList[i].PhysicalAddress+EFI_PAGES_TO_SIZE(j));
					for(UINTN k=j;k<j+Pages;k++)
						SetBitmap(&PageAllocList[i].Bitmap,k);
					NoirDebugPrint("Allocated page 0x%p (to 0x%p)\n",p,(UINT64)p+Length);
					ZeroMem(p,Length);
					return p;
				}
			}
		}
	}
	// At this point, we need to allocate new pages!
	UINTN Index;
	void* p=NoirAllocateAlignedLargePages(Length,&Index);
	if(p)
	{
		for(UINTN i=0;i<Pages;i++)SetBitmap(&PageAllocList[Index].Bitmap,Length);
		return p;
	}
	return NULL;
}

void* noir_alloc_2mb_page()
{
	UINTN Index;
	void* p=NoirAllocateAlignedLargePages(SIZE_2MB,&Index);
	if(p)
	{
		// Occupy the whole large page.
		SetMem(PageAllocList[Index].Bitmap,sizeof(PageAllocList[Index].Bitmap),0xFF);
	}
	return p;
}

void noir_free_contd_memory(IN VOID* VirtualAddress,IN UINTN Length)
{
	// Mark the pages as free.
	// Search the page.
	UINT64 PhysicalAddress=(UINT64)VirtualAddress;
	INT64 Min=0,Max=(INT64)AllocatedLargePages;
	while(Max>=Min)
	{
		INT64 Mid=(Max+Min)>>1;
		if(PhysicalAddress<PageAllocList[Mid].PhysicalAddress)
			Max=Mid-1;
		else if(PhysicalAddress>=PageAllocList[Mid].PhysicalAddress+SIZE_2MB)
			Min=Mid+1;
		else
		{
			UINT64 Position=EFI_SIZE_TO_PAGES(PhysicalAddress-PageAllocList[Mid].PhysicalAddress);
			NoirDebugPrint("Releasing 0x%p (Bit Position %u)\n",VirtualAddress,Position);
			for(UINT64 i=0;i<Position;i++)ResetBitmap(&PageAllocList[Mid].Bitmap,i);
		}
	}
}

void noir_free_2mb_page(IN VOID* VirtualAddress)
{
	noir_free_contd_memory(VirtualAddress,SIZE_2MB);
}

UINT64 noir_get_physical_address(IN VOID* VirtualAddress)
{
	// Mapping is identical.
	return (UINT64)VirtualAddress;
}

VOID* noir_find_virt_by_phys(IN UINT64 PhysicalAddress)
{
	// Mapping is identical.
	return (VOID*)PhysicalAddress;
}

VOID* custom_mmap(IN UINTN Length)
{
	// In baremetal environments like UEFI, it should be simple to allocate contiguous memory.
	UINTN Index;
	void* p=NoirAllocateAlignedLargePages(Length,&Index);
	if(p)
	{
		// These pages are all reserved by Rust global allocator.
		UINTN Pages=EFI_SIZE_TO_PAGES(Length)>>9;
		for(UINTN i=0;i<Pages;i++)SetMem(PageAllocList[Index+i].Bitmap,sizeof(PageAllocList->Bitmap),0xFF);
	}
	NoirDebugPrint("[mmap] ptr: 0x%p, size: 0x%X\n",p,Length);
	return p?p:(void*)-1;
}

INT32 custom_munmap(IN VOID* Pointer,IN UINTN Length)
{
	FreeAlignedPages(Pointer,Length>>EFI_PAGE_SHIFT);
	NoirDebugPrint("[munmap] ptr: 0x%p, size: 0x%X\n",Pointer,Length);
	return 0;
}

VOID* custom_direct_mmap(IN UINTN Length)
{
	return (void*)-1;
}

void custom_abort()
{
	NoirDebugPrint("[dlmalloc] Abort happened!\n");
	while(1);
}

VOID* memcpy(OUT VOID* dest,IN VOID* src,IN UINTN cch)
{
	return CopyMem(dest,src,cch);
}

VOID* memmove(OUT VOID* dest,IN VOID* src,IN UINTN cch)
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

UINTN strlen(IN CHAR8* str)
{
	return AsciiStrLen(str);
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