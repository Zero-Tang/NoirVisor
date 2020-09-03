/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the EFI Application Framework of System.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/efimain.c
*/

#include "efimain.h"
#include <intrin.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>

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

void NoirSetConsoleModeToMaximumRows()
{
	UINTN MaxHgt=0,OptIndex;
	for(UINTN i=0;i<StdOut->Mode->MaxMode;i++)
	{
		UINTN Col,Row;
		EFI_STATUS st=StdOut->QueryMode(StdOut,i,&Col,&Row);
		if(st==EFI_SUCCESS)
		{
			if(Row>MaxHgt)
			{
				OptIndex=i;
				MaxHgt=Row;
			}
		}
	}
	StdOut->SetMode(StdOut,OptIndex);
	StdOut->ClearScreen(StdOut);
}

void NoirPrintProcessorInformation()
{
	int Info[4];
	CHAR8 VendorString[13];
	__cpuid(Info,0);
	*(int*)&VendorString[0]=Info[1];
	*(int*)&VendorString[4]=Info[3];
	*(int*)&VendorString[8]=Info[2];
	VendorString[12]='\0';
	Print(L"Processor Vendor: %a\r\n",VendorString);
	CHAR8 BrandString[0x31];
	__cpuid(Info,0x80000002);
	*(int*)&BrandString[0x00]=Info[0];
	*(int*)&BrandString[0x04]=Info[1];
	*(int*)&BrandString[0x08]=Info[2];
	*(int*)&BrandString[0x0C]=Info[3];
	__cpuid(Info,0x80000003);
	*(int*)&BrandString[0x10]=Info[0];
	*(int*)&BrandString[0x14]=Info[1];
	*(int*)&BrandString[0x18]=Info[2];
	*(int*)&BrandString[0x1C]=Info[3];
	__cpuid(Info,0x80000004);
	*(int*)&BrandString[0x20]=Info[0];
	*(int*)&BrandString[0x24]=Info[1];
	*(int*)&BrandString[0x28]=Info[2];
	*(int*)&BrandString[0x2C]=Info[3];
	BrandString[0x30]='\0';
	Print(L"Processor Brand Name: %a\r\n",BrandString);
}

EFI_STATUS NoirLoadHypervisorDriver(IN EFI_HANDLE ParentImageHandle,OUT EFI_HANDLE *HvImageHandle)
{
	// Initialize Device Path for Loading Image
	EFI_LOADED_IMAGE_PROTOCOL* ImageInfo;
	EFI_STATUS st=gBS->HandleProtocol(ParentImageHandle,&gEfiLoadedImageProtocolGuid,&ImageInfo);
	if(st==EFI_SUCCESS)
	{
		FILEPATH_DEVICE_PATH* FilePath=AllocatePool(4+NoirVisorPathLength+4);
		st=FilePath?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
		if(FilePath)
		{
			// Construct Media Device Path with Ending Node
			EFI_DEVICE_PATH_PROTOCOL* EndingPath=(EFI_DEVICE_PATH_PROTOCOL*)((UINTN)FilePath+4+NoirVisorPathLength);
			FilePath->Header.Type=MEDIA_DEVICE_PATH;
			FilePath->Header.SubType=MEDIA_FILEPATH_DP;
			*(UINT16*)FilePath->Header.Length=4+NoirVisorPathLength;
			CopyMem(&FilePath->PathName,NoirVisorPath,NoirVisorPathLength);
			EndingPath->Type=END_DEVICE_PATH_TYPE;
			EndingPath->SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;
			*(UINT16*)EndingPath->Length=4;
			// Get Device Path and Append.
			EFI_DEVICE_PATH_PROTOCOL* DevPath=NULL;
			st=gBS->HandleProtocol(ImageInfo->DeviceHandle,&gEfiDevicePathProtocolGuid,&DevPath);
			if(st==EFI_SUCCESS)
			{
				EFI_DEVICE_PATH_PROTOCOL* HvDevPath=AppendDevicePath(DevPath,&FilePath->Header);
				if(HvDevPath)
				{
					st=gBS->LoadImage(FALSE,ParentImageHandle,HvDevPath,NULL,0,HvImageHandle);
					FreePool(HvDevPath);
				}
			}
			FreePool(FilePath);
		}
	}
	return st;
}

