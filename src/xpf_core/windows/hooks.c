/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is auxiliary to Hooking facility (MSR Hook, Inline Hook).

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/hooks.c
*/

#include <ntddk.h>
#include <windef.h>
#include <stdlib.h>
#include <ntimage.h>
#include <ntstrsafe.h>
#include "hooks.h"

// Initialize protected file information with shared lock primitive.
NTSTATUS NoirBuildProtectedFile()
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	NoirProtectedFile=NoirAllocateNonPagedMemory(PAGE_SIZE);
	if(NoirProtectedFile)
	{
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

// Finalize protected file information.
void NoirTeardownProtectedFile()
{
	if(NoirProtectedFile)
	{
		ExDeleteResourceLite(&NoirProtectedFile->Lock);
		NoirFreeNonPagedMemory(NoirProtectedFile);
		NoirProtectedFile=NULL;
	}
}

// Use exclusive locking primitive to revise protected file information.
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

// Use shared locking primitive to read protected file information.
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
				Result=(_wcsnicmp(NoirProtectedFile->FileName,FileName,NoirProtectedFile->MaximumLength>>1)==0);
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
		PFILE_NAME_INFORMATION FileNameInfo=NoirAllocatePagedMemory(PAGE_SIZE);
		if(FileNameInfo)
		{
			st=ZwQueryInformationFile(FileHandle,IoStatusBlock,FileNameInfo,PAGE_SIZE,FileNameInformation);
			if(NT_SUCCESS(st))
			{
				if(NoirIsProtectedFile(FileNameInfo->FileName))
					st=STATUS_DEVICE_PAPER_EMPTY;
				else
					st=Old_NtSetInformationFile(FileHandle,IoStatusBlock,FileInformation,Length,FileInformationClass);
			}
			NoirFreePagedMemory(FileNameInfo);
		}
		return st;
	}
	return Old_NtSetInformationFile(FileHandle,IoStatusBlock,FileInformation,Length,FileInformationClass);
}

NTSTATUS NoirConstructHookPage(IN PVOID Address,IN PBYTE HookCode,IN ULONG Length,OUT PULONG RemainingLength OPTIONAL,OUT PULONG CopiedLength OPTIONAL)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PVOID HookCodeEnd=(PVOID)((ULONG_PTR)Address+Length);
	ULONG RemainderLength=ADDRESS_AND_SIZE_TO_SPAN_PAGES(Address,Length)>1?BYTE_OFFSET(HookCodeEnd):0;
	PNOIR_HOOK_PAGE HookPage=NULL;
	ULONG64 FPA=NoirGetPhysicalAddress(Address);
	ULONG PatchSize=GetPatchSize(Address,HookLength);
	// Search if there is a hooked page containing the function.
	for(ULONG i=0;i<HookPageCount;i++)
	{
		if(FPA>=HookPages[i].OriginalPage.PhysicalAddress && FPA<HookPages[i].OriginalPage.PhysicalAddress+PAGE_SIZE)
		{
			HookPage=&HookPages[i];
			break;
		}
	}
	// If there is no such page, then allocate a new page for it.
	if(HookPage==NULL)
	{
		if(HookPageCount>=HookPageLimit)
		{
			NoirDebugPrint("Hooked Page Limit exceeded! Increment the limit to proceed!\n");
			return st;
		}
		HookPage=&HookPages[HookPageCount++];
		HookPage->HookedPage.VirtualAddress=NoirAllocateContiguousMemory(PAGE_SIZE);
		if(HookPage->HookedPage.VirtualAddress==NULL)return st;
		HookPage->HookedPage.PhysicalAddress=NoirGetPhysicalAddress(HookPage->HookedPage.VirtualAddress);
		HookPage->OriginalPage.VirtualAddress=PAGE_ALIGN(Address);
		HookPage->OriginalPage.PhysicalAddress=NoirGetPhysicalAddress(HookPage->OriginalPage.VirtualAddress);
		RtlCopyMemory(HookPage->HookedPage.VirtualAddress,HookPage->OriginalPage.VirtualAddress,PAGE_SIZE);
		// The hooked page is not guaranteed to always be resident, so lock it.
		HookPage->Mdl=IoAllocateMdl(HookPage->OriginalPage.VirtualAddress,PAGE_SIZE,FALSE,FALSE,NULL);
		if(HookPage->Mdl)
		{
			__try
			{
				MmProbeAndLockPages(HookPage->Mdl,KernelMode,IoWriteAccess);
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				NoirDebugPrint("Warning: failed to lock the page [0x%p] to be hooked! Stealthy Inline Hook may be unstable on this page!\n",HookPage->OriginalPage.VirtualAddress);
				IoFreeMdl(HookPage->Mdl);
				HookPage->Mdl=NULL;			// Purposefully set MDL to null to avoid re-unlock and re-free.
				st=GetExceptionCode();
			}
		}
	}
	if(HookPage->HookedPage.VirtualAddress)
	{
		ULONG PageOffset=BYTE_OFFSET(Address);
		PVOID HookedAddress=(PVOID)((ULONG_PTR)HookPage->HookedPage.VirtualAddress+PageOffset);
		ULONG CopySize=Length-RemainderLength;
		// Copy the hook code to the hooked page.
		RtlCopyMemory(HookedAddress,HookCode,CopySize);
		st=STATUS_SUCCESS;
		if(ARGUMENT_PRESENT(CopiedLength))*CopiedLength=CopySize;
	}
	if(ARGUMENT_PRESENT(RemainingLength))*RemainingLength=RemainderLength;
	return st;
}

