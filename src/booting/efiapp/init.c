/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the UEFI Inititialization Facility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/init.c
*/

#include "init.h"

EFI_STATUS EFIAPI NoirEfiInitialize(IN EFI_SYSTEM_TABLE *SystemTable)
{
	// Initialize Basic UEFI Utility Services.
	EfiBoot=SystemTable->BootServices;
	EfiRT=SystemTable->RuntimeServices;
	StdIn=SystemTable->ConIn;
	StdOut=SystemTable->ConOut;
	StdErr=SystemTable->StdErr;
	// Initialize Some Protocols
	EFI_STATUS st=EfiBoot->LocateProtocol(&UniColGuid,NULL,&UnicodeCollation);
	if(st!=EFI_SUCCESS)goto InitEnd;
	st=EfiBoot->LocateProtocol(&MpServGuid,NULL,&MpServices);
	if(st!=EFI_SUCCESS)goto InitEnd;
	st=EfiBoot->LocateProtocol(&DevPathUtilGuid,NULL,&DevPathUtil);
	if(st!=EFI_SUCCESS)goto InitEnd;
	st=EfiBoot->LocateProtocol(&FileSysGuid,NULL,&SimpleFileSystem);
InitEnd:
	return st;
}