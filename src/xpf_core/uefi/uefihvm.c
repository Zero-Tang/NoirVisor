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
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/PeImage.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Register/Intel/Cpuid.h>
#include "uefihvm.h"

void NoirTestCpuid()
{
	CPUID_VERSION_INFO_ECX VerInfoEcx;
	AsmCpuid(CPUID_VERSION_INFO,NULL,NULL,(UINT32*)&VerInfoEcx,NULL);
	if(VerInfoEcx.Bits.ParaVirtualized)
	{
		CHAR8 TempBuffer[512];
		UINTN Length;
		UINT32 MaximumLeaf;
		CHAR8 VendorString[13];
		AsmCpuid(CPUID_LEAF_HV_VENDOR_ID,&MaximumLeaf,(UINT32*)&VendorString[0],(UINT32*)&VendorString[4],(UINT32*)&VendorString[8]);
		VendorString[12]='\0';
		Length=AsciiSPrint(TempBuffer,sizeof(TempBuffer),"[Test] Hypervisor is detected! Maximum Leaf: 0x%X, Vendor: %a\n",MaximumLeaf,VendorString);
		NoirSerialWrite(1,(UINT8*)TempBuffer,Length);
		if(MaximumLeaf>=CPUID_LEAF_HV_VENDOR_NEUTRAL)
		{
			CHAR8 Signature[5];
			AsmCpuid(CPUID_LEAF_HV_VENDOR_NEUTRAL,(UINT32*)Signature,NULL,NULL,NULL);
			Signature[4]='\0';
			Length=AsciiSPrint(TempBuffer,sizeof(TempBuffer),"[Test] Signature: %a\n",Signature);
			NoirSerialWrite(1,(UINT8*)TempBuffer,Length);
		}
	}
}

UINT32 NoirBuildHypervisor()
{
	UINT32 st=nvc_build_hypervisor();
	NoirTestCpuid();
	return st;
}

void NoirTeardownHypervisor()
{
	nvc_teardown_hypervisor();
}

UINT64 noir_query_enabled_features_in_system()
{
	// Future implementation will support user-defined configuration.
	// Current implementation will force the CPUID-presence.
	return NOIR_HVM_FEATURE_CPUID_PRESENCE;
}

void nvc_store_image_info(OUT VOID** Base,OUT UINT32* Size)
{
	if(Base)*Base=NvImageBase;
	if(Size)*Size=NvImageSize;
}

void NoirSaveImageInfo(IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol)
{
	if(LoadedImageProtocol)
	{
		NvImageBase=LoadedImageProtocol->ImageBase;
		NvImageSize=(UINT32)LoadedImageProtocol->ImageSize;
	}
}

// When system enters runtime stage, the operating system may relocate the pointers
// referenced by NoirVisor, and it may thereby results in unpredictable behaviors.
// Most common errors would be #PF exceptions in Host Code.
void NoirSuppressImageRelocation(IN VOID* ImageBase)
{
	EFI_IMAGE_DOS_HEADER *DosHead=(EFI_IMAGE_DOS_HEADER*)ImageBase;
	if(DosHead->e_magic==EFI_IMAGE_DOS_SIGNATURE)
	{
		EFI_IMAGE_NT_HEADERS *NtHead=(EFI_IMAGE_NT_HEADERS*)((UINTN)ImageBase+DosHead->e_lfanew);
		// Make the NT Header Signature invalid so the firmware won't relocate pointers referenced by NoirVisor.
		NtHead->Signature=0;
	}
}

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

// Sleep-and-Wake setup in ACPI.
BOOLEAN noir_query_pm1_port_address(OUT UINT16 *PM1a,OUT UINT16 *PM1b)
{
	EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt=(EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE*)EfiLocateFirstAcpiTable(EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE);
	if(Fadt)
	{
		NoirDebugPrint("Located ACPI FADT at 0x%p\n",Fadt);
		// Use non-extended data for now.
		*PM1a=(UINT16)Fadt->Pm1aCntBlk;
		*PM1b=(UINT16)Fadt->Pm1bCntBlk;
		// Do some assertions.
		if(Fadt->XPm1aCntBlk.AddressSpaceId!=EFI_ACPI_2_0_SYSTEM_IO)
			NoirDebugPrint("X-PM1a does not use port I/O! It use space type %u instead!\n",Fadt->XPm1aCntBlk.AddressSpaceId);
		else
			NoirDebugPrint("X-PM1a regoster bit width: %u\n",Fadt->XPm1aCntBlk.RegisterBitWidth);
		if(Fadt->XPm1bCntBlk.AddressSpaceId!=EFI_ACPI_2_0_SYSTEM_IO)
			NoirDebugPrint("X-PM1b does not use port I/O! It use space type %u instead!\n",Fadt->XPm1bCntBlk.AddressSpaceId);
		else
			NoirDebugPrint("X-PM1b regoster bit width: %u\n",Fadt->XPm1bCntBlk.RegisterBitWidth);
		NoirDebugPrint("X-PM1a Address=0x%llX, PM1a Address=0x%X\n",Fadt->XPm1aCntBlk.Address,Fadt->Pm1aCntBlk);
		NoirDebugPrint("X-PM1b Address=0x%llX, PM1b Address=0x%X\n",Fadt->XPm1bCntBlk.Address,Fadt->Pm1bCntBlk);
	}
	return FALSE;
}