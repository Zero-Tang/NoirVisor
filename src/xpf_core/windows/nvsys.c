/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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

// Asynchronous Debug-Printer Implementation...
// In the context of VM-Exit, debug-printing is not permitted.
// FIXME: Some debug log messages are missing.

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
	PNOIR_ASYNC_DEBUG_LOG_MONITOR LogMonitor=(PNOIR_ASYNC_DEBUG_LOG_MONITOR)StartContext;
	PSTR Level[4]={"Error","Warning","Trace","Info"};
	KAFFINITY Affinity=(KAFFINITY)(1<<LogMonitor->ProcessorId);
	LARGE_INTEGER Delay;
	Delay.QuadPart=NOIR_DEBUG_PRINT_DELAY*(-10000);
	KeSetSystemAffinityThread(Affinity);
	NoirDebugPrint("Asynchronous Logger Thread is listening on Processor Core %03u!\n",LogMonitor->ProcessorId);
	while(InterlockedCompareExchange(&LogMonitor->TerminationSignal,1,1)==0)
	{
		ULONG i=0;
		// Turn off interrupts to prevent the worker thread from being scheduled out.
		_disable();
		// Traverse the log records then print them.
		while(i<LogMonitor->RecordCount)
		{
			PNOIR_DEBUG_LOG_RECORD LogRecord=LogMonitor->LogInfo;
			UCHAR Log[512];
			LARGE_INTEGER Time;
			TIME_FIELDS TimeFields;
			ExSystemTimeToLocalTime(&LogRecord->LogTime,&Time);
			RtlTimeToTimeFields(&Time,&TimeFields);
			RtlStringCchPrintfA(Log,sizeof(Log),"[NoirVisor - Async Log | Core %03u | %s]\t | %04u-%02u-%02u %02u:%02u:%02u.%03u | %s",
				LogMonitor->ProcessorId,Level[LogRecord->Level],TimeFields.Year,TimeFields.Month,TimeFields.Day,
				TimeFields.Hour,TimeFields.Minute,TimeFields.Second,TimeFields.Milliseconds,LogRecord->Message);
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,LogRecord->Level,Log);
			if(++i==NOIR_DEBUG_LOG_RECORD_LIMIT)break;
		}
		if(LogMonitor->RecordCount>=NOIR_DEBUG_LOG_RECORD_LIMIT)
		{
			ULONG DroppedCount=LogMonitor->RecordCount-NOIR_DEBUG_LOG_RECORD_LIMIT+1;
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"[NoirVisor - Async Log | Core %03u | Panic] There are %u debug log record dropped!\n",LogMonitor->ProcessorId,DroppedCount);
		}
		LogMonitor->RecordCount=0;
		// Turn on interrupts to sleep properly.
		_enable();
		// Sleep.
		KeDelayExecutionThread(KernelMode,TRUE,&Delay);
	}
	KeRevertToUserAffinityThread();
	PsTerminateSystemThread(STATUS_SUCCESS);
}

void NoirFinalizeAsyncDebugPrinter()
{
	if(NoirAsyncDebugLogger)
	{
		ULONG NumberOfProcessors=KeQueryActiveProcessorCount(NULL);
		// Join the threads so freeing the buffer does not encounter race conditions.
		for(ULONG32 i=0;i<NumberOfProcessors;i++)
		{
			if(NoirAsyncDebugLogger[i].ThreadHandle)
			{
				// Signal the monitor thread to be ready for termination.
				InterlockedIncrement(&NoirAsyncDebugLogger[i].TerminationSignal);
				// Wake the thread from timer-sleeping.
				ZwAlertThread(NoirAsyncDebugLogger[i].ThreadHandle);
				// Join the thread.
				ZwWaitForSingleObject(NoirAsyncDebugLogger[i].ThreadHandle,FALSE,NULL);
				ZwClose(NoirAsyncDebugLogger[i].ThreadHandle);
			}
			if(NoirAsyncDebugLogger[i].LogInfo)NoirFreeLoggerBuffer(NoirAsyncDebugLogger[i].LogInfo);
		}
		NoirFreeLoggerBuffer(NoirAsyncDebugLogger);
	}
}

NTSTATUS NoirInitializeAsyncDebugPrinter()
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	ULONG NumberOfProcessors=KeQueryActiveProcessorCount(NULL);
	NoirAsyncDebugLogger=NoirAllocateLoggerBuffer(sizeof(NOIR_ASYNC_DEBUG_LOG_MONITOR)*NumberOfProcessors);
	if(NoirAsyncDebugLogger)
	{
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa,NULL,OBJ_KERNEL_HANDLE,NULL,NULL);
		for(ULONG32 i=0;i<NumberOfProcessors;i++)
		{
			NoirAsyncDebugLogger[i].LogInfo=NoirAllocateLoggerBuffer(sizeof(NOIR_DEBUG_LOG_RECORD)*NOIR_DEBUG_LOG_RECORD_LIMIT);
			if(NoirAsyncDebugLogger[i].LogInfo==NULL)
			{
				st=STATUS_INSUFFICIENT_RESOURCES;
				NoirFinalizeAsyncDebugPrinter();
				return st;
			}
			NoirAsyncDebugLogger[i].ProcessorId=i;
			NoirAsyncDebugLogger[i].RecordCount=0;
			NoirAsyncDebugLogger[i].TerminationSignal=0;
			st=PsCreateSystemThread(&NoirAsyncDebugLogger[i].ThreadHandle,SYNCHRONIZE,&oa,NULL,NULL,NoirAsyncDebugLogThreadWorker,&NoirAsyncDebugLogger[i]);
			if(NT_ERROR(st))
			{
				NoirFinalizeAsyncDebugPrinter();
				return st;
			}
		}
	}
	return st;
}

void __cdecl NoirAsyncDebugVPrint(ULONG FilterLevel,const char* Format,va_list ArgList)
{
	ULONG ProcNum=KeGetCurrentProcessorNumber();
	ULONG Index=NoirAsyncDebugLogger[ProcNum].RecordCount++;
	PNOIR_DEBUG_LOG_RECORD LogRecord=&NoirAsyncDebugLogger[ProcNum].LogInfo[Index%NOIR_DEBUG_LOG_RECORD_LIMIT];
	// Record the debug logging messages.
	KeQuerySystemTime(&LogRecord->LogTime);
	LogRecord->Level=FilterLevel;
	// Format the logging message string.
	RtlStringCchVPrintfA(LogRecord->Message,sizeof(LogRecord->Message),Format,ArgList);
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

void NoirFreeNonPagedMemory(IN PVOID VirtualAddress)
{
	ExFreePoolWithTag(VirtualAddress,'pNvN');
	InterlockedDecrement(&NoirAllocatedNonPagedPools);
}

void NoirFreePagedMemory(IN PVOID VirtualAddress)
{
	ExFreePoolWithTag(VirtualAddress,'gPvN');
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

void noir_free_contd_memory(void* virtual_address,size_t length)
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

// Standard I/O
void noir_qsort(IN PVOID base,IN ULONG num,IN ULONG width,IN noir_sorting_comparator comparator)
{
	qsort(base,num,width,comparator);
}