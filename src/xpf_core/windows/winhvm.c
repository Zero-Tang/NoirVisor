/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

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
	NTSTATUS st=STATUS_NOT_SUPPORTED;
	int Info[4];
	__cpuid(Info,1);
	if(_bittest(&Info[2],31))
	{
		__cpuid(Info,CPUID_LEAF_HV_VENDOR_ID);
		if(Info[0]>=CPUID_LEAF_HV_VENDOR_NEUTRAL)
		{
			__cpuid(Info,CPUID_LEAF_HV_VENDOR_NEUTRAL);
			if(Info[0]=='1#vH')
			{
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
			}
		}
	}
	return st;
}

NTSTATUS NoirGetUefiHypervisionStatus()
{
	NTSTATUS st;
	PVOID HalBase=NoirLocateImageBaseByName(L"hal.dll");
	if(HalBase)
	{
		HalGetEnvironmentVariableEx=NoirLocateExportedProcedureByName(HalBase,"HalGetEnvironmentVariableEx");
		if(HalGetEnvironmentVariableEx)
		{
			HYPERVISOR_SYSTEM_IDENTITY HvSysId;
			SIZE_T Size=sizeof(HvSysId);
			st=HalGetEnvironmentVariableEx(L"Version",&EfiNoirVisorVendorGuid,&HvSysId,&Size,EFI_VARIABLE_RUNTIME_ACCESS);
			if(NT_SUCCESS(st))
			{
				HYPERVISOR_LAYERING_PASSCODE PassCode;
				NoirDebugPrint("NoirVisor is present in UEFI!\n");
				Size=sizeof(PassCode);
				st=HalGetEnvironmentVariableEx(L"Passcode",&EfiNoirVisorVendorGuid,&PassCode,&Size,EFI_VARIABLE_RUNTIME_ACCESS);
				if(NT_SUCCESS(st))
					NoirDebugPrint("NoirVisor UEFI Layering Hypervisor Passcode: %.*s\n",PassCode.Length,PassCode.Text);
				else
					NoirDebugPrint("Failed to obtain NoirVisor UEFI Layering Hypervisor Passcode! Status=0x%X\n",st);
				NoirDebugPrint("NoirVisor UEFI Version is %u.%u Service Pack %u Build %u.\n",HvSysId.MajorVersion,HvSysId.MinorVersion,HvSysId.ServicePack,HvSysId.BuildNumber);
				NoirDebugPrint("NoirVisor UEFI Service Branch is %u, Service Number is %u.\n",HvSysId.ServiceBranch,HvSysId.ServiceNumber);
			}
			else
			{
				NoirDebugPrint("Failed to confirm NoirVisor's presence in UEFI! Status=0x%X\n",st);
				// The following code explains the return value of HalGetEnvironmentVariableEx.
				switch(st)
				{
					case STATUS_NOT_IMPLEMENTED:
					{
						NoirDebugPrint("System is booted with BIOS instead of UEFI!\n");
						break;
					}
					case STATUS_VARIABLE_NOT_FOUND:
					{
						NoirDebugPrint("System is booted with UEFI. However, NoirVisor was not loaded during UEFI Boot Stage!\n");
						break;
					}
					default:
					{
						NoirDebugPrint("Unknown Error while querying NoirVisor UEFI Variables! NTSTATUS=0x%X\n",st);
						break;
					}
				}
			}
		}
		else
		{
			NoirDebugPrint("Failed to locate HalGetEnvironmentVariableEx! This system does not support UEFI!\n");
			st=STATUS_NOT_IMPLEMENTED;
		}
	}
	else
	{
		NoirDebugPrint("Failed to locate hal.dll module!\n");
		st=STATUS_UNSUCCESSFUL;
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

NTSTATUS NoirQueryEnabledFeaturesInSystem(OUT PULONG64 Features)
{
	// Setup default values. Hooking becomes a very unstable feature in Windows with post-2018 updates!
	ULONG32 CpuidPresence=1;		// Enable CPUID Presence at default.
	ULONG32 StealthMsrHook=0;		// Disable Stealth MSR Hook at default.
	ULONG32 StealthInlineHook=0;	// Disable Stealth Inline Hook at default.
	// Initialize.
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PKEY_VALUE_PARTIAL_INFORMATION KvPartInf=ExAllocatePool(PagedPool,PAGE_SIZE);
	if(KvPartInf)
	{
		HANDLE hKey=NULL;
		UNICODE_STRING uniKeyName=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Software\\Zero-Tang\\NoirVisor");
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa,&uniKeyName,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
		st=ZwOpenKey(&hKey,GENERIC_READ,&oa);
		if(NT_SUCCESS(st))
		{
			UNICODE_STRING uniKvName;
			ULONG RetLen=0;
			// Detect if CPUID-Presence is enabled.
			RtlInitUnicodeString(&uniKvName,L"CpuidPresence");
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInf,PAGE_SIZE,&RetLen);
			if(NT_SUCCESS(st))CpuidPresence=*(PULONG32)KvPartInf->Data;
			NoirDebugPrint("CPUID-Presence is %s!\n",CpuidPresence?"enabled":"disabled");
			// Detect if Stealth MSR Hook is enabled
			RtlInitUnicodeString(&uniKvName,L"StealthMsrHook");
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInf,PAGE_SIZE,&RetLen);
			if(NT_SUCCESS(st))StealthMsrHook=*(PULONG32)KvPartInf->Data;
			NoirDebugPrint("Stealth MSR Hook is %s!\n",StealthMsrHook?"enabled":"disabled");
			// Detect if Stealth Inline Hook is enabled
			RtlInitUnicodeString(&uniKvName,L"StealthInlineHook");
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInf,PAGE_SIZE,&RetLen);
			if(NT_SUCCESS(st))StealthInlineHook=*(PULONG32)KvPartInf->Data;
			NoirDebugPrint("Stealth Inline Hook is %s!\n",StealthInlineHook?"enabled":"disabled");
			ZwClose(hKey);
		}
		ExFreePool(KvPartInf);
	}
	// Summarize
	*Features|=CpuidPresence<<NOIR_HVM_FEATURE_CPUID_PRESENCE_BIT;
	*Features|=StealthMsrHook<<NOIR_HVM_FEATURE_STEALTH_MSR_HOOK_BIT;
	*Features|=StealthInlineHook<<NOIR_HVM_FEATURE_STEALTH_INLINE_HOOK_BIT;
	return st;
}

ULONG64 noir_query_enabled_features_in_system()
{
	ULONG64 Features=0;
	NoirQueryEnabledFeaturesInSystem(&Features);
	return Features;
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
		if(r==0)
		{
			NoirHypervisorStarted=TRUE;
			NoirDebugPrint("NoirVisor CVM Initialization Status: 0x%X\n",NoirInitializeCvmModule());
		}
		return r;
	}
	return 0;
}

void NoirTeardownHypervisor()
{
	if(NoirHypervisorStarted)
	{
		NoirFinalizeCvmModule();
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

BOOLEAN NoirIsVirtualizationEnabled()
{
	return noir_is_virtualization_enabled();
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