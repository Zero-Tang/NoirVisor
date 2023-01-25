/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the EFI Application Framework of System.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/efimain.h
*/

#include <Uefi.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DevicePathUtilities.h>
#include <Protocol/HiiFont.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/UgaDraw.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/SmBios.h>
#include <Guid/GlobalVariable.h>
#include <IndustryStandard/SmBios.h>

#define NoirVisorPath		L"\\NoirVisor.efi"
#define NoirVisorPathLength	30

EFI_STATUS EFIAPI UefiBootServicesTableLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI UefiRuntimeServicesTableLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI UefiLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI DevicePathLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);

EFI_SIMPLE_TEXT_INPUT_PROTOCOL *StdIn=NULL;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdOut=NULL;

EFI_GUID gEfiSmBiosTableGuid=SMBIOS_TABLE_GUID;

CHAR8 gEfiCallerBaseName[]="NoirVisor";

extern EFI_BOOT_SERVICES *gBS;
extern EFI_SYSTEM_TABLE *gST;