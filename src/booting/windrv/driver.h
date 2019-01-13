/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

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

ULONG NoirBuildHypervisor();
void NoirTeardownHypervisor();
void NoirGetNtOpenProcessIndex();
void NoirSaveImageInfo(IN PDRIVER_OBJECT DriverObject);
void NoirSetProtectedPID(IN ULONG NewPID);
void NoirBuildHookedPages();
extern ULONG_PTR system_cr3;
extern ULONG_PTR orig_system_call;