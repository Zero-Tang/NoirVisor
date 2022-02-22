/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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

#pragma pack(4)
typedef struct _NHTSS64
{
	UINT32 Ignored;		// Offset=0x00
	UINT64 Rsp0;		// Offset=0x04
	UINT64 Rsp1;		// Offset=0x0C
	UINT64 Rsp2;		// Offset=0x14
	UINT64 Ist[8];		// Offset=0x1C
	UINT64 Reserved2;	// Offset=0x5C
	UINT16 Reserved3;	// Offset=0x64
	UINT16 IoMapBase;	// Offset=0x66
}NHTSS64,*PNHTSS64;
#pragma pack()

typedef union _NHGDTCODE
{
	struct
	{
		UINT16 Accessed:1;		// Bit	0
		UINT16 Readable:1;		// Bit	1
		UINT16 Conforming:1;	// Bit	2
		UINT16 CodeData:1;		// Bit	3
		UINT16 System:1;		// Bit	4
		UINT16 DPL:2;			// Bits	5-6
		UINT16 Present:1;		// Bit	7
		UINT16 LimitHi:4;		// Bits	8-11
		UINT16 Available:1;		// Bit	12
		UINT16 LongMode:1;		// Bit	13
		UINT16 DefaultBig:1;	// Bit	14
		UINT16 Granularity:1;	// Bit	15
	};
	UINT16 Value;
}NHGDTCODE,*PNHGDTCODE;

typedef union _NHGDTDATA
{
	struct
	{
		UINT16 Accessed:1;		// Bit	0
		UINT16 Writable:1;		// Bit	1
		UINT16 ExpandDown:1;	// Bit	2
		UINT16 CodeData:1;		// Bit	3
		UINT16 System:1;		// Bit	4
		UINT16 DPL:2;			// Bits	5-6
		UINT16 Present:1;		// Bit	7
		UINT16 LimitHi:4;		// Bits	8-11
		UINT16 Available:1;		// Bit	12
		UINT16 LongMode:1;		// Bit	13
		UINT16 DefaultBig:1;	// Bit	14
		UINT16 Granularity:1;	// Bit	15
	};
	UINT16 Value;
}NHGDTDATA,*PNHGDTDATA;

// Definitions of System Segment Types
#define X64_GDT_LDT				0x2
#define X64_GDT_AVAILABLE_TSS	0x9
#define X64_GDT_BUSY_TSS		0xB
#define X64_GDT_CALL_GATE		0xC
#define X64_GDT_INTERRUPT_GATE	0xE
#define X64_GDT_TRAP_GATE		0xF

#pragma pack(1)
typedef union _NHGDTENTRY64
{
	struct
	{
		UINT64 LimitLo:16;		// Bits	0-15
		UINT64 BaseLo:24;		// Bits	16-39
		UINT64 Type:5;			// Bits	40-44
		UINT64 Dpl:2;			// Bits	45-46
		UINT64 Present:1;		// Bit	47
		UINT64 LimitHi:4;		// Bits	48-51
		UINT64 Available:1;		// Bit	52
		UINT64 LongMode:1;		// Bit	53
		UINT64 DefaultBig:1;	// Bit	54
		UINT64 Granularity:1;	// Bit	55
		UINT64 BaseHi:8;		// Bits	56-63
	}Bits;
	struct
	{
		UINT16 LimitLo;
		UINT16 BaseLo;
		UINT8 BaseMid;
		UINT16 Flags;
		UINT8 BaseHi;
	}Bytes;
	UINT64 Value;
}NHGDTENTRY64,*PNHGDTENTRY64;
#pragma pack()

#pragma pack(1)
typedef struct _NHIDTENTRY64
{
	UINT16 OffsetLow;
	UINT16 Selector;
	struct
	{
		UINT16 IstIndex:3;	// Bits 0-2
		UINT16 Reserved0:5;	// Bits	3-7
		UINT16 Type:5;		// Bits	8-12
		UINT16 DPL:2;		// Bits	13-14
		UINT16 Present:1;	// Bit	15
	};
	UINT16 OffsetMid;
	UINT32 OffsetHigh;
	UINT32 Reserved1;
}NHIDTENTRY64,*PNHIDTENTRY64;
#pragma pack()

// Root of Paging.
typedef union _NHX64_PML4E
{
	struct
	{
		UINT64 Present:1;		// Bit	0
		UINT64 Write:1;			// Bit	1
		UINT64 User:1;			// Bit	2
		UINT64 PWT:1;			// Bit	3
		UINT64 PCD:1;			// Bit	4
		UINT64 Accessed:1;		// Bit	5
		UINT64 Ignored:1;		// Bit	6
		UINT64 Reserved:2;		// Bits	7-8
		UINT64 Avl:3;			// Bits	9-11
		UINT64 PdptBase:40;		// Bits	12-51
		UINT64 Available:11;	// Bits	52-62
		UINT64 NoExecute:1;		// Bit	63
	};
	UINT64 Value;
}NHX64_PML4E,*PNHX64_PML4E;

