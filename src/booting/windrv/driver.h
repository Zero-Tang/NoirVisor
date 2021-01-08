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

void NoirInitializeDisassembler();
NTSTATUS NoirReportWindowsVersion();
NTSTATUS NoirBuildProtectedFile();
void NoirTeardownProtectedFile();
void NoirSetProtectedFile(IN PWSTR FileName);
void NoirPrintCompilerVersion();
NTSTATUS NoirGetSystemVersion(OUT PWSTR VersionString,IN ULONG VersionLength);
void NoirReportMemoryIntrospectionCounter();
ULONG NoirBuildHypervisor();
void NoirTeardownHypervisor();
ULONG NoirVisorVersion();
ULONG NoirQueryVirtualizationSupportability();
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