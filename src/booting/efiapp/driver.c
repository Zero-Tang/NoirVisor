/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the UEFI Runtime Driver Framework.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/driver.c
*/

#include <Uefi.h>
#include <intrin.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include "driver.h"

EFI_STATUS EFIAPI NoirDriverUnload(IN EFI_HANDLE ImageHandle)
{
	Print(L"NoirVisor is unloaded!\r\n");
	return EFI_SUCCESS;
}

void EFIAPI NoirNotifyExitBootServices(IN EFI_EVENT Event,IN VOID* Context)
{
	;
}

EFI_STATUS EFIAPI NoirEfiInitialize(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	UefiBootServicesTableLibConstructor(ImageHandle,SystemTable);
	UefiRuntimeServicesTableLibConstructor(ImageHandle,SystemTable);
	UefiLibConstructor(ImageHandle,SystemTable);
	DevicePathLibConstructor(ImageHandle,SystemTable);
	StdIn=SystemTable->ConIn;
	StdOut=SystemTable->ConOut;
	return gBS->LocateProtocol(&gEfiMpServicesProtocolGuid,NULL,(VOID**)&MpServices);
}

EFI_STATUS EFIAPI NoirDriverEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS st=NoirEfiInitialize(ImageHandle,SystemTable);
	EFI_LOADED_IMAGE_PROTOCOL* ImageInfo=NULL;
	Print(L"Welcome to NoirVisor Runtime Driver!\r\n");
	Print(L"NoirVisor's Compiler Version: LLVM Clang %a\r\n",__clang_version__);
	Print(L"NoirVisor's Date of Compilation: %a\n",__TIMESTAMP__);
	gBS->CreateEvent(EVT_SIGNAL_EXIT_BOOT_SERVICES,TPL_NOTIFY,NoirNotifyExitBootServices,NULL,&NoirEfiExitBootServicesNotification);
	st=gBS->HandleProtocol(ImageHandle,&gEfiLoadedImageProtocolGuid,&ImageInfo);
	if(st==EFI_SUCCESS)ImageInfo->Unload=NoirDriverUnload;
	st=NoirBuildHostEnvironment();
	Print(L"NoirVisor Runtime Driver Initialization Status: 0x%X MpServices: 0x%p\r\n",st,MpServices);
	return EFI_SUCCESS;
}