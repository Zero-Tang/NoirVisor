/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file is the kernel-mode driver framework of Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/windrv/driver.h
*/

#include <ntddk.h>
#include <windef.h>

// NoirVisor Device Name
#define DEVICE_NAME			L"\\Device\\NoirVisor"
#define LINK_NAME			L"\\DosDevices\\NoirVisor"

// Definitions of I/O Control Codes of NoirVisor Driver.
#define CTL_CODE_GEN(i)		CTL_CODE(FILE_DEVICE_UNKNOWN,i,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_Subvert		CTL_CODE_GEN(0x801)
#define IOCTL_Restore		CTL_CODE_GEN(0x802)
#define IOCTL_SetPID		CTL_CODE_GEN(0x803)
#define IOCTL_SetNs			CTL_CODE_GEN(0x804)
#define IOCTL_SetVs			CTL_CODE_GEN(0x805)
#define IOCTL_SetName		CTL_CODE_GEN(0x806)
#define IOCTL_CpuVs			CTL_CODE_GEN(0x811)
#define IOCTL_CpuPn			CTL_CODE_GEN(0x812)
#define IOCTL_OsVer			CTL_CODE_GEN(0x813)
#define IOCTL_VirtCap		CTL_CODE_GEN(0x814)
#define IOCTL_VirtEn		CTL_CODE_GEN(0x815)

// Following definitions are intended for CVM use.
#define IOCTL_CvmCreateVm		CTL_CODE_GEN(0x880)
#define IOCTL_CvmDeleteVm		CTL_CODE_GEN(0x881)
#define IOCTL_CvmSetMapping		CTL_CODE_GEN(0x882)
#define IOCTL_CvmQueryGpaAdMap	CTL_CODE_GEN(0x883)
#define IOCTL_CvmClearGpaAdBit	CTL_CODE_GEN(0x884)
#define IOCTL_CvmQueryHvStatus	CTL_CODE_GEN(0x88F)
#define IOCTL_CvmCreateVcpu		CTL_CODE_GEN(0x890)
#define IOCTL_CvmDeleteVcpu		CTL_CODE_GEN(0x891)
#define IOCTL_CvmRunVcpu		CTL_CODE_GEN(0x892)
#define IOCTL_CvmViewVcpuReg	CTL_CODE_GEN(0x893)
#define IOCTL_CvmEditVcpuReg	CTL_CODE_GEN(0x894)
#define IOCTL_CvmRescindVcpu	CTL_CODE_GEN(0x895)
#define IOCTL_CvmInjectEvent	CTL_CODE_GEN(0x896)
#define IOCTL_CvmSetVcpuOptions	CTL_CODE_GEN(0x897)
#define IOCTL_CvmQueryVcpuStats	CTL_CODE_GEN(0x898)

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
NTSTATUS NoirConfigureInternalDebugger();
BOOL NoirAcpiInitialize();
void NoirAcpiFinalize();
BOOL NoirInitializeLogger();
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
NTSTATUS NoirSubvertSystemOnDriverLoad(OUT PBOOLEAN Subvert);
void NoirFreeAllAllocatedPages();
void __cdecl NoirDebugPrint(const char* Format,...);

void __isa_available_init();
extern ULONG32 __isa_available;

extern ULONG32 noir_cvm_exit_context_size;

char virtual_vstr[12];
char virtual_nstr[48];

ULONG_PTR orig_system_call=0;

PEPROCESS SubversionProcess=NULL;
BOOLEAN SubvertOnDriverLoad=FALSE;

PDRIVER_OBJECT NoirDriverObject=NULL;
PDEVICE_OBJECT NoirDeviceObject=NULL;

// Required for being compatible with Rust.
int _fltused=0;