/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/winhvm.c
*/

#include <ntddk.h>
#include <windef.h>
#include <ntimage.h>
#include <ntstrsafe.h>
#include "winhvm.h"

NTSTATUS NoirReportWindowsVersion()
{
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	RTL_OSVERSIONINFOEXW OsVer;
	OsVer.dwOSVersionInfoSize=sizeof(OsVer);
	st=RtlGetVersion((PRTL_OSVERSIONINFOW)&OsVer);
	if(NT_SUCCESS(st))
	{
		HV_MSR_PROPRIETARY_GUEST_OS_ID HvMsrOsId;
		HvMsrOsId.BuildNumber=OsVer.dwBuildNumber;
		HvMsrOsId.ServiceVersion=OsVer.wServicePackMajor;
		HvMsrOsId.MajorVersion=OsVer.dwMajorVersion;
		HvMsrOsId.MinorVersion=OsVer.dwMinorVersion;
		HvMsrOsId.OsId=HV_WINDOWS_NT_OS_ID;
		HvMsrOsId.VendorId=HV_MICROSOFT_VENDOR_ID;
		HvMsrOsId.OsType=0;
		__writemsr(HV_X64_MSR_GUEST_OS_ID,HvMsrOsId.Value);
	}
	return st;
}

void NoirPrintCompilerVersion()
{
	ULONG Build=_MSC_FULL_VER%100000;
	ULONG Minor=_MSC_VER%100;
	ULONG Major=_MSC_VER/100;
	NoirDebugPrint("Compiler Version: MSVC %02d.%02d.%05d\n",Major,Minor,Build);
	NoirDebugPrint("NoirVisor Compliation Date: %s %s\n",__DATE__,__TIME__);
}

NTSTATUS NoirGetSystemVersion(OUT PWSTR VersionString,IN ULONG VersionLength)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PKEY_VALUE_PARTIAL_INFORMATION KvPartInf=ExAllocatePool(PagedPool,PAGE_SIZE);
	if(KvPartInf)
	{
		HANDLE hKey=NULL;
		UNICODE_STRING uniKeyName=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion");
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa,&uniKeyName,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
		st=ZwOpenKey(&hKey,GENERIC_READ,&oa);
		if(NT_SUCCESS(st))
		{
			UNICODE_STRING uniKvName;
			ULONG RetLen=0;
			WCHAR ProductName[128],BuildLabEx[128],BuildNumber[8];
			RtlZeroMemory(VersionString,VersionLength);
			// Get Windows Product Name
			RtlZeroMemory(KvPartInf,PAGE_SIZE);
			RtlZeroMemory(ProductName,sizeof(ProductName));
			RtlInitUnicodeString(&uniKvName,L"ProductName");
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInf,PAGE_SIZE,&RetLen);
			if(NT_SUCCESS(st))RtlStringCbCopyNW(ProductName,sizeof(ProductName),(PWSTR)KvPartInf->Data,KvPartInf->DataLength);
			// Get Windows CSD Version
			RtlZeroMemory(KvPartInf,PAGE_SIZE);
			RtlZeroMemory(BuildLabEx,sizeof(BuildLabEx));
			RtlInitUnicodeString(&uniKvName,L"BuildLabEx");
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInf,PAGE_SIZE,&RetLen);
			if(NT_SUCCESS(st))RtlStringCbCopyNW(BuildLabEx,sizeof(BuildLabEx),(PWSTR)KvPartInf->Data,KvPartInf->DataLength);
			// Get Windows Build Number
			RtlZeroMemory(KvPartInf,PAGE_SIZE);
			RtlZeroMemory(BuildNumber,sizeof(BuildNumber));
			RtlInitUnicodeString(&uniKvName,L"CurrentBuildNumber");
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInf,PAGE_SIZE,&RetLen);
			if(NT_SUCCESS(st))RtlStringCbCopyNW(BuildNumber,sizeof(BuildNumber),(PWSTR)KvPartInf->Data,KvPartInf->DataLength);
			// Build the String
			st=RtlStringCbPrintfW(VersionString,VersionLength,L"%ws Build %ws (%ws)",ProductName,BuildNumber,BuildLabEx);
			NoirDebugPrint("System Version: %ws\n",VersionString);
			ZwClose(hKey);
		}
		ExFreePool(KvPartInf);
	}
	return st;
}

ULONG NoirBuildHypervisor()
{
	if(NoirHypervisorStarted==FALSE)
	{
		ULONG r=nvc_build_hypervisor();
		if(r==0)NoirHypervisorStarted=TRUE;
		return r;
	}
	return 0;
}

