/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the EFI Application Framework of System.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/efimain.c
*/

#include <Uefi.h>
#include <UnicodeCollation.h>
#include "efimain.h"

EFI_STATUS EFIAPI NoirEfiInitialize(IN EFI_SYSTEM_TABLE *SystemTable)
{
	// Initialize Basic UEFI Utility Services.
	EfiBoot=SystemTable->BootServices;
	EfiRT=SystemTable->RuntimeServices;
	StdIn=SystemTable->ConIn;
	StdOut=SystemTable->ConOut;
	StdErr=SystemTable->StdErr;
	// Initialize Some Protocols
	EFI_GUID UniColGuid=EFI_UNICODE_COLLATION_PROTOCOL2_GUID;
	EFI_STATUS st=EfiBoot->LocateProtocol(&UniColGuid,NULL,&UnicodeCollation);
	StdOut->ClearScreen(StdOut);
	return st;
}

EFI_STATUS EFIAPI NoirEfiUnload(IN EFI_HANDLE ImageHandle)
{
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI NoirEfiEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{
	NoirConsolePrintfW(L"Welcome to NoirVisor!\r\n");
	return EFI_SUCCESS;
}