// Make sure each banner line is at most 80 characters.
// Minimal UEFI console supports only 80 columns.
/*
  The Banner ASCII looks like this:
					_____   __       _____         ___    _______                        
					___  | / /______ ___(_)__________ |  / /___(_)______________ ________
					__   |/ / _  __ \__  / __  ___/__ | / / __  / __  ___/_  __ \__  ___/
					_  /|  /  / /_/ /_  /  _  /    __ |/ /  _  /  _(__  ) / /_/ /_  /    
					/_/ |_/   \____/ /_/   /_/     _____/   /_/   /____/  \____/ /_/     
				  __  __         _       _           ____              _____               
				 |  \/  |__ _ __| |___  | |__ _  _  |_  /___ _ _ ___  |_   _|_ _ _ _  __ _ 
				 | |\/| / _` / _` / -_) | '_ | || |  / // -_| '_/ _ \   | |/ _` | ' \/ _` |
				 |_|  |_\__,_\__,_\___| |_.__/\_, | /___\___|_| \___/   |_|\__,_|_||_\__, |
											  |__/                                   |___/ 

  Credits: "Speed" ASCII Art Font, "Small" ASCII Art Font.
*/
EFI_STATUS NoirPrintBanner()
{
	UINTN Col,Row;
	EFI_STATUS st=StdOut->QueryMode(StdOut,StdOut->Mode->Mode,&Col,&Row);
	if(st==EFI_SUCCESS)
	{
		UINTN Mid=(Col-70)>>1;		// There are 70 characters per line in this part of banner
		StdOut->ClearScreen(StdOut);
		StdOut->SetCursorPosition(StdOut,Mid,0);
		StdOut->OutputString(StdOut,L"_____   __       _____         ___    _______                        ");
		StdOut->SetCursorPosition(StdOut,Mid,1);
		StdOut->OutputString(StdOut,L"___  | / /______ ___(_)__________ |  / /___(_)______________ ________");
		StdOut->SetCursorPosition(StdOut,Mid,2);
		StdOut->OutputString(StdOut,L"__   |/ / _  __ \\__  / __  ___/__ | / / __  / __  ___/_  __ \\__  ___/");
		StdOut->SetCursorPosition(StdOut,Mid,3);
		StdOut->OutputString(StdOut,L"_  /|  /  / /_/ /_  /  _  /    __ |/ /  _  /  _(__  ) / /_/ /_  /    ");
		StdOut->SetCursorPosition(StdOut,Mid,4);
		StdOut->OutputString(StdOut,L"/_/ |_/   \\____/ /_/   /_/     _____/   /_/   /____/  \\____/ /_/     ");
		Mid=(Col-76)>>1;			// There are 76 characters per line in this part of banner
		StdOut->SetCursorPosition(StdOut,Mid,5);
		StdOut->OutputString(StdOut,L"  __  __         _       _           ____              _____               ");
		StdOut->SetCursorPosition(StdOut,Mid,6);
		StdOut->OutputString(StdOut,L" |  \\/  |__ _ __| |___  | |__ _  _  |_  /___ _ _ ___  |_   _|_ _ _ _  __ _ ");
		StdOut->SetCursorPosition(StdOut,Mid,7);
		StdOut->OutputString(StdOut,L" | |\\/| / _` / _` / -_) | '_ | || |  / // -_| '_/ _ \\   | |/ _` | ' \\/ _` |");
		StdOut->SetCursorPosition(StdOut,Mid,8);
		StdOut->OutputString(StdOut,L" |_|  |_\\__,_\\__,_\\___| |_.__/\\_, | /___\\___|_| \\___/   |_|\\__,_|_||_\\__, |");
		StdOut->SetCursorPosition(StdOut,Mid,9);
		StdOut->OutputString(StdOut,L"                              |__/                                   |___/ \r\n");
	}
	return st;
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

EFI_STATUS EFIAPI NoirEfiEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	UINT16 RevHi=(UINT16)(SystemTable->Hdr.Revision>>16);
	UINT16 RevLo=(UINT16)(SystemTable->Hdr.Revision&0xffff);
	UINT16 RevP1=RevLo/10,RevP2=RevLo%10;
	EFI_HANDLE HvImage;
	EFI_STATUS st=NoirEfiInitialize(ImageHandle,SystemTable);
	NoirSetConsoleModeToMaximumRows();
	NoirPrintBanner();
	Print(L"Welcome to NoirVisor Loader!\r\n");
	Print(L"Firmware Vendor: %s Revision: %d\r\n",SystemTable->FirmwareVendor,SystemTable->FirmwareRevision);
	Print(L"Firmware UEFI Specification: %d.%d.%d\r\n",RevHi,RevP1,RevP2);
	Print(L"Initialization Status: 0x%X\r\n",st);
	NoirPrintProcessorInformation();
	st=NoirLoadHypervisorDriver(ImageHandle,&HvImage);
	Print(L"Load Image Status: 0x%X\r\n",st);
	if(st==EFI_SUCCESS)
	{
		st=gBS->StartImage(HvImage,NULL,NULL);
		Print(L"Start Image Status: 0x%X\r\n",st);
	}
	Print(L"Press enter key to continue...\r\n");
	NoirBlockUntilKeyStroke(L'\r');
	return EFI_SUCCESS;
}