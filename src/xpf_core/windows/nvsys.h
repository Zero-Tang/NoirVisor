/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This header file is for NoirVisor's System Function assets of XPF-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/nvsys.h
*/

#include <ntifs.h>
#include <windef.h>

#define NOIR_DEBUG_LOG_RECORD_LIMIT		64
#define NOIR_DEBUG_PRINT_DELAY			100

typedef struct _NOIR_DEBUG_LOG_RECORD
{
	LARGE_INTEGER LogTime;
	ULONG Level;
	UCHAR LogInfo[500];
}NOIR_DEBUG_LOG_RECORD,*PNOIR_DEBUG_LOG_RECORD;

typedef struct _NOIR_ASYNC_DEBUG_LOG_MONITOR
{
	ULONG ProcessorNumber;
	ULONG RecordCount;
	LONG volatile Signal;
	HANDLE ThreadHandle;
	PNOIR_DEBUG_LOG_RECORD LogInfo;
}NOIR_ASYNC_DEBUG_LOG_MONITOR,*PNOIR_ASYNC_DEBUG_LOG_MONITOR;

typedef struct _NOIR_ASYNC_DEBUG_LOG_SYSTEM
{
	ULONG NumberOfProcessors;
	ULONG LogLimit;
	NOIR_ASYNC_DEBUG_LOG_MONITOR LogMonitor[1];
}NOIR_ASYNC_DEBUG_LOG_SYSTEM,*PNOIR_ASYNC_DEBUG_LOG_SYSTEM;

typedef void(*noir_broadcast_worker)(void* context,ULONG ProcessorNumber);
typedef LONG(__cdecl *noir_sorting_comparator)(const void* a,const void*b);

NTSYSAPI NTSTATUS NTAPI ZwAlertThread(IN HANDLE ThreadHandle);
ULONG LDE(IN PVOID Code,IN ULONG Architecture);

PNOIR_ASYNC_DEBUG_LOG_SYSTEM NoirAsyncDebugLogger=NULL;