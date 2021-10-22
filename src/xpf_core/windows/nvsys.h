/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This header file is for NoirVisor's System Function assets of XPF-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/nvsys.h
*/

#include <ntifs.h>
#include <windef.h>

#define NOIR_DEBUG_LOG_RECORD_LIMIT		160
#define NOIR_DEBUG_PRINT_DELAY			2000

typedef union _MEMORY_WORKING_SET_EX_BLOCK
{
	struct
	{
		ULONG_PTR Valid:1;				// Bit	0
		ULONG_PTR ShareCount:3;			// Bits	1-3
		ULONG_PTR Win32Protection:11;	// Bits	4-14
		ULONG_PTR Shared:1;				// Bit	15
		ULONG_PTR Node:6;				// Bits	16-21
		ULONG_PTR Locked:1;				// Bit	22
		ULONG_PTR LargePage:1;			// Bit	23
		ULONG_PTR Priority:3;			// Bits	24-26
		ULONG_PTR Reserved:3;			// Bits	27-29
		ULONG_PTR SharedOriginal:1;		// Bit	30
		ULONG_PTR Bad:1;				// Bit	31
#if defined(_WIN64)
		ULONG64 Win32GraphicsProtection:4;	// Bits	32-35
		ULONG64 ReservedUlong:28;			// Bits	36-63
#endif
	};
	ULONG_PTR Value;
}MEMORY_WORKING_SET_EX_BLOCK,*PMEMORY_WORKING_SET_EX_BLOCK;

typedef struct _NOIR_GPR_STATE
{
	ULONG64 Rax;
	ULONG64 Rcx;
	ULONG64 Rdx;
	ULONG64 Rbx;
	ULONG64 Rsp;
	ULONG64 Rbp;
	ULONG64 Rsi;
	ULONG64 Rdi;
	ULONG64 R8;
	ULONG64 R9;
	ULONG64 R10;
	ULONG64 R11;
	ULONG64 R12;
	ULONG64 R13;
	ULONG64 R14;
	ULONG64 R15;
	ULONG64 Rflags;
	ULONG64 Rip;
}NOIR_GPR_STATE,*PNOIR_GPR_STATE;

typedef struct _NOIR_DEBUG_LOG_RECORD
{
	LARGE_INTEGER LogTime;
	ULONG32 Level;
	CHAR Message[436];
}NOIR_DEBUG_LOG_RECORD,*PNOIR_DEBUG_LOG_RECORD;

typedef struct _NOIR_ASYNC_DEBUG_LOG_MONITOR
{
	ULONG32 ProcessorId;
	ULONG32 RecordCount;
	LONG32 volatile TerminationSignal;
	HANDLE ThreadHandle;
	PNOIR_DEBUG_LOG_RECORD LogInfo;
}NOIR_ASYNC_DEBUG_LOG_MONITOR,*PNOIR_ASYNC_DEBUG_LOG_MONITOR;

typedef void(*noir_broadcast_worker)(void* context,ULONG ProcessorNumber);
typedef LONG(__cdecl *noir_sorting_comparator)(const void* a,const void*b);

NTSYSAPI NTSTATUS NTAPI ZwAlertThread(IN HANDLE ThreadHandle);
BYTE NoirDisasmCode16(PSTR Mnemonic,SIZE_T MnemonicLength,PBYTE Code,SIZE_T CodeLength,ULONG64 VirtualAddress);
BYTE NoirDisasmCode32(PSTR Mnemonic,SIZE_T MnemonicLength,PBYTE Code,SIZE_T CodeLength,ULONG64 VirtualAddress);
BYTE NoirDisasmCode64(PSTR Mnemonic,SIZE_T MnemonicLength,PBYTE Code,SIZE_T CodeLength,ULONG64 VirtualAddress);
BYTE NoirGetInstructionLength16(PBYTE Code,SIZE_T CodeLength);
BYTE NoirGetInstructionLength32(PBYTE Code,SIZE_T CodeLength);
BYTE NoirGetInstructionLength64(PBYTE Code,SIZE_T CodeLength);
NTSTATUS NoirGetPageInformation(IN PVOID PageAddress,OUT PMEMORY_WORKING_SET_EX_BLOCK Information);
BOOLEAN NoirDoSystemCallHook(IN OUT PVOID GprState);

PNOIR_ASYNC_DEBUG_LOG_MONITOR NoirAsyncDebugLogger=NULL;

// Simple Memory Introspection Counters
LONG volatile NoirAllocatedNonPagedPools=0;
LONG volatile NoirAllocatedPagedPools=0;
LONG volatile NoirAllocatedContiguousMemoryCount=0;