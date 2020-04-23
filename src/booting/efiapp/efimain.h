/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the EFI Application Framework of System.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/efimain.h
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

#define NoirVisorPath			L"\\NoirVisor.efi"
#define NoirVisorPathLength		30

void __cdecl NoirConsolePrintfW(CHAR16* format,...);
EFI_STATUS EFIAPI NoirEfiInitialize(IN EFI_SYSTEM_TABLE *SystemTable);

extern EFI_BOOT_SERVICES* EfiBoot;
extern EFI_RUNTIME_SERVICES* EfiRT;
extern EFI_SIMPLE_TEXT_INPUT_PROTOCOL* StdIn;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdOut;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr;
extern EFI_UNICODE_COLLATION_PROTOCOL* UnicodeCollation;
extern EFI_MP_SERVICES_PROTOCOL* MpServices;
extern EFI_DEVICE_PATH_UTILITIES_PROTOCOL* DevPathUtil;
extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFileSystem;