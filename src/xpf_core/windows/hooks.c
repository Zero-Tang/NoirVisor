/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

  This file is auxiliary to Hooking facility (MSR Hook, Inline Hook).

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/hooks.c
*/

#include <ntddk.h>
#include <windef.h>
#include <ntstrsafe.h>
#include "hooks.h"

NTSTATUS NoirBuildProtectedFile()
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	NoirProtectedFile=ExAllocatePool(NonPagedPool,PAGE_SIZE);
	if(NoirProtectedFile)
	{
		RtlZeroMemory(NoirProtectedFile,PAGE_SIZE);
		st=ExInitializeResourceLite(&NoirProtectedFile->Lock);
		if(NT_SUCCESS(st))
		{
			NoirProtectedFile->MaximumLength=PAGE_SIZE-sizeof(NOIR_PROTECTED_FILE_NAME);
			RtlStringCbCopyW(NoirProtectedFile->FileName,NoirProtectedFile->MaximumLength,L"NoirVisor.sys");
			RtlStringCbLengthW(NoirProtectedFile->FileName,NoirProtectedFile->MaximumLength,&NoirProtectedFile->Length);
		}
	}
	return st;
}

void NoirTeardownProtectedFile()
{
	if(NoirProtectedFile)
	{
		ExDeleteResourceLite(&NoirProtectedFile->Lock);
		ExFreePool(NoirProtectedFile);
	}
}

void NoirSetProtectedFile(IN PWSTR FileName)
{
	if(NoirProtectedFile)
	{
		KeEnterCriticalRegion();
		if(ExAcquireResourceExclusiveLite(&NoirProtectedFile->Lock,TRUE))
		{
			RtlZeroMemory(NoirProtectedFile->FileName,NoirProtectedFile->MaximumLength);
			RtlStringCbCopyW(NoirProtectedFile->FileName,NoirProtectedFile->MaximumLength,FileName);
			RtlStringCbLengthW(NoirProtectedFile->FileName,NoirProtectedFile->MaximumLength,&NoirProtectedFile->Length);
			ExReleaseResourceLite(&NoirProtectedFile->Lock);
		}
		KeLeaveCriticalRegion();
	}
}

BOOLEAN NoirIsProtectedFile(IN PWSTR FilePath)
{
	BOOLEAN Result=FALSE;
	if(NoirProtectedFile)
	{
		KeEnterCriticalRegion();
		if(ExAcquireSharedStarveExclusive(&NoirProtectedFile->Lock,TRUE))
		{
			PWSTR FileName=wcsrchr(FilePath,L'\\');
			if(FileName)
			{
				FileName++;
				Result=(_wcsnicmp(NoirProtectedFile->FileName,FileName,NoirProtectedFile->MaximumLength)==0);
			}
			ExReleaseResourceLite(&NoirProtectedFile->Lock);
		}
		KeLeaveCriticalRegion();
	}
	return Result;
}

NTSTATUS static fake_NtSetInformationFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,IN PVOID FileInformation,IN ULONG Length,IN FILE_INFORMATION_CLASS FileInformationClass)
{
	if(FileInformationClass==FileDispositionInformation)
	{
		NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
		PFILE_NAME_INFORMATION FileNameInfo=ExAllocatePool(PagedPool,PAGE_SIZE);
		if(FileNameInfo)
		{
			RtlZeroMemory(FileNameInfo,PAGE_SIZE);
			st=ZwQueryInformationFile(FileHandle,IoStatusBlock,FileNameInfo,PAGE_SIZE,FileNameInformation);
			if(NT_SUCCESS(st))
			{
				if(NoirIsProtectedFile(FileNameInfo->FileName))
					st=STATUS_DEVICE_PAPER_EMPTY;
				else
					st=Old_NtSetInformationFile(FileHandle,IoStatusBlock,FileInformation,Length,FileInformationClass);
			}
			ExFreePool(FileNameInfo);
		}
		return st;
	}
	return Old_NtSetInformationFile(FileHandle,IoStatusBlock,FileInformation,Length,FileInformationClass);
}

