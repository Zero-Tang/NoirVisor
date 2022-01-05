/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/uefihvm.c
*/

#include <Uefi.h>
#include <IndustryStandard/PeImage.h>
#include <Library/BaseLib.h>
#include "uefihvm.h"

BOOLEAN NoirInitializeCodeIntegrity(IN VOID* ImageBase)
{
	// Locate Section List
	EFI_IMAGE_DOS_HEADER *DosHead=(EFI_IMAGE_DOS_HEADER*)ImageBase;
	if(DosHead->e_magic==EFI_IMAGE_DOS_SIGNATURE)
	{
		EFI_IMAGE_NT_HEADERS *NtHead=(EFI_IMAGE_NT_HEADERS*)((UINTN)ImageBase+DosHead->e_lfanew);
		if(NtHead->Signature==EFI_IMAGE_NT_SIGNATURE)
		{
			EFI_IMAGE_SECTION_HEADER *SectionHeaders=(EFI_IMAGE_SECTION_HEADER*)((UINTN)NtHead+sizeof(EFI_IMAGE_NT_HEADERS));
			for(UINT16 i=0;i<NtHead->FileHeader.NumberOfSections;i++)
			{
				// Locate Code Section
				if(AsciiStrnCmp((CHAR8*)SectionHeaders[i].Name,".text",EFI_IMAGE_SIZEOF_SHORT_NAME)==0)
				{
					VOID* CodeBase=(VOID*)((UINTN)ImageBase+SectionHeaders[i].VirtualAddress);
					UINT32 CodeSize=SectionHeaders[i].SizeOfRawData;
					// Software CI Enforcement won't be supported in EFI Runtime Stage.
					// Hence, we will run Hardware CI Enforcement only in EFI.
					return noir_initialize_ci(CodeBase,CodeSize,FALSE,TRUE);
				}
			}
		}
	}
	return FALSE;
}

void NoirFinalizeCodeIntegrity()
{
	noir_finalize_ci();
}