NTSTATUS NoirConstructHook(IN PVOID Address,IN PVOID Proxy,OUT PVOID* Detour)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	ULONG AffectedPages=ADDRESS_AND_SIZE_TO_SPAN_PAGES(Address,HookLength);
	ULONG PatchSize=GetPatchSize(Address,HookLength);
	PVOID TargetAddress=Address;
	ULONG HookCodeIndex=0;
	*Detour=NoirAllocateNonPagedMemory(PatchSize+DetourLength);
	if(*Detour==NULL)return st;
	/*
		I have checked the methods by tandasat, who wrote the DdiMon and SimpleSvmHook.
		His way of rip redirecting somewhat lacks robustness.
		He uses a byte CC to inject a debug-break and intercept it with a VM-Exit.
		This would significantly increase performance consumption in high-rate functions.
		My implementation aims to reduce such performance consumption, where VM-Exit is avoided.
	*/
#if defined(_WIN64)
	// This shellcode can breach the 4GB-limit in AMD64 architecture.
	// No register would be destroyed.
	/*
		ShellCode Overview:
		push rax						50
		mov rax,proxy					48 B8 XX XX XX XX XX XX XX XX
		xchg rax,qword ptr [rsp]		48 87 04 24
		add rsp,8						48 83 C4 08
		jmp qword ptr [rsp-8]			FF 64 24 F8
		23 bytes in total. This shellcode is provided By AyalaRs, improved by Zero Tang.
		Original implementation is not compatible with Control-Flow Enforcement.
	*/
	BYTE HookCode[23]={0x50,0x48,0xB8,0,0,0,0,0,0,0,0,0x48,0x87,0x04,0x24,0xC3};
	BYTE DetourCode[14]={0xFF,0x25,0x0,0x0,0x0,0x0,0,0,0,0,0,0,0,0};
	*(PULONG64)((ULONG64)HookCode+3)=(ULONG64)Proxy;
	*(PULONG64)((ULONG64)DetourCode+6)=(ULONG64)Address+PatchSize;
#else
	// In Win32, this is a common trick of eip redirecting.
	BYTE HookCode[5]={0xE9,0,0,0,0};
	BYTE DetourCode[5]={0xE9,0,0,0,0};
	*(PULONG)((ULONG)HookCode+1)=(ULONG)Proxy-(ULONG)Address-5;
	*(PULONG)((ULONG)DetourCode+1)=(ULONG)Address-(ULONG)Detour-5;
#endif
	NoirDebugPrint("Patch Size: %d\t 0x%p\n",PatchSize,Address);
	RtlCopyMemory(*Detour,Address,PatchSize);
	RtlCopyMemory((PVOID)((ULONG_PTR)*Detour+PatchSize),DetourCode,DetourLength);
	st=STATUS_SUCCESS;
	// Copy the hook code to shadow pages.
	for(ULONG i=0;i<AffectedPages;i++)
	{
		ULONG CopiedLength;
		st=NoirConstructHookPage(TargetAddress,&HookCode[HookCodeIndex],HookLength-HookCodeIndex,NULL,&CopiedLength);
		if(NT_ERROR(st))break;
		// Increment the addressing.
		HookCodeIndex+=CopiedLength;
		TargetAddress=(PVOID)((ULONG_PTR)TargetAddress+CopiedLength);
	}
	return st;
}

int static __cdecl NoirHookPageComparator(const void* a,const void*b)
{
	PNOIR_HOOK_PAGE ahp=(PNOIR_HOOK_PAGE)a;
	PNOIR_HOOK_PAGE bhp=(PNOIR_HOOK_PAGE)b;
	if(ahp->OriginalPage.PhysicalAddress>bhp->OriginalPage.PhysicalAddress)
		return 1;
	else if(ahp->OriginalPage.PhysicalAddress<bhp->OriginalPage.PhysicalAddress)
		return -1;
	return 0;
}

