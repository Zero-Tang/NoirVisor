/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the NoirVisor's System Function assets of XPF-Core.
  Facilities are implemented in this file, including:
  Debugging facilities...
  Memory Management...
  Processor Management...
  Multithreading Management...

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/nvsys.c
*/

#include <ntifs.h>
#include <windef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ntstrsafe.h>
#include <intrin.h>
#include "nvsys.h"

// Synchronous Debug-Printer
void __cdecl NoirDebugPrint(const char* Format,...)
{
	va_list arg_list;
	va_start(arg_list,Format);
	vDbgPrintExWithPrefix("[NoirVisor - Driver] ",DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,Format,arg_list);
	va_end(arg_list);
}

void __cdecl nvci_tracef(const char* format,...)
{
	LARGE_INTEGER SystemTime,LocalTime;
	TIME_FIELDS Time;
	char Buffer[512];
	PSTR LogBuffer;
	SIZE_T LogSize;
	va_list arg_list;
	va_start(arg_list,format);
	KeQuerySystemTime(&SystemTime);
	ExSystemTimeToLocalTime(&SystemTime,&LocalTime);
	RtlTimeToTimeFields(&LocalTime,&Time);
	RtlStringCbPrintfExA(Buffer,sizeof(Buffer),&LogBuffer,&LogSize,STRSAFE_FILL_BEHIND_NULL,"[NoirVisor - CI Log]\t| %04d-%02d-%02d %02d:%02d:%02d.%03d | ",Time.Year,Time.Month,Time.Day,Time.Hour,Time.Minute,Time.Second,Time.Milliseconds);
	RtlStringCbVPrintfA(LogBuffer,LogSize,format,arg_list);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_TRACE_LEVEL,Buffer);
	va_end(arg_list);
}

void __cdecl nvci_panicf(const char* format,...)
{
	LARGE_INTEGER SystemTime,LocalTime;
	TIME_FIELDS Time;
	char Buffer[512];
	PSTR LogBuffer;
	SIZE_T LogSize;
	va_list arg_list;
	va_start(arg_list,format);
	KeQuerySystemTime(&SystemTime);
	ExSystemTimeToLocalTime(&SystemTime,&LocalTime);
	RtlTimeToTimeFields(&LocalTime,&Time);
	RtlStringCbPrintfExA(Buffer,sizeof(Buffer),&LogBuffer,&LogSize,STRSAFE_FILL_BEHIND_NULL,"[NoirVisor - CI Panic]\t| %04d-%02d-%02d %02d:%02d:%02d.%03d | ",Time.Year,Time.Month,Time.Day,Time.Hour,Time.Minute,Time.Second,Time.Milliseconds);
	RtlStringCbVPrintfA(LogBuffer,LogSize,format,arg_list);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,Buffer);
	va_end(arg_list);
}

void __cdecl nv_dprintf2(IN BOOL DateTime,IN BOOL ProcessorNumber,IN PCSTR FunctionName,IN PCSTR Format,...)
{
	CHAR Buffer[512];
	PSTR ContentBuffer,TimeBuffer;
	SIZE_T ContentSize,TimeSize;
	va_list ArgList;
	if(ProcessorNumber)
		RtlStringCbPrintfExA(Buffer,sizeof(Buffer),&TimeBuffer,&TimeSize,STRSAFE_FILL_BEHIND_NULL,"[NoirVisor - Core %03u] ",KeGetCurrentProcessorNumber());
	else
		RtlStringCbCopyExA(Buffer,sizeof(Buffer),"[NoirVisor] ",&TimeBuffer,&TimeSize,STRSAFE_FILL_BEHIND_NULL);
	if(DateTime)
	{
		LARGE_INTEGER SystemTime,LocalTime;
		TIME_FIELDS Time;
		KeQuerySystemTime(&SystemTime);
		ExSystemTimeToLocalTime(&SystemTime,&LocalTime);
		RtlTimeToTimeFields(&LocalTime,&Time);
		RtlStringCbPrintfExA(TimeBuffer,TimeSize,&ContentBuffer,&ContentSize,STRSAFE_FILL_BEHIND_NULL,"%04d-%02d-%02d %02d:%02d:%02d.%03d | ",Time.Year,Time.Month,Time.Day,Time.Hour,Time.Minute,Time.Second,Time.Milliseconds);
	}
	else
	{
		ContentBuffer=TimeBuffer;
		ContentSize=TimeSize;
	}
	va_start(ArgList,Format);
	RtlStringCbVPrintfA(ContentBuffer,ContentSize,Format,ArgList);
	va_end(ArgList);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,Buffer);
}

