/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

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

// Asynchronous Debug-Printer
// For Hypervisor
PVOID NoirAllocateLoggerBuffer(IN ULONG Size)
{
	PVOID p=ExAllocatePoolWithTag(NonPagedPool,Size,'gLvN');
	if(p)RtlZeroMemory(p,Size);
	return p;
}

void NoirFreeLoggerBuffer(IN PVOID Buffer)
{
	ExFreePoolWithTag(Buffer,'gLvN');
}

void NoirAsyncDebugLogThreadWorker(IN PVOID StartContext)
{
	PSTR Level[4]={"Error","Warning","Trace","Info"};
	PNOIR_ASYNC_DEBUG_LOG_MONITOR LogMonitor=(PNOIR_ASYNC_DEBUG_LOG_MONITOR)StartContext;
	LARGE_INTEGER Delay;
	Delay.QuadPart=NOIR_DEBUG_PRINT_DELAY*(-10000);
	KeSetSystemAffinityThread(1i64<<LogMonitor->ProcessorNumber);
	while(InterlockedCompareExchange(&LogMonitor->Signal,1,1)==0)
	{
		ULONG i=0;
		while(i<LogMonitor->RecordCount)
		{
			UCHAR Log[512];
			LARGE_INTEGER Time;
			TIME_FIELDS TimeFields;
			ExSystemTimeToLocalTime(&LogMonitor->LogInfo[i].LogTime,&Time);
			RtlTimeToTimeFields(&Time,&TimeFields);
			RtlStringCchPrintfA(Log,sizeof(Log),"[NoirVisor Async Log %s - Core %d]\t|%04d-%02d-%02d %02d:%02d:%02d.%03d| %s",
				Level[LogMonitor->LogInfo[i].Level],LogMonitor->ProcessorNumber,
				TimeFields.Year,TimeFields.Month,TimeFields.Day,
				TimeFields.Hour,TimeFields.Minute,TimeFields.Second,
				TimeFields.Milliseconds,LogMonitor->LogInfo[i].LogInfo);
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,LogMonitor->LogInfo[i].Level,Log);
			if(++i==NOIR_DEBUG_LOG_RECORD_LIMIT)break;
		}
		if(LogMonitor->RecordCount>=NOIR_DEBUG_LOG_RECORD_LIMIT)
		{
			ULONG DroppedCount=LogMonitor->RecordCount-NOIR_DEBUG_LOG_RECORD_LIMIT-1;
			nv_panicf("[Core %d] There are %d debug log record dropped!\n",LogMonitor->ProcessorNumber,DroppedCount);
		}
		RtlZeroMemory(LogMonitor->LogInfo,sizeof(NOIR_DEBUG_LOG_RECORD)*NOIR_DEBUG_LOG_RECORD_LIMIT);
		LogMonitor->RecordCount=0;
		KeDelayExecutionThread(KernelMode,TRUE,&Delay);
	}
	KeRevertToUserAffinityThread();
	PsTerminateSystemThread(STATUS_SUCCESS);
}

void NoirFinalizeAsyncDebugPrinter()
{
	if(NoirAsyncDebugLogger)
	{
		ULONG i=0;
		PNOIR_ASYNC_DEBUG_LOG_MONITOR LogMon=NoirAsyncDebugLogger->LogMonitor;
		for(;i<NoirAsyncDebugLogger->NumberOfProcessors;LogMon=&NoirAsyncDebugLogger->LogMonitor[++i])
		{
			InterlockedIncrement(&LogMon->Signal);	// Signal the monitor thread.
			ZwAlertThread(LogMon->ThreadHandle);	// Alerting the thread will automatically flush the log.
			// Join the thread.
			ZwWaitForSingleObject(LogMon->ThreadHandle,FALSE,NULL);
			ZwClose(LogMon->ThreadHandle);
			// Free log info.
			NoirFreeLoggerBuffer(LogMon->LogInfo);
		}
		NoirFreeLoggerBuffer(NoirAsyncDebugLogger);
	}
}