void NoirTeardownHypervisor()
{
	if(NoirHypervisorStarted)
	{
		nvc_teardown_hypervisor();
		NoirHypervisorStarted=FALSE;
	}
}

ULONG NoirVisorVersion()
{
	return noir_visor_version();
}

void NoirGetVendorString(OUT PSTR VendorString)
{
	noir_get_vendor_string(VendorString);
}

void NoirGetProcessorName(OUT PSTR ProcessorName)
{
	noir_get_processor_name(ProcessorName);
}

ULONG NoirQueryVirtualizationSupportability()
{
	return noir_get_virtualization_supportability();
}

void NoirSaveImageInfo(IN PDRIVER_OBJECT DriverObject)
{
	if(DriverObject)
	{
		NvImageBase=DriverObject->DriverStart;
		NvImageSize=DriverObject->DriverSize;
	}
}

void nvc_store_image_info(OUT PVOID* Base,OUT PULONG Size)
{
	if(Base)*Base=NvImageBase;
	if(Size)*Size=NvImageSize;
}

BOOLEAN NoirInitializeCodeIntegrity(IN PVOID ImageBase)
{
	PIMAGE_DOS_HEADER DosHead=(PIMAGE_DOS_HEADER)ImageBase;
	if(DosHead->e_magic==IMAGE_DOS_SIGNATURE)
	{
		PIMAGE_NT_HEADERS NtHead=(PIMAGE_NT_HEADERS)((ULONG_PTR)ImageBase+DosHead->e_lfanew);
		if(NtHead->Signature==IMAGE_NT_SIGNATURE)
		{
			PIMAGE_SECTION_HEADER SectionHeaders=(PIMAGE_SECTION_HEADER)((ULONG_PTR)NtHead+sizeof(IMAGE_NT_HEADERS));
			USHORT NumberOfSections=NtHead->FileHeader.NumberOfSections;
			USHORT i=0;
			for(;i<NumberOfSections;i++)
			{
				if(_strnicmp(SectionHeaders[i].Name,".text",8)==0)
				{
					PVOID CodeBase=(PVOID)((ULONG_PTR)ImageBase+SectionHeaders[i].VirtualAddress);
					ULONG CodeSize=SectionHeaders[i].SizeOfRawData;
					NoirDebugPrint("Code Base: 0x%p\t Size: 0x%X\n",CodeBase,CodeSize);
					return noir_initialize_ci(CodeBase,CodeSize,TRUE,TRUE);
				}
			}
		}
	}
	return FALSE;
}

void NoirFinalizeCodeIntegrity()
{
	noir_finalize_ci();
}

void static NoirPowerStateCallback(IN PVOID CallbackContext,IN PVOID Argument1,IN PVOID Argument2)
{
	if(Argument1==(PVOID)PO_CB_SYSTEM_STATE_LOCK)
	{
		NoirDebugPrint("At Power State Callback, IRQL=%d\n",KeGetCurrentIrql());
		if(Argument2)
		{
			// System is resuming from sleeping or hibernating.
			// Restart the hypervisor.
			NoirBuildHookedPages();
			if(NoirHypervisorStarted)nvc_build_hypervisor();
		}
		else
		{
			// System is shutting down, sleeping or hibernating.
			// This will trigger VM-Exit of Processor Power.
			// Things will be complex before hypervisor is stopped.
			// So we stop hypervisor before system power state changes.
			if(NoirHypervisorStarted)nvc_teardown_hypervisor();
			NoirTeardownHookedPages();
		}
	}
}

void NoirFinalizePowerStateCallback()
{
	if(NoirPowerCallbackObject)
	{
		ExUnregisterCallback(NoirPowerCallbackObject);
		NoirPowerCallbackObject=NULL;
	}
}

NTSTATUS NoirInitializePowerStateCallback()
{
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	PCALLBACK_OBJECT pCallback=NULL;
	UNICODE_STRING uniObjectName=RTL_CONSTANT_STRING(L"\\Callback\\PowerState");
	OBJECT_ATTRIBUTES oa;
	InitializeObjectAttributes(&oa,&uniObjectName,OBJ_CASE_INSENSITIVE,NULL,NULL);
	st=ExCreateCallback(&pCallback,&oa,FALSE,FALSE);
	if(NT_SUCCESS(st))
	{
		NoirPowerCallbackObject=ExRegisterCallback(pCallback,NoirPowerStateCallback,NULL);
		if(NoirPowerCallbackObject==NULL)st=STATUS_UNSUCCESSFUL;
		ObDereferenceObject(pCallback);
	}
	return st;
}