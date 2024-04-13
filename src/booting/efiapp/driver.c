/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the UEFI Runtime Driver Framework.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/driver.c
*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include "driver.h"

EFI_STATUS EFIAPI NoirDriverUnload(IN EFI_HANDLE ImageHandle)
{
	NoirFinalizeCodeIntegrity();
	NoirFinalizeConfigurationManager();
	NoirDebugPrint("NoirVisor is unloaded!\r\n");
	return EFI_SUCCESS;
}

void EFIAPI NoirNotifyExitBootServices(IN EFI_EVENT Event,IN VOID* Context)
{
	NoirEfiInRuntimeStage=TRUE;
	NoirDebugPrint("UEFI now enters Runtime Stage!\n");
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
			NoirDebugPrint("Passcode for Layered Hypervisor is too long!\n");
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

void NoirPrintCompilerVersion()
{
	UINT32 Build=_MSC_FULL_VER%100000;
	UINT32 Minor=_MSC_VER%100;
	UINT32 Major=_MSC_VER/100;
	NoirDebugPrint("Compiler Version: MSVC %02d.%02d.%05d\n",Major,Minor,Build);
	NoirDebugPrint("NoirVisor Compliation Date: %a %a\n",__DATE__,__TIME__);
}

EFI_STATUS EFIAPI NoirDriverEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS st=NoirEfiInitialize(ImageHandle,SystemTable);
	EFI_LOADED_IMAGE_PROTOCOL* ImageInfo=NULL;
	UINT32 Supportability=NoirQueryVirtualizationSupportability();
	NoirDebugPrint("Welcome to NoirVisor Runtime Driver!\r\n");
	NoirPrintCompilerVersion();
	if((Supportability&3)!=3)
	{
		StdOut->OutputString(StdOut,L"Required features of Hardware Accelerated Virtualization is unsupported!\r\n");
		return EFI_UNSUPPORTED;
	}
	if(NoirIsVirtualizationEnabled()==FALSE)
	{
		StdOut->OutputString(StdOut,L"Hardware Accelerated Virtualization is disabled! Check your firmware settings!\r\n");
		return EFI_UNSUPPORTED;
	}
	gBS->CreateEvent(EVT_SIGNAL_EXIT_BOOT_SERVICES,TPL_NOTIFY,NoirNotifyExitBootServices,NULL,&NoirEfiExitBootServicesNotification);
	st=gBS->HandleProtocol(ImageHandle,&gEfiLoadedImageProtocolGuid,&ImageInfo);
	if(st==EFI_SUCCESS)
	{
		ImageInfo->Unload=NoirDriverUnload;
		RootDeviceHandle=ImageInfo->DeviceHandle;
	}
	NoirDebugPrint("NoirVisor is loaded to base 0x%p, Size=0x%X\n",ImageInfo->ImageBase,ImageInfo->ImageSize);
	NoirInitializeDisassembler();
	NoirInitializeConfigurationManager();
	st=NoirRegisterHypervisorVariables();
	NoirDebugPrint("NoirVisor Variables Registration Status=0x%X\n",st);
	StdOut->OutputString(StdOut,L"Press Enter key to continue subversion!\r\n");
	NoirBlockUntilKeyStroke(L'\r');
	system_cr3=AsmReadCr3();
	NoirInitializeCodeIntegrity(ImageInfo->ImageBase);
	NoirSuppressImageRelocation(ImageInfo->ImageBase);
	NoirConfigureInternalDebugger();
	NoirAcpiInitialize();
	NoirHpetInitialize();
	NoirBuildHypervisor();
	return EFI_SUCCESS;
}