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

EFI_STATUS EFIAPI NoirDriverUnload(IN EFI_HANDLE ImageHandle)
{
	NoirConsolePrintfW(L"NoirVisor is unloaded!\r\n");
	return EFI_SUCCESS;
}

void EFIAPI NoirNotifyExitBootServices(IN EFI_EVENT Event,IN VOID* Context)
{
	;
}

EFI_STATUS EFIAPI NoirDriverEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	NoirEfiInitialize(SystemTable);
	NoirConsolePrintfW(L"Welcome to NoirVisor Runtime Driver!\r\n");
	EfiBoot->CreateEvent(EVT_SIGNAL_EXIT_BOOT_SERVICES,TPL_NOTIFY,NoirNotifyExitBootServices,NULL,&NoirEfiExitBootServicesNotification);
	EFI_GUID LoadedImageGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_LOADED_IMAGE_PROTOCOL* ImageInfo;
	EFI_STATUS st=EfiBoot->HandleProtocol(ImageHandle,&LoadedImageGuid,&ImageInfo);
	if(st==EFI_SUCCESS)ImageInfo->Unload=NoirDriverUnload;
	NoirConsolePrintfW(L"NoirVisor Runtime Driver Initialization Status: 0x%X\r\n",st);
	return EFI_SUCCESS;
}