void __cdecl nv_dprintf_unprefixed(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	vDbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,format,arg_list);
	va_end(arg_list);
}

void __cdecl nv_dprintf(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	vDbgPrintExWithPrefix("[NoirVisor] ",DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,format,arg_list);
	va_end(arg_list);
}

void __cdecl nv_tracef(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	vDbgPrintExWithPrefix("[NoirVisor - Trace] ",DPFLTR_IHVDRIVER_ID,DPFLTR_TRACE_LEVEL,format,arg_list);
	va_end(arg_list);
}

void __cdecl nv_panicf(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	vDbgPrintExWithPrefix("[NoirVisor - Panic] ",DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,format,arg_list);
	va_end(arg_list);
}

void NoirReportMemoryIntrospectionCounter()
{
	NoirDebugPrint("============NoirVisor Memory Introspection Report Start============\n");
	NoirDebugPrint("Unreleased NonPaged Pools: %d\n",NoirAllocatedNonPagedPools);
	NoirDebugPrint("Unreleased Paged Pools: %d\n",NoirAllocatedPagedPools);
	NoirDebugPrint("Unreleased Contiguous Memory Count: %d\n",NoirAllocatedContiguousMemoryCount);
	if(NoirAllocatedNonPagedPools || NoirAllocatedPagedPools || NoirAllocatedContiguousMemoryCount)
		NoirDebugPrint("Memory Leak is detected!\n");
	else
		NoirDebugPrint("No Memory Leaks...\n");
	NoirDebugPrint("=============NoirVisor Memory Introspection Report End=============\n");
}

// Asynchronous Debug-Printer Implementation...
// In the context of VM-Exit, debug-printing is not permitted.
// FIXME: Some debug log messages are missing.

PVOID NoirAllocateLoggerBuffer(IN ULONG Size)
{
#if _MSC_FULL_VER>192930140
	return ExAllocatePool2(POOL_FLAG_NON_PAGED,Size,'gLvN');
#else
	PVOID p=ExAllocatePoolWithTag(NonPagedPool,Size,'gLvN');
	if(p)RtlZeroMemory(p,Size);
	return p;
#endif
}

PVOID NoirAllocateContiguousMemory(IN SIZE_T Length)
{
	PHYSICAL_ADDRESS H={0xFFFFFFFFFFFFFFFF};
	PVOID p=MmAllocateContiguousMemory(Length,H);
	if(p)
	{
		RtlZeroMemory(p,Length);
		InterlockedIncrement(&NoirAllocatedContiguousMemoryCount);
	}
	return p;
}

PVOID NoirAllocateNonPagedMemory(IN SIZE_T Length)
{
#if _MSC_FULL_VER>192930140
	PVOID p=ExAllocatePool2(POOL_FLAG_NON_PAGED_EXECUTE,Length,'pNvN');
	if(p)InterlockedIncrement(&NoirAllocatedNonPagedPools);
#else
	PVOID p=ExAllocatePoolWithTag(NonPagedPool,Length,'pNvN');
	if(p)
	{
		RtlZeroMemory(p,Length);
		InterlockedIncrement(&NoirAllocatedNonPagedPools);
	}
#endif
	return p;
}

PVOID NoirAllocatePagedMemory(IN SIZE_T Length)
{
#if _MSC_FULL_VER>192930140
	PVOID p=ExAllocatePool2(POOL_FLAG_PAGED,Length,'gPvN');
	if(p)InterlockedIncrement(&NoirAllocatedPagedPools);
#else
	PVOID p=ExAllocatePoolWithTag(PagedPool,Length,'gPvN');
	if(p)
	{
		RtlZeroMemory(p,Length);
		InterlockedIncrement(&NoirAllocatedPagedPools);
	}
#endif
	return p;
}

void NoirFreeContiguousMemory(PVOID VirtualAddress)
{
#if defined(_WINNT5)
	// It is recommended to release contiguous memory at APC level on NT5.
	KIRQL f_oldirql;
	KeRaiseIrql(APC_LEVEL,&f_oldirql);
#endif
	MmFreeContiguousMemory(VirtualAddress);
	InterlockedDecrement(&NoirAllocatedContiguousMemoryCount);
#if defined(_WINNT5)
	KeLowerIrql(f_oldirql);
#endif
}