NTSTATUS NoirBuildHookedPages()
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	HookPages=NoirAllocateNonPagedMemory(sizeof(NOIR_HOOK_PAGE)*HookPageLimit);
	if(HookPages)
	{
		PVOID NtKernelBase=NoirLocateImageBaseByName(L"ntoskrnl.exe");
		if(NtKernelBase)
		{
			NtSetInformationFile=NoirLocateExportedProcedureByName(NtKernelBase,"NtSetInformationFile");
			st=NoirConstructHook(NtSetInformationFile,&fake_NtSetInformationFile,(PVOID*)&Old_NtSetInformationFile);
			if(NT_ERROR(st))return st;
			// Add your code here if you want to hook functions from
			// ntoskrnl.exe. You may simply follow the style above.

		}
		// Add your code here if you want to hook functions from
		// modules other than ntoskrnl (e.g win32k, ntfs, etc.)

		// Now, construction is completed. Sort them to reduce running time complexity.
		qsort(HookPages,HookPageCount,sizeof(NOIR_HOOK_PAGE),NoirHookPageComparator);
	}
	return st;
}

void NoirTeardownHookedPages()
{
	// Free all hook pages.
	if(HookPages)
	{
		for(ULONG i=0;i<HookPageCount;i++)
		{
			if(HookPages[i].HookedPage.VirtualAddress)
				NoirFreeContiguousMemory(HookPages[i].HookedPage.VirtualAddress);
			if(HookPages[i].Mdl)
			{
				MmUnlockPages(HookPages[i].Mdl);
				IoFreeMdl(HookPages[i].Mdl);
			}
		}
		NoirFreeNonPagedMemory(HookPages);
		HookPages=NULL;
	}
	// Free all detour functions allocated previously.
	if(Old_NtSetInformationFile)
	{
		NoirFreeNonPagedMemory(Old_NtSetInformationFile);
		Old_NtSetInformationFile=NULL;
	}
	// Add your code here to release detour functions for additional hooks.
}

void NoirGetNtOpenProcessIndex()
{
	UNICODE_STRING uniFuncName=RTL_CONSTANT_STRING(L"ZwOpenProcess");
	PVOID p=MmGetSystemRoutineAddress(&uniFuncName);
	if(p)IndexOf_NtOpenProcess=*(PULONG)((ULONG_PTR)p+INDEX_OFFSET);
}

void NoirSetProtectedPID(IN ULONG NewPID)
{
	// Use atomic operation for thread-safe consideration.
	InterlockedExchange(&ProtPID,NewPID&0xFFFFFFFC);
}

// The detection algorithm is very simple: is the syscall handler located in KVASCODE section?
BOOLEAN NoirDetectKvaShadow()
{
	PIMAGE_DOS_HEADER DosHead=NoirLocateImageBaseByName(L"ntoskrnl.exe");
	if(DosHead)
	{
		PIMAGE_NT_HEADERS NtHead=(PIMAGE_NT_HEADERS)((ULONG_PTR)DosHead+DosHead->e_lfanew);
		if(DosHead->e_magic==IMAGE_DOS_SIGNATURE && NtHead->Signature==IMAGE_NT_SIGNATURE)
		{
			PIMAGE_SECTION_HEADER SectionTable=(PIMAGE_SECTION_HEADER)((ULONG_PTR)NtHead+sizeof(IMAGE_NT_HEADERS));
			ULONG_PTR KvaShadowCodeStart=0;
			ULONG_PTR KvaShadowCodeSize=0;
			for(USHORT i=0;i<NtHead->FileHeader.NumberOfSections;i++)
			{
				ULONG_PTR SectionBase=(ULONG_PTR)DosHead+SectionTable[i].VirtualAddress;
				if(_strnicmp(SectionTable[i].Name,"KVASCODE",IMAGE_SIZEOF_SHORT_NAME)==0)
				{
					KvaShadowCodeStart=SectionBase;
					KvaShadowCodeSize=SectionTable[i].SizeOfRawData;
				}
			}
			if(KvaShadowCodeStart)
			{
#if defined(_WIN64)
				ULONG64 SysCallHandler=__readmsr(MSR_LSTAR);
#else
				ULONG32 SysCallHandler=(ULONG32)__readmsr(MSR_SYSENTER_EIP);
#endif
				NoirDebugPrint("KVA Shadow Code Base=0x%p\t Size=0x%X\n",KvaShadowCodeStart,KvaShadowCodeSize);
				if(SysCallHandler>=KvaShadowCodeStart && SysCallHandler<KvaShadowCodeStart+KvaShadowCodeSize)
				{
					NoirDebugPrint("The system call handler is located in KVA Shadow Code Section!\n");
					return TRUE;
				}
				NoirDebugPrint("The system call handler is not located in KVA Shadow Code Section!\n");
				return FALSE;
			}
			else
			{
				NoirDebugPrint("KVA Shadow Code Section is not present! Your system is not Meltdown-mitigated!\n");
				return FALSE;
			}
		}
	}
	NoirDebugPrint("Failed to locate NT Kernel-Mode Image!\n");
	return FALSE;
}