// We will use 1GiB Huge Page.
typedef union _NHX64_HUGE_PDPTE
{
	struct
	{
		UINT64 Present:1;		// Bit	0
		UINT64 Write:1;			// Bit	1
		UINT64 User:1;			// Bit	2
		UINT64 PWT:1;			// Bit	3
		UINT64 PCD:1;			// Bit	4
		UINT64 Accessed:1;		// Bit	5
		UINT64 Dirty:1;			// Bit	6
		UINT64 PageSize:1;		// Bit	7
		UINT64 Global:1;		// Bit	8
		UINT64 Avl:3;			// Bits	9-11
		UINT64 PAT:1;			// Bit	12
		UINT64 Reserved:17;		// Bits	13-29
		UINT64 PageBase:22;		// Bits	30-51
		UINT64 Available:7;		// Bits	52-58
		UINT64 MpKey:4;			// Bits	59-62
		UINT64 NoExecute:1;		// Bit	63
	};
	UINT64 Value;
}NHX64_HUGE_PDPTE,*PNHX64_HUGE_PDPTE;

// Definitions of NoirVisor Host Segment Selectors
// These selectors are only used if processor enters trusted computing mode.
#define NH_X64_CS_SELECTOR32	0x08
#define NH_X64_CS_SELECTOR64	0x10
#define NH_X64_DS_SELECTOR64	0x18
#define NH_X64_ES_SELECTOR64	0x18
#define NH_X64_FS_SELECTOR64	0x20
#define NH_X64_GS_SELECTOR64	0x30
#define NH_X64_SS_SELECTOR64	0x18
#define NH_X64_TR_SELECTOR64	0x40
#define NH_X64_NULL_SELECTOR	0x00

#define NH_INTERRUPT_GATE_TYPE	0xE

// Definitions of NoirVisor Host Segment Attributes
#define NH_X64_CODE_ATTRIBUTES	0xA09B
#define NH_X64_DATA_ATTRIBUTES	0xC093
#define NH_X64_TASK_ATTRIBUTES	0x8B

// NoirVisor Host Processor Control Region
typedef struct _NHPCR
{
	struct _NHPCR* Self;		// Point to this structure (Base of FS&GS)
	UINTN ProcessorNumber;		// Current Processor Number
	IA32_DESCRIPTOR GdtR;		// Global Descriptor Table
	IA32_DESCRIPTOR IdtR;		// Interrupt Descriptor Table
	PNHGDTENTRY64 Gdt;			// Global Descriptor Table
	PNHIDTENTRY64 Idt;			// Interrupt Descriptor Table
	PNHTSS64 Tss;				// Task Segment State
	VOID* IntStack;				// Stack Base for Interrupt Handler.
	// Processor State will be forged for NoirVisor in Host Mode.
	CHAR8 ProcessorVendor[13];
}NHPCR,*PNHPCR;

// NoirVisor Host Processor Control Region Page
typedef struct _NHPCRP
{
	NHPCR Core;
	UINT8 Misc[EFI_PAGE_SIZE-sizeof(NHPCR)];
}NHPCRP,*PNHPCRP;

typedef struct _NV_HOST_SYSTEM
{
	PNHPCRP ProcessorBlocks;
	UINTN NumberOfProcessors;
	PNHX64_PML4E Pml4Base;		// Paging for NoirVisor in Host Mode
	PNHX64_HUGE_PDPTE PdptBase;	// Paging for NoirVisor in Host Mode
}NV_HOST_SYSTEM,*PNV_HOST_SYSTEM;

typedef void(*noir_broadcast_worker)(void* context,UINT32 ProcessorNumber);

typedef struct _NV_MP_SERVICE_GENERIC_INFO
{
	noir_broadcast_worker Worker;
	VOID* Context;
}NV_MP_SERVICE_GENERIC_INFO,*PNV_MP_SERVICE_GENERIC_INFO;

#define NoirSaveProcessorState		noir_save_processor_state
#define NoirSetHostInterruptHandler		noir_
void noir_save_processor_state(OUT PNOIR_PROCESSOR_STATE State);
void NoirBlockUntilKeyStroke(IN CHAR16 Unicode);
void NoirUnexpectedInterruptHandler(void);

extern EFI_MP_SERVICES_PROTOCOL *MpServices;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_SYSTEM_TABLE *gST;

NV_HOST_SYSTEM Host;