void static NoirHookNtSetInformationFile(IN PVOID HookedAddress)
{
	//Note that Length-Disassembler is required.
	ULONG PatchSize=GetPatchSize(NtSetInformationFile,HookLength);
	Old_NtSetInformationFile=ExAllocatePool(NonPagedPool,PatchSize+DetourLength);
	if(Old_NtSetInformationFile)
	{
		/*
		  I have checked the methods by tandasat, who wrote the DdiMon and SimpleSvmHook.
		  His way of rip redirecting somewhat lacks robustness.
		  He uses a byte CC to inject a debug-break and intercept it with a VM-Exit.
		  This would significantly increase performance consumption in high-rate functions.
		  My implementation aims to reduce such performance consumption, where VM-Exit is avoided.
		*/
#if defined(_WIN64)
		//This shellcode can breach the 4GB-limit in AMD64 architecture.
		//No register would be destroyed.
		/*
		  ShellCode Overview:
		  push rax			-- 50
		  mov rax, proxy	-- 48 B8 XX XX XX XX XX XX XX XX
		  xchg [rsp],rax	-- 48 87 04 24
		  ret				-- C3
		  16 bytes in total. This shellcode is provided By AyalaRs.
		  This could be perfect ShellCode in my opinion.
		  Note that all functions in Windows Kernel are 16-bytes aligned.
		  Thus, this shellcode would not break next function.
		*/
		BYTE HookCode[16]={0x50,0x48,0xB8,0,0,0,0,0,0,0,0,0x48,0x87,0x04,0x24,0xC3};
		BYTE DetourCode[14]={0xFF,0x25,0x0,0x0,0x0,0x0,0,0,0,0,0,0,0,0};
		*(PULONG64)((ULONG64)HookCode+3)=(ULONG64)fake_NtSetInformationFile;
		*(PULONG64)((ULONG64)DetourCode+6)=(ULONG64)NtSetInformationFile+PatchSize;
#else
		//In Win32, this is a common trick of eip redirecting.
		BYTE HookCode[5]={0xE9,0,0,0,0};
		BYTE DetourCode[5]={0xE9,0,0,0,0};
		*(PULONG)((ULONG)HookCode+1)=(ULONG)fake_NtSetInformationFile-(ULONG)NtSetInformationFile-5;
		*(PULONG)((ULONG)DetourCode+1)=(ULONG)NtSetInformationFile-(ULONG)Old_NtSetInformationFile-5;
#endif
		NoirDebugPrint("Patch Size of NtSetInformationFile: %d\t 0x%p\n",PatchSize,NtSetInformationFile);
		RtlCopyMemory(Old_NtSetInformationFile,NtSetInformationFile,PatchSize);
		RtlCopyMemory((PVOID)((ULONG_PTR)Old_NtSetInformationFile+PatchSize),DetourCode,DetourLength);
		RtlCopyMemory(HookedAddress,HookCode,HookLength);
	}
}

void NoirBuildHookedPages()
{
	HookPages=ExAllocatePool(NonPagedPool,sizeof(NOIR_HOOK_PAGE));
	if(HookPages)
	{
		UNICODE_STRING uniFuncName=RTL_CONSTANT_STRING(L"NtSetInformationFile");
		NtSetInformationFile=MmGetSystemRoutineAddress(&uniFuncName);
		HookPages->OriginalPage.VirtualAddress=NoirGetPageBase(NtSetInformationFile);
		HookPages->OriginalPage.PhysicalAddress=NoirGetPhysicalAddress(HookPages->OriginalPage.VirtualAddress);
		HookPages->HookedPage.VirtualAddress=NoirAllocateContiguousMemory(PAGE_SIZE);
		if(HookPages->HookedPage.VirtualAddress)
		{
			USHORT PageOffset=(USHORT)((ULONG_PTR)NtSetInformationFile&0xFFF);
			HookPages->HookedPage.PhysicalAddress=NoirGetPhysicalAddress(HookPages->HookedPage.VirtualAddress);
			RtlCopyMemory(HookPages->HookedPage.VirtualAddress,HookPages->OriginalPage.VirtualAddress,PAGE_SIZE);
			NoirHookNtSetInformationFile((PVOID)((ULONG_PTR)HookPages->HookedPage.VirtualAddress+PageOffset));
		}
		HookPages->NextHook=NULL;
	}
}

void NoirTeardownHookedPages()
{
	if(HookPages)
	{
		PNOIR_HOOK_PAGE CurHp=HookPages;
		while(CurHp)
		{
			PNOIR_HOOK_PAGE NextHp=CurHp->NextHook;
			if(CurHp->HookedPage.VirtualAddress)
				MmFreeContiguousMemory(CurHp->HookedPage.VirtualAddress);
			ExFreePool(CurHp);
			CurHp=NextHp;
		}
		HookPages=NULL;
	}
	if(Old_NtSetInformationFile)
	{
		ExFreePool(Old_NtSetInformationFile);
		Old_NtSetInformationFile=NULL;
	}
}

void NoirGetNtOpenProcessIndex()
{
	UNICODE_STRING uniFuncName=RTL_CONSTANT_STRING(L"ZwOpenProcess");
	PVOID p=MmGetSystemRoutineAddress(&uniFuncName);
	if(p)IndexOf_NtOpenProcess=*(PULONG)((ULONG_PTR)p+INDEX_OFFSET);
}

void NoirSetProtectedPID(IN ULONG NewPID)
{
	ProtPID=NewPID;
	ProtPID&=0xFFFFFFFC;
}