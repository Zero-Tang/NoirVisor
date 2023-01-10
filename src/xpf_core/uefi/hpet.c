/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file defines operations with High-Pricision Event Timer (HPET).

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/hpet.c
*/

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/UefiLib.h>
#include "hpet.h"

EFI_STATUS NoirSelectBestHpetHardware()
{
	EFI_STATUS st=EFI_NOT_FOUND;
	EFI_ACPI_HIGH_PRECISION_EVENT_TIMER_TABLE_HEADER *HpetEntry=(EFI_ACPI_HIGH_PRECISION_EVENT_TIMER_TABLE_HEADER*)EfiLocateFirstAcpiTable(EFI_ACPI_3_0_HIGH_PRECISION_EVENT_TIMER_TABLE_SIGNATURE);
	if(HpetEntry)
	{
		// The best HPET hardware we are looking for has smallest incremental period. (i.e: most precise)
		UINT64 HpetBase=HpetEntry->BaseAddressLower32Bit.Address;
		UINT32 LowestPeriod=0xFFFFFFFF;
		do
		{
			UINT32 Period=MmioRead32(HpetBase+HpetGeneralCounterClockPeriod);
			if(Period<LowestPeriod)
			{
				LowestPeriod=Period;
				NoirHpetHardware=HpetBase;
			}
			HpetEntry=(EFI_ACPI_HIGH_PRECISION_EVENT_TIMER_TABLE_HEADER*)EfiLocateNextAcpiTable(EFI_ACPI_3_0_HIGH_PRECISION_EVENT_TIMER_TABLE_SIGNATURE,(EFI_ACPI_COMMON_HEADER*)HpetEntry);
		}while(HpetEntry);
		NoirHpetIncrementingPeriod=LowestPeriod;
		st=EFI_SUCCESS;
	}
	return st;
}

UINT64 NoirReadHpetCounter()
{
	if(NoirHpetHardware)return MmioRead64(NoirHpetHardware+HpetMainCounterValue);
	StdOut->OutputString(StdOut,L"Warning: No HPET hardware is selected!\r\n");
	return 0;
}