NTSTATUS NoirInitializeAsyncDebugPrinter(IN BOOLEAN EnableFileLogging)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	KAFFINITY af;
	ULONG Count=KeQueryActiveProcessorCount(&af);
	NoirAsyncDebugLogger=NoirAllocateLoggerBuffer(Count*sizeof(NOIR_ASYNC_DEBUG_LOG_MONITOR)+sizeof(NOIR_ASYNC_DEBUG_LOG_SYSTEM));
	if(NoirAsyncDebugLogger)
	{
		OBJECT_ATTRIBUTES oa;
		ULONG i=0;
		PNOIR_ASYNC_DEBUG_LOG_MONITOR LogMon=NoirAsyncDebugLogger->LogMonitor;
		InitializeObjectAttributes(&oa,NULL,OBJ_KERNEL_HANDLE,NULL,NULL);
		NoirAsyncDebugLogger->NumberOfProcessors=Count;
		NoirAsyncDebugLogger->LogLimit=NOIR_DEBUG_LOG_RECORD_LIMIT;		// Number of logs within 100 milliseconds.
		for(;i<Count;LogMon=&NoirAsyncDebugLogger->LogMonitor[++i])
		{
			LogMon->ProcessorNumber=i;
			st=STATUS_INSUFFICIENT_RESOURCES;
			LogMon->LogInfo=NoirAllocateLoggerBuffer(sizeof(NOIR_DEBUG_LOG_RECORD)*NOIR_DEBUG_LOG_RECORD_LIMIT);
			if(LogMon)st=PsCreateSystemThread(&LogMon->ThreadHandle,SYNCHRONIZE,&oa,NULL,NULL,NoirAsyncDebugLogThreadWorker,LogMon);
			if(NT_ERROR(st))
			{
				NoirFinalizeAsyncDebugPrinter();
				break;
			}
		}
	}
	NoirDebugPrint("Async Debug Printer Initialization Status: 0x%X\n",st);
	return st;
}

void __cdecl NoirAsyncDebugVPrint(ULONG FilterLevel,const char* Format,va_list ArgList)
{
	ULONG PN=KeGetCurrentProcessorNumber();
	PNOIR_ASYNC_DEBUG_LOG_MONITOR LogMon=&NoirAsyncDebugLogger->LogMonitor[PN];
	ULONG Index=++LogMon->RecordCount;
	KeQuerySystemTime(&LogMon->LogInfo[Index].LogTime);
	LogMon->LogInfo[Index].Level=FilterLevel;
	RtlStringCchVPrintfA(LogMon->LogInfo[Index].LogInfo,500,Format,ArgList);
}

void __cdecl nv_async_dprintf(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	NoirAsyncDebugVPrint(DPFLTR_INFO_LEVEL,format,arg_list);
	va_end(arg_list);
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
	PVOID IpbBuffer=ExAllocatePool(NonPagedPool,Num*sizeof(KDPC)+4);
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
		ExFreePool(IpbBuffer);
	}
}

ULONG32 noir_get_instruction_length(IN PVOID code,IN BOOLEAN LongMode)
{
	if(LongMode)
		return NoirGetInstructionLength64(code,0);
	return NoirGetInstructionLength32(code,0);
}

void NoirReportMemoryIntrospectionCounter()
{
	NoirDebugPrint("============NoirVisor Memory Introspection Report Start============\n");
	NoirDebugPrint("Unreleased NonPaged Pools: %d\n",NoirAllocatedNonPagedPools);
	NoirDebugPrint("Unreleased Paged Pools: %d\n",NoirAllocatedPagedPools);
	NoirDebugPrint("Unreleased Contiguous Memory Couunt: %d\n",NoirAllocatedContiguousMemoryCount);
	if(NoirAllocatedNonPagedPools || NoirAllocatedPagedPools || NoirAllocatedContiguousMemoryCount)
		NoirDebugPrint("Memory Leak is detected!\n");
	else
		NoirDebugPrint("No Memory Leaks...\n");
	NoirDebugPrint("=============NoirVisor Memory Introspection Report End=============\n");
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
	PVOID p=ExAllocatePoolWithTag(NonPagedPool,Length,'pNvN');
	if(p)
	{
		RtlZeroMemory(p,Length);
		InterlockedIncrement(&NoirAllocatedNonPagedPools);
	}
	return p;
}

