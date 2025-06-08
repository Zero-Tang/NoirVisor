/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file builds the host environment for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/host.h
*/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>

typedef void (*NOIR_PHYSICAL_MEMORY_RANGE_CALLBACK)
(
	IN UINT64 Start,
	IN UINT64 Length,
	IN OUT VOID* Context
);

typedef struct _MEMORY_RANGE
{
	UINT64 BaseAddress;
	UINT64 RangeSize;
}MEMORY_RANGE,*PMEMORY_RANGE;

typedef void(*noir_broadcast_worker)(void* context,UINT32 ProcessorNumber);

typedef struct _NV_MP_SERVICE_GENERIC_INFO
{
	noir_broadcast_worker Worker;
	VOID* Context;
}NV_MP_SERVICE_GENERIC_INFO,*PNV_MP_SERVICE_GENERIC_INFO;

void noir_debug_output(IN CHAR8 *buffer,IN UINTN length);

extern EFI_MP_SERVICES_PROTOCOL *MpServices;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_SYSTEM_TABLE *gST;

extern BOOLEAN NoirEfiInRuntimeStage;

CHAR8* EfiMemoryTypeNames[]=
{
	"Reserved",
	"Loader Code",
	"Loader Data",
	"Boot-Services Code",
	"Boot-Services Data",
	"Runtime-Services Code",
	"Runtime-Services Data",
	"Conventional Memory",
	"Unusable Memory",
	"ACPI Reclaim Memory",
	"ACPI Memory NVS",
	"Memory-Mapped I/O",
	"Memory-Mapped I/O (Port-Space)",
	"PAL Code",
	"Persistent Memory",
	"Unaccepted"
};

CHAR8* EfiMemoryAttributeNames[]=
{
	"Uncacheable",				// 0x0000000000000001
	"Write-Combined",			// 0x0000000000000002
	"Write-Through",			// 0x0000000000000004
	"Write-Back",				// 0x0000000000000008
	"Uncacheable-Exported",		// 0x0000000000000010
	"",							// 0x0000000000000020
	"",							// 0x0000000000000040
	"",							// 0x0000000000000080
	"","","","",				// 0x0000000000000X00
	"Write-Protected",			// 0x0000000000001000
	"Read-Protected",			// 0x0000000000002000
	"Execute-Protected",		// 0x0000000000004000
	"Non-Volatile",				// 0x0000000000008000
	"More-Reliable",			// 0x0000000000010000
	"Read-Only",				// 0x0000000000020000
	"Specific-Purpose",			// 0x0000000000040000
	"CPU-Crypto",				// 0x0000000000080000
	"","","","","","","","",	// 0x000000000XX00000
	"","","","","","","","",	// 0x0000000XX0000000
	"","","","","","","","",	// 0x00000XX000000000
	"","","","","","","","",	// 0x000XX00000000000
	"","","","","","","","",	// 0x0XX0000000000000
	"",							// 0x1000000000000000
	"",							// 0x2000000000000000
	"",							// 0x4000000000000000
	"Runtime"					// 0x8000000000000000
};