void NoirFreeNonPagedMemory(IN PVOID VirtualAddress)
{
#if _MSC_FULL_VER>192930140
	ExFreePool2(VirtualAddress,'pNvN',NULL,0);
#else
	ExFreePoolWithTag(VirtualAddress,'pNvN');
#endif
	InterlockedDecrement(&NoirAllocatedNonPagedPools);
}

void NoirFreePagedMemory(IN PVOID VirtualAddress)
{
#if _MSC_FULL_VER>192930140
	ExFreePool2(VirtualAddress,'gPvN',NULL,0);
#else
	ExFreePoolWithTag(VirtualAddress,'gPvN');
#endif
	InterlockedDecrement(&NoirAllocatedPagedPools);
}

void NoirFreeLoggerBuffer(IN PVOID Buffer)
{
#if _MSC_FULL_VER>192930140
	ExFreePool2(Buffer,'gLvN',NULL,0);
#else
	ExFreePoolWithTag(Buffer,'gLvN');
#endif
}

ULONG32 noir_get_processor_count()
{
	KAFFINITY af;
	return KeQueryActiveProcessorCount(&af);
}

ULONG32 noir_get_current_processor()
{
	return KeGetCurrentProcessorNumber();
}

void static NoirDpcRT(IN PKDPC Dpc,IN PVOID DeferedContext OPTIONAL,IN PVOID SystemArgument1 OPTIONAL,IN PVOID SystemArgument2 OPTIONAL)
{
	noir_broadcast_worker worker=(noir_broadcast_worker)SystemArgument1;
	PLONG32 volatile GlobalOperatingNumber=(PLONG32)SystemArgument2;
	ULONG Pn=KeGetCurrentProcessorNumber();
	worker(DeferedContext,Pn);
	InterlockedDecrement(GlobalOperatingNumber);
}

void noir_generic_call(noir_broadcast_worker worker,void* context)
{
	ULONG32 Num=noir_get_processor_count();
	PVOID IpbBuffer=NoirAllocateNonPagedMemory(Num*sizeof(KDPC)+4);
	if(IpbBuffer)
	{
		PLONG32 volatile GlobalOperatingNumber=(PULONG32)((ULONG_PTR)IpbBuffer+Num*sizeof(KDPC));
		PKDPC pDpc=(PKDPC)IpbBuffer;
		*GlobalOperatingNumber=Num;
		for(ULONG i=0;i<Num;i++)
		{
			KeInitializeDpc(&pDpc[i],NoirDpcRT,context);
			KeSetTargetProcessorDpc(&pDpc[i],(BYTE)i);
			KeSetImportanceDpc(&pDpc[i],HighImportance);
			KeInsertQueueDpc(&pDpc[i],(PVOID)worker,(PVOID)GlobalOperatingNumber);
		}
		// Use TTAS-Spinning Semaphore here for better performance.
		while(InterlockedCompareExchange(GlobalOperatingNumber,0,0))
			_mm_pause();		// Optimized Processor Relax.
		NoirFreeNonPagedMemory(IpbBuffer);
	}
}

