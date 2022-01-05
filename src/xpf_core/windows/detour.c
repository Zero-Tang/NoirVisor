/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is an auxiliary library for hooking facility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/detour.c
*/

#include <ntddk.h>
#include <windef.h>
#include <ntimage.h>
#include "detour.h"

ULONG GetPatchSize(IN PVOID Code,IN ULONG HookLength)
{
	ULONG s=0,l=0;
	while(s<HookLength)
	{
		PVOID p=(PVOID)((ULONG_PTR)Code+s);
#if defined(_WIN64)
		s+=NoirGetInstructionLength64(p,0);
#else
		s+=NoirGetInstructionLength32(p,0);
#endif
	}
	return s;
}

PVOID NoirLocateImageBaseByName(IN PWSTR ImageName)
{
	PVOID ImageBase=NULL;
	if(PsLoadedModuleResource && PsLoadedModuleList)
	{
		// We traverse the Kernel LDR Double-Linked List.
		PKLDR_DATA_TABLE_ENTRY pLdr=(PKLDR_DATA_TABLE_ENTRY)PsLoadedModuleList->InLoadOrderLinks.Flink;
		UNICODE_STRING uniModName;
		RtlInitUnicodeString(&uniModName,ImageName);
		// Acquire Shared Lock.
		KeEnterCriticalRegion();
		if(ExAcquireResourceSharedLite(PsLoadedModuleResource,TRUE))
		{
			do
			{
				if(RtlEqualUnicodeString(&uniModName,&pLdr->BaseImageName,TRUE))
				{
					ImageBase=pLdr->ImageBase;
					break;
				}
				pLdr=(PKLDR_DATA_TABLE_ENTRY)pLdr->InLoadOrderLinks.Flink;
			}while(pLdr!=PsLoadedModuleList);
			// Release Lock.
			ExReleaseResourceLite(PsLoadedModuleResource);
		}
		KeLeaveCriticalRegion();
	}
	return ImageBase;
}

PVOID NoirLocateExportedProcedureByName(IN PVOID ImageBase,IN PSTR ProcedureName)
{
	// Check DOS Header Magic Number
	PIMAGE_DOS_HEADER DosHead=(PIMAGE_DOS_HEADER)ImageBase;
	if(DosHead->e_magic==IMAGE_DOS_SIGNATURE)
	{
		// Check NT Header Signature
		PIMAGE_NT_HEADERS NtHead=(PIMAGE_NT_HEADERS)((ULONG_PTR)ImageBase+DosHead->e_lfanew);
		if(NtHead->Signature==IMAGE_NT_SIGNATURE)
		{
			if(NtHead->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
			{
				// Locate Export Directory
				PIMAGE_EXPORT_DIRECTORY ExpDir=(PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)ImageBase+NtHead->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
				PULONG FuncRva=(PULONG)((ULONG_PTR)ImageBase+ExpDir->AddressOfFunctions);
				PULONG NameRva=(PULONG)((ULONG_PTR)ImageBase+ExpDir->AddressOfNames);
				PUSHORT OrdRva=(PUSHORT)((ULONG_PTR)ImageBase+ExpDir->AddressOfNameOrdinals);
				ULONG Low=0,High=ExpDir->NumberOfNames;
				// Use Binary-Search to reduce running-time complexity
				while(High>=Low)
				{
					ULONG Mid=(Low+High)>>1;
					PSTR CurrentName=(PSTR)((ULONG_PTR)ImageBase+NameRva[Mid]);
					// strcmp can compare strings greater or lower.
					LONG CompareResult=strcmp(ProcedureName,CurrentName);
					if(CompareResult<0)
						High=Mid-1;
					else if(CompareResult>0)
						Low=Mid+1;
					else
						return (PVOID)((ULONG_PTR)ImageBase+FuncRva[OrdRva[Mid]]);
				}
			}
		}
	}
	return NULL;
}

// This (might be) hardcode searching method.
// First lea instruction references this variable.
// This is for thread-safe consideration.
void static NoirLocatePsLoadedModuleResource()
{
	UNICODE_STRING uniFuncName=RTL_CONSTANT_STRING(L"MmLockPagableDataSection");
	ULONG_PTR p1=(ULONG_PTR)MmGetSystemRoutineAddress(&uniFuncName);
	if(p1)
	{
		ULONG Len=0;
		// Search Instruction
		for(ULONG_PTR p2=p1;p2<p1+0x200;p2+=Len)
		{
#if defined(_WIN64)
			Len=NoirGetInstructionLength64((PBYTE)p2,0);
			// Compare if current instruction is "lea rcx,xxxx"
			// 48 8D 0D XX XX XX XX		lea rcx,PsLoadedModuleResource
			if(Len==7 && *(PUSHORT)p2==0x8D48 && *(PBYTE)(p2+2)==0xD)
			{
				PsLoadedModuleResource=(PERESOURCE)(*(PLONG)(p2+3)+p2+7);
				break;
			}
#endif
		}
	}
}

void static NoirLocatePsLoadedModuleList(IN PDRIVER_OBJECT DriverObject)
{
	PKLDR_DATA_TABLE_ENTRY pLdr=(PKLDR_DATA_TABLE_ENTRY)DriverObject->DriverSection;
	if(PsLoadedModuleResource)
	{
		// Acquiring Locking Primitive...
		KeEnterCriticalRegion();
		// We are reading the list, so acquire shared lock.
		if(ExAcquireResourceSharedLite(PsLoadedModuleResource,TRUE))
		{
			PKLDR_DATA_TABLE_ENTRY tLdr=(PKLDR_DATA_TABLE_ENTRY)pLdr->InLoadOrderLinks.Blink;
			while(pLdr!=tLdr)
			{
				if(pLdr->ImageSize==0)	// I assert it is zero here, not sure if there could be exceptions.
				{
					PsLoadedModuleList=pLdr;
					break;
				}
				pLdr=(PKLDR_DATA_TABLE_ENTRY)pLdr->InLoadOrderLinks.Flink;
			}
			// Releasing Locking Primitive...
			ExReleaseResourceLite(PsLoadedModuleResource);
		}
		KeLeaveCriticalRegion();
	}
}

void NoirLocatePsLoadedModule(IN PDRIVER_OBJECT DriverObject)
{
	// Since Windows 10 Redstone, these two variables are exported.
	UNICODE_STRING uniVarName1=RTL_CONSTANT_STRING(L"PsLoadedModuleList");
	UNICODE_STRING uniVarName2=RTL_CONSTANT_STRING(L"PsLoadedModuleResource");
	PsLoadedModuleList=MmGetSystemRoutineAddress(&uniVarName1);
	PsLoadedModuleResource=MmGetSystemRoutineAddress(&uniVarName2);
	// If not exported, then find them on our own...
	if(PsLoadedModuleResource==NULL)
		NoirLocatePsLoadedModuleResource();
	if(PsLoadedModuleList==NULL)
		NoirLocatePsLoadedModuleList(DriverObject);
	// Print them to debug-console for debugging diagnostics purpose...
	NoirDebugPrint("PsLoadedModuleList: 0x%p\t PsLoadedModuleResource: 0x%p\n",PsLoadedModuleList,PsLoadedModuleResource);
}