/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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
	NoirEfiInRuntimeStage=TRUE;
}

void NoirBlockUntilKeyStroke(IN CHAR16 Unicode)
{
	EFI_INPUT_KEY InKey;
	do
	{
		UINTN fi=0;
		gBS->WaitForEvent(1,&StdIn->WaitForKey,&fi);
		StdIn->ReadKeyStroke(StdIn,&InKey);
	}while(InKey.UnicodeChar!=Unicode);
}

EFI_STATUS EFIAPI NoirRegisterHypervisorVariables()
{
	EFI_STATUS st=gRT->SetVariable(L"Version",&gEfiNoirVisorVendorGuid,EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,sizeof(HvSysId),&HvSysId);
	if(st==EFI_SUCCESS)
	{
		if(sizeof(LayeringPasscode)>255)
		{
			Print(L"Passcode for Layered Hypervisor is too long!\n");
			st=EFI_BAD_BUFFER_SIZE;
		}
		else
		{
			Passcode.Length=sizeof(LayeringPasscode);
			CopyMem(Passcode.Text,LayeringPasscode,sizeof(LayeringPasscode));
			st=gRT->SetVariable(L"Passcode",&gEfiNoirVisorVendorGuid,EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,sizeof(Passcode),&Passcode);
		}
	}
	return st;
}

EFI_STATUS EFIAPI NoirEfiInitialize(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	// Constructors...
	UefiBootServicesTableLibConstructor(ImageHandle,SystemTable);
	UefiRuntimeServicesTableLibConstructor(ImageHandle,SystemTable);
	UefiLibConstructor(ImageHandle,SystemTable);
	DevicePathLibConstructor(ImageHandle,SystemTable);
	// Standard I/O
	StdIn=SystemTable->ConIn;
	StdOut=SystemTable->ConOut;
	// Hypervisor System Identity
	HvSysId.BuildNumber=0;
	HvSysId.MajorVersion=0;
	HvSysId.MinorVersion=6;
	HvSysId.ServicePack=0;
	HvSysId.ServiceNumber=0;
	HvSysId.ServiceBranch=0;
	// Locate Protocols Required by NoirVisor.
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
	Print(L"NoirVisor is loaded to base 0x%p, Size=0x%X\n",ImageInfo->ImageBase,ImageInfo->ImageSize);
	st=NoirRegisterHypervisorVariables();
	Print(L"NoirVisor Variables Registration Status=0x%X\n",st);
	st=NoirBuildHostEnvironment();
	Print(L"NoirVisor Runtime Driver Initialization Status: 0x%X\r\n",st);
	NoirInitializeSerialPort(1,0);
	StdOut->OutputString(StdOut,L"Press Enter key to continue subversion!\r\n");
	NoirBlockUntilKeyStroke(L'\r');
	NoirBuildHypervisor();
	return EFI_SUCCESS;
}