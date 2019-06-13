/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/winhvm.c
*/

#include <ntddk.h>
#include <windef.h>
#include <ntstrsafe.h>
#include "winhvm.h"

NTSTATUS static NoirQueryRegistryRoutine(IN PWSTR ValueName,IN ULONG ValueType,IN PVOID ValueData,IN ULONG ValueLength,IN PVOID Context,IN PVOID EntryContext)
{
	if(ValueType==REG_SZ)
	{
		PWSTR* Names=(PWSTR*)Context;
		if(wcscmp(ValueName,L"ProductName")==0)
			RtlCopyMemory(Names[0],ValueData,ValueLength);
		else if(wcscmp(ValueName,L"CSDVersion")==0)
			RtlCopyMemory(Names[1],ValueData,ValueLength);
		else if(wcscmp(ValueName,L"CurrentBuild")==0)
			RtlCopyMemory(Names[2],ValueData,ValueLength);
	}
	return STATUS_SUCCESS;
}

NTSTATUS NoirGetSystemVersion(OUT PWSTR VersionString,IN ULONG VersionLength)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PWSTR ProductName=ExAllocatePool(NonPagedPool,128);
	PWSTR CsdVersion=ExAllocatePool(NonPagedPool,128);
	if(ProductName && CsdVersion)
	{
		PWSTR Cxt[3];
		WCHAR BuildNumber[16];
		RTL_QUERY_REGISTRY_TABLE RegQueryTable;
		RtlZeroMemory(&RegQueryTable,sizeof(RegQueryTable));
		RtlZeroMemory(ProductName,128);
		RtlZeroMemory(CsdVersion,128);
		RtlZeroMemory(BuildNumber,32);
		RegQueryTable.QueryRoutine=NoirQueryRegistryRoutine;
		RegQueryTable.Flags=RTL_QUERY_REGISTRY_REQUIRED;
		RegQueryTable.DefaultType=REG_SZ;
		Cxt[0]=ProductName;
		Cxt[1]=CsdVersion;
		Cxt[2]=BuildNumber;
		st=RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT,NULL,&RegQueryTable,Cxt,NULL);
		st=RtlStringCbPrintfW(VersionString,VersionLength,L"%ws %ws Build %ws",ProductName,CsdVersion,BuildNumber);
	}
	if(ProductName)ExFreePool(ProductName);
	if(CsdVersion)ExFreePool(CsdVersion);
	return st;
}

ULONG NoirBuildHypervisor()
{
	return nvc_build_hypervisor();
}

void NoirTeardownHypervisor()
{
	nvc_teardown_hypervisor();
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