NTSTATUS NoirCopyAcpiTableRootFromRegistry(OUT PVOID *Rsdt,OUT PSIZE_T Length)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PKEY_BASIC_INFORMATION KeyBasInf=NoirAllocatePagedMemory(PAGE_SIZE);
	if(KeyBasInf)
	{
		HANDLE hKey=NULL;
		UNICODE_STRING uniKeyName=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Hardware\\ACPI\\RSDT");
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa,&uniKeyName,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
		st=ZwOpenKey(&hKey,GENERIC_READ,&oa);
		if(NT_SUCCESS(st))
		{
			ULONG RetLen=0;
			st=ZwEnumerateKey(hKey,0,KeyBasicInformation,KeyBasInf,PAGE_SIZE,&RetLen);
			if(NT_ERROR(st))
				NoirDebugPrint("Failed to enumerate %wZ! Status=0x%X, Return-Length=0x%X\n",&uniKeyName,st,RetLen);
			else
			{
				HANDLE hSubKey1=NULL;
				UNICODE_STRING uniSubKeyName={(USHORT)KeyBasInf->NameLength,PAGE_SIZE,KeyBasInf->Name};
				NoirDebugPrint("ACPI Registry Key Name (Lv1): %wZ\n",&uniSubKeyName);
				oa.RootDirectory=hKey;
				oa.ObjectName=&uniSubKeyName;
				st=ZwOpenKey(&hSubKey1,GENERIC_READ,&oa);
				if(NT_SUCCESS(st))
				{
					st=ZwEnumerateKey(hSubKey1,0,KeyBasicInformation,KeyBasInf,PAGE_SIZE,&RetLen);
					if(NT_ERROR(st))
						NoirDebugPrint("Failed to enumerate %wZ! Status=0x%X, Return-Length=0x%X\n",&uniSubKeyName,st,RetLen);
					else
					{
						HANDLE hSubKey2=NULL;
						uniSubKeyName.Length=(USHORT)KeyBasInf->NameLength;
						oa.RootDirectory=hSubKey1;
						NoirDebugPrint("ACPI Registry Key Name (Lv2): %wZ\n",&uniSubKeyName);
						st=ZwOpenKey(&hSubKey2,GENERIC_READ,&oa);
						if(NT_SUCCESS(st))
						{
							st=ZwEnumerateKey(hSubKey2,0,KeyBasicInformation,KeyBasInf,PAGE_SIZE,&RetLen);
							if(NT_ERROR(st))
								NoirDebugPrint("Failed to enumerate %wZ! Status=0x%X, Return-Length=0x%X\n",&uniSubKeyName,st,RetLen);
							else
							{
								HANDLE hSubKey3=NULL;
								uniSubKeyName.Length=(USHORT)KeyBasInf->NameLength;
								oa.RootDirectory=hSubKey2;
								NoirDebugPrint("ACPI Registry Key Name (Lv3): %wZ\n",&uniSubKeyName);
								st=ZwOpenKey(&hSubKey3,GENERIC_READ,&oa);
								if(NT_SUCCESS(st))
								{
									UNICODE_STRING uniKvName=RTL_CONSTANT_STRING(L"00000000");
									PKEY_VALUE_PARTIAL_INFORMATION KvPartInfo=(PKEY_VALUE_PARTIAL_INFORMATION)KeyBasInf;
									st=ZwQueryValueKey(hSubKey3,&uniKvName,KeyValuePartialInformation,KvPartInfo,PAGE_SIZE,&RetLen);
									if(NT_SUCCESS(st))
									{
										if(KvPartInfo->Type!=REG_BINARY)
										{
											NoirDebugPrint("Unknown Key-Value type of ACPI RSDT: 0x%X!\n",KvPartInfo->Type);
											st=STATUS_UNSUCCESSFUL;
										}
										else
										{
											*Length=(SIZE_T)KvPartInfo->DataLength;
											*Rsdt=NoirAllocateNonPagedMemory(KvPartInfo->DataLength);
											if(*Rsdt)
												RtlCopyMemory(*Rsdt,KvPartInfo->Data,KvPartInfo->DataLength);
											else
											{
												NoirDebugPrint("Failed to copy ACPI Root Table!");
												st=STATUS_INSUFFICIENT_RESOURCES;
											}
										}
									}
									ZwClose(hSubKey3);
								}
							}
							ZwClose(hSubKey2);
						}
					}
					ZwClose(hSubKey1);
				}
			}
			ZwClose(hKey);
		}
		NoirFreePagedMemory(KeyBasInf);
	}
	return st;
}

void* noir_locate_acpi_rsdt(size_t *length)
{
	PVOID Rsdt=NULL;
	SIZE_T Length=0;
	NTSTATUS st=NoirCopyAcpiTableRootFromRegistry(&Rsdt,&Length);
	NoirDebugPrint("Copy ACPI Table Root Status=0x%X\n",st);
	if(ARGUMENT_PRESENT(length))*length=Length;
	return Rsdt;
}

ULONG32 noir_get_instruction_length(IN PVOID code,IN BOOLEAN LongMode)
{
	if(LongMode)
		return NoirGetInstructionLength64(code,0);
	return NoirGetInstructionLength32(code,0);
}

ULONG32 noir_get_instruction_length_ex(IN PVOID Code,IN BYTE Bits)
{
	switch(Bits)
	{
	case 16:
		return NoirGetInstructionLength16(Code,0);
	case 32:
		return NoirGetInstructionLength32(Code,0);
	case 64:
		return NoirGetInstructionLength64(Code,0);
	default:
		return 0;
	}
}

