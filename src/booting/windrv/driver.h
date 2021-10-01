/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the kernel-mode driver framework of Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/windrv/driver.h
*/

#include <ntddk.h>
#include <windef.h>

#define DEVICE_NAME			L"\\Device\\NoirVisor"
#define LINK_NAME			L"\\DosDevices\\NoirVisor"

// Definitions of Status Codes of NoirVisor.
#define NOIR_SUCCESS					0
#define NOIR_UNSUCCESSFUL				0xC0000000
#define NOIR_INSUFFICIENT_RESOURCES		0xC0000001
#define NOIR_NOT_IMPLEMENTED			0xC0000002
#define NOIR_UNKNOWN_PROCESSOR			0xC0000003
#define NOIR_INVALID_PARAMETER			0xC0000004
#define NOIR_HYPERVISION_ABSENT			0xC0000005

typedef ULONG32 NOIR_STATUS;

// Definitions of I/O Control Codes of NoirVisor Driver.
#define CTL_CODE_GEN(i)		CTL_CODE(FILE_DEVICE_UNKNOWN,i,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_Subvert		CTL_CODE_GEN(0x801)
#define IOCTL_Restore		CTL_CODE_GEN(0x802)
#define IOCTL_SetPID		CTL_CODE_GEN(0x803)
#define IOCTL_SetNs			CTL_CODE_GEN(0x804)
#define IOCTL_SetVs			CTL_CODE_GEN(0x805)
#define IOCTL_SetName		CTL_CODE_GEN(0x806)
#define IOCTL_NvVer			CTL_CODE_GEN(0x810)
#define IOCTL_CpuVs			CTL_CODE_GEN(0x811)
#define IOCTL_CpuPn			CTL_CODE_GEN(0x812)
#define IOCTL_OsVer			CTL_CODE_GEN(0x813)
#define IOCTL_VirtCap		CTL_CODE_GEN(0x814)
#define IOCTL_VirtEn		CTL_CODE_GEN(0x815)

// Following definitions are intended for CVM use.
#define IOCTL_CvmCreateVm		CTL_CODE_GEN(0x880)
#define IOCTL_CvmDeleteVm		CTL_CODE_GEN(0x881)
#define IOCTL_CvmSetMapping		CTL_CODE_GEN(0x882)
#define IOCTL_CvmQueryHvStatus	CTL_CODE_GEN(0x88F)
#define IOCTL_CvmCreateVcpu		CTL_CODE_GEN(0x890)
#define IOCTL_CvmDeleteVcpu		CTL_CODE_GEN(0x891)
#define IOCTL_CvmRunVcpu		CTL_CODE_GEN(0x892)
#define IOCTL_CvmViewVcpuReg	CTL_CODE_GEN(0x893)
#define IOCTL_CvmEditVcpuReg	CTL_CODE_GEN(0x894)

// Layered Hypervisor Functions
typedef ULONG_PTR CVM_HANDLE;
typedef PULONG_PTR PCVM_HANDLE;

typedef struct _NOIR_ADDRESS_MAPPING
{
	ULONG64 GPA;
	ULONG64 HVA;
	ULONG32 NumberOfPages;
	union
	{
		struct
		{
			ULONG32 Present:1;
			ULONG32 Write:1;
			ULONG32 Execute:1;
			ULONG32 User:1;
			ULONG32 Caching:3;
			ULONG32 PageSize:2;
			ULONG32 Reserved:23;
		};
		ULONG32 Value;
	}Attributes;
}NOIR_ADDRESS_MAPPING,*PNOIR_ADDRESS_MAPPING;

NOIR_STATUS NoirCreateVirtualMachine(OUT PCVM_HANDLE VirtualMachine);
NOIR_STATUS NoirReleaseVirtualMachine(IN CVM_HANDLE VirtualMachine);
NOIR_STATUS NoirCreateVirtualProcessor(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex);
NOIR_STATUS NoirReleaseVirtualProcessor(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex);
NOIR_STATUS NoirSetMapping(IN CVM_HANDLE VirtualMachine,IN PNOIR_ADDRESS_MAPPING MappingInformation);

void NoirInitializeDisassembler();
NTSTATUS NoirReportWindowsVersion();
NTSTATUS NoirBuildProtectedFile();
void NoirTeardownProtectedFile();
void NoirSetProtectedFile(IN PWSTR FileName);
void NoirPrintCompilerVersion();
NTSTATUS NoirGetSystemVersion(OUT PWSTR VersionString,IN ULONG VersionLength);
NTSTATUS NoirGetUefiHypervisionStatus();
void NoirReportMemoryIntrospectionCounter();
ULONG NoirBuildHypervisor();
void NoirTeardownHypervisor();
ULONG NoirVisorVersion();
ULONG NoirQueryVirtualizationSupportability();
BOOLEAN NoirIsVirtualizationEnabled();
void NoirLocatePsLoadedModule(IN PDRIVER_OBJECT DriverObject);
BOOLEAN NoirInitializeCodeIntegrity(IN PVOID ImageBase);
void NoirFinalizeCodeIntegrity();
NTSTATUS NoirInitializePowerStateCallback();
void NoirFinalizePowerStateCallback();
void NoirGetVendorString(OUT PSTR VendorString);
void NoirGetProcessorName(OUT PSTR ProcessorName);
void NoirGetNtOpenProcessIndex();
void NoirSaveImageInfo(IN PDRIVER_OBJECT DriverObject);
void NoirSetProtectedPID(IN ULONG NewPID);
void NoirBuildHookedPages();
void NoirTeardownHookedPages();
extern ULONG_PTR system_cr3;
extern ULONG_PTR orig_system_call;
extern PSTR virtual_vstr;
extern PSTR virtual_nstr;