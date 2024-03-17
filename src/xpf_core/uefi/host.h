/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

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

#define MSR_SYSENTER_CS		0x174
#define MSR_SYSENTER_ESP	0x175
#define MSR_SYSENTER_EIP	0x176
#define MSR_DEBUG_CONTROL	0x1D9
#define MSR_PAT				0x277
#define MSR_EFER			0xC0000080
#define MSR_STAR			0xC0000081
#define MSR_LSTAR			0xC0000082
#define MSR_CSTAR			0xC0000083
#define MSR_SFMASK			0xC0000084
#define MSR_FSBASE			0xC0000100
#define MSR_GSBASE			0xC0000101
#define MSR_GSSWAP			0xC0000102

#define GDT_SELECTOR_MASK		0xFFF8

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

typedef struct _SEGMENT_REGISTER
{
	UINT16 Selector;
	UINT16 Attributes;
	UINT32 Limit;
	UINT64 Base;
}SEGMENT_REGISTER,*PSEGMENT_REGISTER;

typedef struct _NOIR_PROCESSOR_STATE
{
	SEGMENT_REGISTER Cs;
	SEGMENT_REGISTER Ds;
	SEGMENT_REGISTER Es;
	SEGMENT_REGISTER Fs;
	SEGMENT_REGISTER Gs;
	SEGMENT_REGISTER Ss;
	SEGMENT_REGISTER Tr;
	SEGMENT_REGISTER Gdtr;
	SEGMENT_REGISTER Idtr;
	SEGMENT_REGISTER Ldtr;
	UINTN Cr0;
	UINTN Cr2;
	UINTN Cr3;
	UINTN Cr4;
#if defined(MDE_CPU_X64)
	UINT64 Cr8;
#endif
	UINTN Dr0;
	UINTN Dr1;
	UINTN Dr2;
	UINTN Dr3;
	UINTN Dr6;
	UINTN Dr7;
	UINT64 SysEnter_Cs;
	UINT64 SysEnter_Esp;
	UINT64 SysEnter_Eip;
	UINT64 DebugControl;
	UINT64 Pat;
	UINT64 Efer;
	UINT64 Star;
	UINT64 LStar;
	UINT64 CStar;
	UINT64 SfMask;
	UINT64 FsBase;
	UINT64 GsBase;
	UINT64 GsSwap;
}NOIR_PROCESSOR_STATE,*PNOIR_PROCESSOR_STATE;

typedef void(*noir_broadcast_worker)(void* context,UINT32 ProcessorNumber);

typedef struct _NV_MP_SERVICE_GENERIC_INFO
{
	noir_broadcast_worker Worker;
	VOID* Context;
}NV_MP_SERVICE_GENERIC_INFO,*PNV_MP_SERVICE_GENERIC_INFO;

#define NoirSaveProcessorState		noir_save_processor_state
#define NoirGetSegmentAttributes	noir_get_segment_attributes
void noir_save_processor_state(OUT PNOIR_PROCESSOR_STATE State);
void __cdecl NoirDebugPrint(IN CONST CHAR8 *Format,...);
void NoirBlockUntilKeyStroke(IN CHAR16 Unicode);
void NoirUnexpectedInterruptHandler(void);
void NoirSetupDebugSupportPerProcessor(IN VOID *ProcedureArgument);
UINT64 nvc_hpet_read_counter();

UINT8 NoirGetInstructionLength16(IN UINT8 *Code,IN UINTN CodeLength);
UINT8 NoirGetInstructionLength32(IN UINT8 *Code,IN UINTN CodeLength);
UINT8 NoirGetInstructionLength64(IN UINT8 *Code,IN UINTN CodeLength);
UINT8 NoirDisasmCode16(OUT CHAR8 *Mnemonic,IN UINTN MnemonicLength,IN UINT8 *Code,IN UINTN CodeLength,IN UINT64 VirtualAddress);
UINT8 NoirDisasmCode32(OUT CHAR8 *Mnemonic,IN UINTN MnemonicLength,IN UINT8 *Code,IN UINTN CodeLength,IN UINT64 VirtualAddress);
UINT8 NoirDisasmCode64(OUT CHAR8 *Mnemonic,IN UINTN MnemonicLength,IN UINT8 *Code,IN UINTN CodeLength,IN UINT64 VirtualAddress);

extern EFI_MP_SERVICES_PROTOCOL *MpServices;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_SYSTEM_TABLE *gST;

extern BOOLEAN NoirEfiInRuntimeStage;
extern UINT32 hpet_period;

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