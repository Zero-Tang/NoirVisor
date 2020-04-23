/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the UEFI Inititialization Facility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/init.h
*/

#include <Uefi.h>
#include <Protocol/UnicodeCollation.h>
#include <Protocol/LoadedImage.h>
#include <Pi/PiDxeCis.h>
#include <Protocol/MpService.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathUtilities.h>
#include <Protocol/SimpleFileSystem.h>

// Protocol Variables
EFI_BOOT_SERVICES* EfiBoot=NULL;
EFI_RUNTIME_SERVICES* EfiRT=NULL;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL* StdIn=NULL;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdOut=NULL;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr=NULL;
EFI_UNICODE_COLLATION_PROTOCOL* UnicodeCollation=NULL;
EFI_MP_SERVICES_PROTOCOL* MpServices=NULL;
EFI_DEVICE_PATH_UTILITIES_PROTOCOL* DevPathUtil=NULL;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFileSystem=NULL;

// Protocol GUIDs
EFI_GUID UniColGuid=EFI_UNICODE_COLLATION_PROTOCOL2_GUID;
EFI_GUID MpServGuid=EFI_MP_SERVICES_PROTOCOL_GUID;
EFI_GUID DevPathUtilGuid=EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID;
EFI_GUID FileSysGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;