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

void __cdecl NoirConsolePrintfW(CHAR16* format,...);

EFI_BOOT_SERVICES* EfiBoot=NULL;
EFI_RUNTIME_SERVICES* EfiRT=NULL;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL* StdIn=NULL;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdOut=NULL;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr=NULL;
EFI_UNICODE_COLLATION_PROTOCOL* UnicodeCollation=NULL;