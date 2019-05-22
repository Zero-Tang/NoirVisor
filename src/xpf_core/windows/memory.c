/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is memory management facility on Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/memory.c
*/

#include <ntddk.h>
#include <windef.h>

void __cdecl NoirDebugPrint(const char* Format,...);

void* noir_alloc_contd_memory(size_t length)
{
	PHYSICAL_ADDRESS M={0xFFFFFFFFFFFFFFFF};
	PVOID p=MmAllocateContiguousMemory(length,M);
	if(p)RtlZeroMemory(p,length);
	return p;
}

void* noir_alloc_nonpg_memory(size_t length)
{
	PVOID p=ExAllocatePoolWithTag(NonPagedPool,length,'pNvN');
	if(p)RtlZeroMemory(p,length);
	return p;
}

void* noir_alloc_paged_memory(size_t length)
{
	PVOID p=ExAllocatePoolWithTag(PagedPool,length,'gPvN');
	if(p)RtlZeroMemory(p,length);
	return p;
}

void noir_free_contd_memory(void* virtual_address)
{
#if defined(_WINNT5)
	//It is recommended to release contiguous memory at APC level on NT5.
	KIRQL f_oldirql;
	KeRaiseIrql(APC_LEVEL,&f_oldirql);
#endif
	MmFreeContiguousMemory(virtual_address);
#if defined(_WINNT5)
	KeLowerIrql(f_oldirql);
#endif
}

void noir_free_nonpg_memory(void* virtual_address)
{
	ExFreePoolWithTag(virtual_address,'pNvN');
}

void noir_free_paged_memory(void* virtual_address)
{
	ExFreePoolWithTag(virtual_address,'gPvN');
}

ULONG64 noir_get_physical_address(void* virtual_address)
{
	PHYSICAL_ADDRESS pa;
	pa=MmGetPhysicalAddress(virtual_address);
	return pa.QuadPart;
}

//We need to map physical memory in nesting virtualization.
void* noir_map_physical_memory(ULONG64 physical_address,size_t length)
{
	PHYSICAL_ADDRESS pa;
	pa.QuadPart=physical_address;
	return MmMapIoSpace(pa,length,MmCached);
}

void noir_unmap_physical_memory(void* virtual_address,size_t length)
{
	MmUnmapIoSpace(virtual_address,length);
}

void noir_copy_memory(void* dest,void* src,size_t cch)
{
	RtlCopyMemory(dest,src,cch);
}

void* noir_alloc_2mb_page()
{
	PHYSICAL_ADDRESS L={0};
	PHYSICAL_ADDRESS H={0xFFFFFFFFFFFFFFFF};
	PHYSICAL_ADDRESS B={0x200000};
	PVOID p=MmAllocateContiguousMemorySpecifyCache(0x200000,L,H,B,MmCached);
	if(p)RtlZeroMemory(p,0x200000);
	return p;
}

void noir_free_2mb_page(void* virtual_address)
{
	MmFreeContiguousMemorySpecifyCache(virtual_address,0x200000,MmCached);
}

//Some Additional repetitive functions
ULONG64 NoirGetPhysicalAddress(IN PVOID VirtualAddress)
{
	PHYSICAL_ADDRESS pa=MmGetPhysicalAddress(VirtualAddress);
	return pa.QuadPart;
}

PVOID NoirAllocateContiguousMemory(IN ULONG Length)
{
	PHYSICAL_ADDRESS MaxAddr={0xFFFFFFFFFFFFFFFF};
	return MmAllocateContiguousMemory(Length,MaxAddr);
}