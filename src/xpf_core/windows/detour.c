/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

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
		s+=LDE(p,sizeof(void*)*16-64);
	}
	return s;
}

PVOID NoirLocateImageBaseByName(IN PWSTR ImageName)
{
	PVOID ImageBase=NULL;
	if(PsLoadedModuleResource && PsLoadedModuleList)
	{
		PKLDR_DATA_TABLE_ENTRY pLdr=(PKLDR_DATA_TABLE_ENTRY)PsLoadedModuleList->InLoadOrderLinks.Flink;
		UNICODE_STRING uniModName;
		RtlInitUnicodeString(&uniModName,ImageName);
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
			ExReleaseResourceLite(PsLoadedModuleResource);
		}
		KeLeaveCriticalRegion();
	}
	return ImageBase;
}

PVOID NoirLocateExportedProcedureByName(IN PVOID ImageBase,IN PSTR ProcedureName)
{
	PIMAGE_DOS_HEADER DosHead=(PIMAGE_DOS_HEADER)ImageBase;
	if(DosHead->e_magic==IMAGE_DOS_SIGNATURE)
	{
		PIMAGE_NT_HEADERS NtHead=(PIMAGE_NT_HEADERS)((ULONG_PTR)ImageBase+DosHead->e_lfanew);
		if(NtHead->Signature==IMAGE_NT_SIGNATURE)
		{
			if(NtHead->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
			{
				PIMAGE_EXPORT_DIRECTORY ExpDir=(PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)ImageBase+NtHead->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
				PULONG FuncRva=(PULONG)((ULONG_PTR)ImageBase+ExpDir->AddressOfFunctions);
				PULONG NameRva=(PULONG)((ULONG_PTR)ImageBase+ExpDir->AddressOfNames);
				PUSHORT OrdRva=(PUSHORT)((ULONG_PTR)ImageBase+ExpDir->AddressOfNameOrdinals);
				ULONG Low=0,High=ExpDir->NumberOfNames;
				while(High>=Low)
				{
					ULONG Mid=(Low+High)>>1;
					PSTR CurrentName=(PSTR)((ULONG_PTR)ImageBase+NameRva[Mid]);
					LONG CompareResult=strcmp(ProcedureName,CurrentName);
					if(CompareResult<0)
						High=Mid-1;
					else if(CompareResult>0)
						Low=Mid+1;
					else
						return (PVOID)((ULONG_PTR)ImageBase+FuncRva[OrdRva[Mid]]);
					NoirDebugPrint("Compared %s, Not Match!\n",CurrentName);
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
		ULONG_PTR p2=p1;
		ULONG Len=0;
		for(;p2<p1+0x200;p2+=Len)
		{
			Len=LDE((PVOID)p2,sizeof(void*)*16-64);
#if defined(_WIN64)
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
				if(pLdr->ImageSize==0)	// I assert it is zero here. Not sure if there is exceptions.
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
	// In Windows 10 Redstone, these two variables are exported.
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