ULONG32 noir_disasm_instruction(IN PVOID Code,OUT PSTR Mnemonic,IN SIZE_T MnemonicLength,IN BYTE Bits,IN ULONG64 VirtualAddress)
{
	switch(Bits)
	{
	case 16:
		return NoirDisasmCode16(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	case 32:
		return NoirDisasmCode32(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	case 64:
		return NoirDisasmCode64(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	default:
		return 0;
	}
}

void* noir_alloc_contd_memory(size_t length)
{
	PHYSICAL_ADDRESS L={0};
	PHYSICAL_ADDRESS H={0xFFFFFFFFFFFFFFFF};
	PHYSICAL_ADDRESS B={0};
	PVOID p=MmAllocateContiguousMemorySpecifyCacheNode(length,L,H,B,MmCached,MM_ANY_NODE_OK);
	// PVOID p=MmAllocateContiguousMemory(length,H);
	if(p)
	{
		RtlZeroMemory(p,length);
		InterlockedIncrement(&NoirAllocatedContiguousMemoryCount);
	}
	return p;
}

void* noir_alloc_nonpg_memory(size_t length)
{
	return NoirAllocateNonPagedMemory(length);
}

void* noir_alloc_paged_memory(size_t length)
{
	return NoirAllocatePagedMemory(length);
}

void noir_free_contd_memory(void* virtual_address,size_t length)
{
	MmFreeContiguousMemorySpecifyCache(virtual_address,length,MmCached);
	InterlockedDecrement(&NoirAllocatedContiguousMemoryCount);
}

void noir_free_nonpg_memory(void* virtual_address)
{
	NoirFreeNonPagedMemory(virtual_address);
}

void noir_free_paged_memory(void* virtual_address)
{
	NoirFreePagedMemory(virtual_address);
}

void noir_get_locked_range(PMDL Mdl,void** virt,PULONG bytes)
{
	*virt=MmGetMdlVirtualAddress(Mdl);
	*bytes=MmGetMdlByteCount(Mdl);
}

void noir_unlock_pages(PMDL Mdl)
{
	MmUnlockPages(Mdl);
	IoFreeMdl(Mdl);
}

PMDL noir_lock_pages(void* virt,ULONG32 bytes,PULONG64 phys)
{
	// Use MDL to lock memories...
	PMDL pMdl=IoAllocateMdl(virt,bytes,FALSE,FALSE,NULL);
	if(pMdl)
	{
		ULONG Pages=ADDRESS_AND_SIZE_TO_SPAN_PAGES(virt,bytes);
		PPFN_NUMBER PfnArray;
		__try
		{
			MmProbeAndLockPages(pMdl,KernelMode,IoWriteAccess);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			IoFreeMdl(pMdl);
			return NULL;
		}
		PfnArray=MmGetMdlPfnArray(pMdl);
		for(ULONG i=0;i<Pages;i++)phys[i]=PfnArray[i]<<PAGE_SHIFT;
	}
	return pMdl;
}

ULONG64 noir_get_physical_address(void* virtual_address)
{
	PHYSICAL_ADDRESS pa;
	pa=MmGetPhysicalAddress(virtual_address);
	return pa.QuadPart;
}

ULONG64 noir_get_user_physical_address(void* virtual_address)
{
	PHYSICAL_ADDRESS pa={0};
	PMDL pMdl=IoAllocateMdl(virtual_address,1,FALSE,FALSE,NULL);
	if(pMdl)
	{
		__try
		{
			PVOID Buffer;
			MmProbeAndLockPages(pMdl,KernelMode,IoWriteAccess);
			Buffer=MmMapLockedPagesSpecifyCache(pMdl,KernelMode,MmCached,NULL,FALSE,HighPagePriority);
			if(Buffer)
			{
				pa=MmGetPhysicalAddress(Buffer);
				MmUnmapLockedPages(Buffer,pMdl);
			}
			MmUnlockPages(pMdl);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			;
		}
		IoFreeMdl(pMdl);
	}
	return pa.QuadPart;
}

// Query Page information
BOOL noir_query_page_attributes(IN PVOID virtual_address,OUT PBOOLEAN valid,OUT PBOOLEAN locked,OUT PBOOLEAN large_page)
{
	MEMORY_WORKING_SET_EX_BLOCK Info;
	NTSTATUS st=NoirGetPageInformation(virtual_address,&Info);
	if(NT_SUCCESS(st))
	{
		*valid=(BOOLEAN)Info.Valid;
		*locked=(BOOLEAN)Info.Locked;
		*large_page=(BOOLEAN)Info.LargePage;
		return TRUE;
	}
	return FALSE;
}

void noir_copy_memory(void* dest,void* src,size_t cch)
{
	RtlCopyMemory(dest,src,cch);
}

void* noir_find_virt_by_phys(ULONG64 physical_address)
{
	PHYSICAL_ADDRESS pa;
	pa.QuadPart=physical_address;
	/*
	  The WDK Document claims that MmGetVirtualForPhysical
	  is a routine reserved for system use.
	  However, I think this is the perfect solution to resolve
	  the physical address issue in virtualization nesting.

	  As described in WRK v1.2, MmGetVirtualForPhysical could run
	  at any IRQL, given that the address is in system space.
	*/
	return MmGetVirtualForPhysical(pa);
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

/*
// NoirVisor should be aware of large-scale systems with NUMA.
void* noir_alloc_contd_memory_for_numa(ULONG32 numa_node,ULONG64 alignment,size_t length)
{
	PHYSICAL_ADDRESS L={0};
	PHYSICAL_ADDRESS H={0xFFFFFFFFFFFFFFFFui64};
	PHYSICAL_ADDRESS B={alignment};
	PVOID p=MmAllocateContiguousMemorySpecifyCacheNode(length,L,H,B,MmCached,numa_node);
	if(p)RtlZeroMemory(p,length);
	return p;
}
*/

void noir_enum_physical_memory_ranges(IN NOIR_PHYSICAL_MEMORY_RANGE_CALLBACK CallbackRoutine,IN OUT PVOID Context)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PKEY_VALUE_PARTIAL_INFORMATION KvPartInfo=NoirAllocatePagedMemory(PAGE_SIZE);
	if(KvPartInfo)
	{
		HANDLE hKey=NULL;
		UNICODE_STRING uniKeyName=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Hardware\\RESOURCEMAP\\System Resources\\Physical Memory");
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa,&uniKeyName,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
		st=ZwOpenKey(&hKey,GENERIC_READ,&oa);
		if(NT_SUCCESS(st))
		{
			UNICODE_STRING uniKvName=RTL_CONSTANT_STRING(L".Translated");
			ULONG KvRetLen;
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInfo,PAGE_SIZE,&KvRetLen);
			if(NT_SUCCESS(st))
			{
				PCM_RESOURCE_LIST CmResList=(PCM_RESOURCE_LIST)KvPartInfo->Data;
				for(ULONG i=0;i<CmResList->List->PartialResourceList.Count;i++)
				{
					PCM_PARTIAL_RESOURCE_DESCRIPTOR Prd=&CmResList->List->PartialResourceList.PartialDescriptors[i];
					switch(Prd->Type)
					{
						case CmResourceTypeMemory:
						{
							CallbackRoutine(Prd->u.Memory.Start.QuadPart,(ULONG64)Prd->u.Memory.Length,Context);
							break;
						}
						case CmResourceTypeMemoryLarge:
						{
							ULONG64 Length=(ULONG64)Prd->u.Memory.Length;
							if(Prd->Flags & CM_RESOURCE_MEMORY_LARGE_40)
								Length<<=8ui64;
							else if(Prd->Flags & CM_RESOURCE_MEMORY_LARGE_48)
								Length<<=16ui64;
							else if(Prd->Flags & CM_RESOURCE_MEMORY_LARGE_64)
								Length<<=32ui32;
							else
							{
								NoirDebugPrint("Warning: Unknown Flags (0x%04X) was encountered! Skipping it...\n",Prd->Flags);
								break;
							}
							CallbackRoutine(Prd->u.Memory.Start.QuadPart,Length,Context);
							break;
						}
						default:
						{
							NoirDebugPrint("Unknown Resource Type: 0x%X!\n",Prd->Type);
							break;
						}
					}
				}
			}
			else
			{
				NoirDebugPrint("Failed to query physical memory layout! Status=0x%X, Length: 0x%X\n",st,KvRetLen);
				__debugbreak();
			}
			ZwClose(hKey);
		}
		else
		{
			NoirDebugPrint("Failed to query physical memory layout! Status=0x%X\n");
			__debugbreak();
		}
		NoirFreePagedMemory(KvPartInfo);
	}
	NoirDebugPrint("Status of Query Physical-Memory Ranges: 0x%X\n",st);
}

ULONG64 noir_get_current_process_cr3()
{
	PEPROCESS Process=PsGetCurrentProcess();
#if defined(_WIN64)
	// Note that KPROCESS+0x110 also points to a paging base, but it
	// does not map kernel mode address space except syscall handler.
	return *(PULONG64)((ULONG_PTR)Process+0x28);
#else
	return *(PULONG64)((ULONG_PTR)Process+0x18);
#endif
}

// Some Additional repetitive functions
ULONG64 NoirGetPhysicalAddress(IN PVOID VirtualAddress)
{
	PHYSICAL_ADDRESS pa=MmGetPhysicalAddress(VirtualAddress);
	return pa.QuadPart;
}

ULONG64 noir_get_system_time()
{
	LARGE_INTEGER Time;
	KeQuerySystemTime(&Time);
	return Time.QuadPart;
}

// Essential Multi-Threading Facility.
HANDLE noir_create_thread(IN PKSTART_ROUTINE StartRoutine,IN PVOID Context)
{
	OBJECT_ATTRIBUTES oa;
	HANDLE hThread=NULL;
	InitializeObjectAttributes(&oa,NULL,OBJ_KERNEL_HANDLE,NULL,NULL);
	PsCreateSystemThread(&hThread,SYNCHRONIZE,&oa,NULL,NULL,StartRoutine,Context);
	return hThread;
}

void noir_exit_thread(IN NTSTATUS Status)
{
	PsTerminateSystemThread(Status);
}

BOOLEAN noir_join_thread(IN HANDLE ThreadHandle)
{
	NTSTATUS st=ZwWaitForSingleObject(ThreadHandle,FALSE,NULL);
	if(st==STATUS_SUCCESS)
	{
		ZwClose(ThreadHandle);
		return TRUE;
	}
	return FALSE;
}

BOOLEAN noir_alert_thread(IN HANDLE ThreadHandle)
{
	NTSTATUS st=ZwAlertThread(ThreadHandle);
	return st==STATUS_SUCCESS;
}

// Sleep
void noir_sleep(IN ULONG64 ms)
{
	LARGE_INTEGER Time;
	Time.QuadPart=ms*(-10000);
	KeDelayExecutionThread(KernelMode,TRUE,&Time);
}

// Resource Lock (R/W Lock)
PERESOURCE noir_initialize_reslock()
{
	PERESOURCE Res=NoirAllocateNonPagedMemory(sizeof(ERESOURCE));
	if(Res)
	{
		NTSTATUS st=ExInitializeResourceLite(Res);
		if(NT_ERROR(st))
		{
			NoirFreeNonPagedMemory(Res);
			Res=NULL;
		}
	}
	return Res;
}

void noir_finalize_reslock(IN PERESOURCE Resource)
{
	if(Resource)
	{
		ExDeleteResourceLite(Resource);
		NoirFreeNonPagedMemory(Resource);
	}
}

void noir_acquire_reslock_shared(IN PERESOURCE Resource)
{
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(Resource,TRUE);
}

void noir_acquire_reslock_shared_ex(IN PERESOURCE Resource)
{
	KeEnterCriticalRegion();
	ExAcquireSharedStarveExclusive(Resource,TRUE);
}

void noir_acquire_reslock_exclusive(IN PERESOURCE Resource)
{
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite(Resource,TRUE);
}

void noir_release_reslock(IN PERESOURCE Resource)
{
	ExReleaseResourceLite(Resource);
	KeLeaveCriticalRegion();
}

void noir_acquire_pushlock_exclusive(IN PEX_PUSH_LOCK PushLock)
{
	KeEnterCriticalRegion();
	ExfAcquirePushLockExclusive(PushLock);
}

void noir_acquire_pushlock_shared(IN PEX_PUSH_LOCK PushLock)
{
	KeEnterCriticalRegion();
	ExfAcquirePushLockShared(PushLock);
}

void noir_release_pushlock_exclusive(IN PEX_PUSH_LOCK PushLock)
{
	ExfReleasePushLockExclusive(PushLock);
	KeLeaveCriticalRegion();
}

void noir_release_pushlock_shared(IN PEX_PUSH_LOCK PushLock)
{
	ExfReleasePushLockShared(PushLock);
	KeLeaveCriticalRegion();
}

// Standard I/O
void noir_qsort(IN PVOID base,IN ULONG num,IN ULONG width,IN noir_sorting_comparator comparator)
{
	qsort(base,num,width,comparator);
}