/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the EFI Memory Management Utility

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/devpath.c
*/

#include <Uefi.h>
#include "ncrt.h"

// These functions are intended for NoirVisor Loader Application
void* NoirAllocateMemory(IN UINTN Size)
{
	void* p=NULL;
	EFI_STATUS st=EfiBoot->AllocatePool(EfiLoaderData,Size,&p);
	if(st==EFI_SUCCESS)EfiBoot->SetMem(p,Size,0);
	return p;
}

void NoirFreeMemory(IN void* Memory)
{
	EfiBoot->FreePool(p);
}