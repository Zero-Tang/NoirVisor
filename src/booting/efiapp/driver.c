/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the UEFI Runtime Driver Framework.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/driver.c
*/

#include "efimain.h"
#include "driver.h"
#include <intrin.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>

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
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI NoirDriverEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	NoirEfiInitialize(ImageHandle,SystemTable);
	Print(L"Welcome to NoirVisor Runtime Driver!\r\n");
	Print(L"NoirVisor's Compiler Version: LLVM Clang %a\r\n",__clang_version__);
	Print(L"NoirVisor's Date of Compilation: %s %s\n",__DATE__,__TIME__);
	gBS->CreateEvent(EVT_SIGNAL_EXIT_BOOT_SERVICES,TPL_NOTIFY,NoirNotifyExitBootServices,NULL,&NoirEfiExitBootServicesNotification);
	EFI_LOADED_IMAGE_PROTOCOL* ImageInfo;
	EFI_STATUS st=gBS->HandleProtocol(ImageHandle,&gEfiLoadedImageProtocolGuid,&ImageInfo);
	if(st==EFI_SUCCESS)ImageInfo->Unload=NoirDriverUnload;
	Print(L"NoirVisor Runtime Driver Initialization Status: 0x%X\r\n",st);
	return EFI_SUCCESS;
}