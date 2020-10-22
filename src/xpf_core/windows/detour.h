/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is an auxiliary library for hooking facility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/detour.h
*/

#include <ntddk.h>
#include <windef.h>
#include <ntimage.h>

typedef struct _KLDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	PVOID ExceptionTable;
	ULONG ExceptionTableSize;
	PVOID GpValue;
	PNON_PAGED_DEBUG_INFO NonPagedDebugInfo;
	PVOID ImageBase;
	PVOID EntryPoint;
	ULONG ImageSize;
	UNICODE_STRING FullImageName;
	UNICODE_STRING BaseImageName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	PVOID SectionPointer;
	PVOID LoadedImports;
	PVOID PatchInformation;
}KLDR_DATA_TABLE_ENTRY,*PKLDR_DATA_TABLE_ENTRY;

void __cdecl NoirDebugPrint(const char* Format,...);
BYTE NoirDisasmCode32(PSTR Mnemonic,SIZE_T MnemonicLength,PBYTE Code,SIZE_T CodeLength,ULONG64 VirtualAddress);
BYTE NoirDisasmCode64(PSTR Mnemonic,SIZE_T MnemonicLength,PBYTE Code,SIZE_T CodeLength,ULONG64 VirtualAddress);
BYTE NoirGetInstructionLength32(BYTE* Code,SIZE_T CodeLength);
BYTE NoirGetInstructionLength64(BYTE* Code,SIZE_T CodeLength);

PKLDR_DATA_TABLE_ENTRY PsLoadedModuleList=NULL;
PERESOURCE PsLoadedModuleResource=NULL;