PVOID NoirAllocatePagedMemory(IN SIZE_T Length)
{
	PVOID p=ExAllocatePoolWithTag(PagedPool,Length,'gPvN');
	if(p)
	{
		RtlZeroMemory(p,Length);
		InterlockedIncrement(&NoirAllocatedPagedPools);
	}
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

void NoirFreeNonPagedMemory(void* VirtualAddress)
{
	ExFreePoolWithTag(VirtualAddress,'pNvN');
	InterlockedDecrement(&NoirAllocatedNonPagedPools);
}

void NoirFreePagedMemory(void* virtual_address)
{
	ExFreePoolWithTag(virtual_address,'gPvN');
	InterlockedDecrement(&NoirAllocatedPagedPools);
}

void* noir_alloc_contd_memory(size_t length)
{
	// PHYSICAL_ADDRESS L={0};
	PHYSICAL_ADDRESS H={0xFFFFFFFFFFFFFFFF};
	// PHYSICAL_ADDRESS B={0};
	// PVOID p=MmAllocateContiguousMemorySpecifyCacheNode(length,L,H,B,MmCached,MM_ANY_NODE_OK);
	PVOID p=MmAllocateContiguousMemory(length,H);
	if(p)
	{
		RtlZeroMemory(p,length);
		InterlockedIncrement(&NoirAllocatedContiguousMemoryCount);
	}
	return p;
}

void* noir_alloc_nonpg_memory(size_t length)
{
	PVOID p=ExAllocatePoolWithTag(NonPagedPool,length,'pNvN');
	if(p)
	{
		RtlZeroMemory(p,length);
		InterlockedIncrement(&NoirAllocatedNonPagedPools);
	}
	return p;
}

void* noir_alloc_paged_memory(size_t length)
{
	PVOID p=ExAllocatePoolWithTag(PagedPool,length,'gPvN');
	if(p)
	{
		RtlZeroMemory(p,length);
		InterlockedIncrement(&NoirAllocatedPagedPools);
	}
	return p;
}

void noir_free_contd_memory(void* virtual_address)
{
#if defined(_WINNT5)
	// It is recommended to release contiguous memory at APC level on NT5.
	KIRQL f_oldirql;
	KeRaiseIrql(APC_LEVEL,&f_oldirql);
#endif
	MmFreeContiguousMemory(virtual_address);
	InterlockedDecrement(&NoirAllocatedContiguousMemoryCount);
#if defined(_WINNT5)
	KeLowerIrql(f_oldirql);
#endif
}

void noir_free_nonpg_memory(void* virtual_address)
{
	ExFreePoolWithTag(virtual_address,'pNvN');
	InterlockedDecrement(&NoirAllocatedNonPagedPools);
}

void noir_free_paged_memory(void* virtual_address)
{
	ExFreePoolWithTag(virtual_address,'gPvN');
	InterlockedDecrement(&NoirAllocatedPagedPools);
}

ULONG64 noir_get_physical_address(void* virtual_address)
{
	PHYSICAL_ADDRESS pa;
	pa=MmGetPhysicalAddress(virtual_address);
	return pa.QuadPart;
}

// We need to map physical memory in nesting virtualization.
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

// Some Additional repetitive functions
ULONG64 NoirGetPhysicalAddress(IN PVOID VirtualAddress)
{
	PHYSICAL_ADDRESS pa=MmGetPhysicalAddress(VirtualAddress);
	return pa.QuadPart;
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
	PERESOURCE Res=ExAllocatePool(NonPagedPool,sizeof(ERESOURCE));
	if(Res)
	{
		NTSTATUS st=ExInitializeResourceLite(Res);
		if(NT_ERROR(st))
		{
			ExFreePool(Res);
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
		ExFreePool(Resource);
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

// Standard I/O
void noir_qsort(IN PVOID base,IN ULONG num,IN ULONG width,IN noir_sorting_comparator comparator)
{
	qsort(base,num,width,comparator);
}