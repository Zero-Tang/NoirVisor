/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file builds the host environment for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/host.c
*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>
#include <Register/Intel/Cpuid.h>
#include <stdarg.h>
#include "host.h"

void NoirFreeHostEnvironment()
{
	if(Host.ProcessorBlocks)
	{
		FreePool(Host.ProcessorBlocks);
		Host.ProcessorBlocks=NULL;
	}
	if(Host.Pml4Base)
	{
		FreePages(Host.Pml4Base,1);
		Host.Pml4Base=NULL;
	}
	if(Host.PdptBase)
	{
		FreePages(Host.PdptBase,1);
		Host.PdptBase=NULL;
	}
}

void noir_get_prebuilt_host_processor_state(OUT PNOIR_PROCESSOR_STATE State)
{
	UINTN CurProc=0;
	if(MpServices)MpServices->WhoAmI(MpServices,&CurProc);
	ZeroMem(State,sizeof(NOIR_PROCESSOR_STATE));
	// Only necessary states are filled in.
	// Segment Selectors
	State->Cs.Selector=AsmReadCs();
	State->Ds.Selector=AsmReadDs();
	State->Es.Selector=AsmReadEs();
	State->Fs.Selector=AsmReadFs();
	State->Gs.Selector=AsmReadGs();
	State->Ss.Selector=AsmReadSs();
	// GDT & IDT Registers
	State->Gdtr.Limit=Host.ProcessorBlocks[CurProc].Core.GdtR.Limit;
	State->Idtr.Limit=Host.ProcessorBlocks[CurProc].Core.IdtR.Limit;
	State->Gdtr.Base=Host.ProcessorBlocks[CurProc].Core.GdtR.Base;
	State->Idtr.Base=Host.ProcessorBlocks[CurProc].Core.IdtR.Base;
	// Task Segment State
	State->Tr=Host.ProcessorBlocks[CurProc].Core.Tr;
	// Paging Base
	State->Cr3=(UINTN)Host.Pml4Base;
	// CR4.
	State->Cr4=AsmReadCr4();
	// EFER
	State->Efer=AsmReadMsr64(MSR_EFER);
	// GS Segment.
	State->Gs.Attributes=NH_X64_DATA_ATTRIBUTES;
	State->Gs.Limit=0xFFFFFFFF;
	State->Gs.Base=(UINT64)Host.ProcessorBlocks[CurProc].Core.Self;
}

UINT16 NoirAllocateSegmentSelector(IN PNHGDTENTRY64 GdtBase,IN UINT16 Limit)
{
	for(UINT16 i=8;i<Limit;i+=8)
	{
		if(GdtBase[i>>3].Bits.Present)
		{
			switch(GdtBase[i>>3].Bits.Type)
			{
				case X64_GDT_LDT:
				case X64_GDT_AVAILABLE_TSS:
				case X64_GDT_BUSY_TSS:
				{
					i+=8;
					break;
				}
			}
		}
		else
		{
			// This selector is available for use.
			GdtBase[i>>3].Bits.Present=1;	// Mark as used.
			return i;
		}
	}
	return 0;
}

void NoirPrepareHostProcedureAp(IN VOID *ProcedureArgument)
{
	PNHPCRP Pcr=(PNHPCRP)ProcedureArgument;
	UINTN AllocationBase;
	IA32_DESCRIPTOR IdtR,GdtR;
	VOID *NewGdt,*NewIdt;
	UINTN CurProc=0;
	UINT16 TrSel,TrAttrib;
	PNHGDTENTRY64 TssGdtEntry;
	// Get the processor number.
	if(MpServices)MpServices->WhoAmI(MpServices,&CurProc);
	AllocationBase=((UINTN)Pcr[CurProc].Misc+0xF)&0xFFFFFFFFFFFFFFF0;
	Pcr[CurProc].Core.ProcessorNumber=CurProc;
	// Copy the GDT and IDT.
	AsmReadGdtr(&GdtR);
	AsmReadIdtr(&IdtR);
	AsmReadIdtr(&Pcr[CurProc].Core.OldIdtR);
	NewIdt=(VOID*)Pcr[CurProc].IdtBuffer;
	NewGdt=(VOID*)AllocationBase;
	CopyMem(NewGdt,(VOID*)GdtR.Base,GdtR.Limit+1);
	CopyMem(NewIdt,(VOID*)IdtR.Base,IdtR.Limit+1);
	Pcr[CurProc].Core.Gdt=(PNHGDTENTRY64)NewGdt;
	Pcr[CurProc].Core.Idt=(PNHIDTENTRY64)NewIdt;
	Pcr[CurProc].Core.GdtR.Limit=GdtR.Limit+32;		// Give 32 more bytes to allow allocating segments.
	Pcr[CurProc].Core.IdtR.Limit=EFI_PAGE_SIZE-1;
	Pcr[CurProc].Core.GdtR.Base=(UINTN)NewGdt;
	Pcr[CurProc].Core.IdtR.Base=(UINTN)NewIdt;
	// Initialize TSS.
	TrSel=AsmReadTr();
	TrAttrib=NoirGetSegmentAttributes(TrSel,NewGdt);
	if((TrAttrib & 0x80) && TrSel!=0)	// Is TSS present?
		Pcr[CurProc].Core.Tr.Selector=TrSel;
	else								// If TSS is absent, allocate a selector for it.
		Pcr[CurProc].Core.Tr.Selector=NoirAllocateSegmentSelector(NewGdt,Pcr[CurProc].Core.GdtR.Limit+33);
	Pcr[CurProc].Core.Tr.Attributes=NH_X64_TASK_ATTRIBUTES;
	Pcr[CurProc].Core.Tr.Limit=sizeof(NHTSS64);
	Pcr[CurProc].Core.Tr.Base=AllocationBase+Pcr[CurProc].Core.GdtR.Limit+33;
	// Guarantee that TSS Base is aligned on 16-byte boundary
	if(Pcr[CurProc].Core.Tr.Base & 0xF)
	{
		Pcr[CurProc].Core.Tr.Base+=0xF;
		Pcr[CurProc].Core.Tr.Base&=0xFFFFFFFFFFFFFFF0;
	}
	Pcr[CurProc].Core.Tss=(PNHTSS64)Pcr[CurProc].Core.Tr.Base;
	// Initialize TSS in the GDT.
	TssGdtEntry=(PNHGDTENTRY64)((UINTN)NewGdt+Pcr[CurProc].Core.Tr.Selector);
	TssGdtEntry->Value=0;
	TssGdtEntry->Bits.LimitLo=Pcr[CurProc].Core.Tr.Limit;
	TssGdtEntry->Bits.BaseLo=Pcr[CurProc].Core.Tr.Base&0xFFFFFF;
	TssGdtEntry->Bits.Type=X64_GDT_AVAILABLE_TSS;
	TssGdtEntry->Bits.Present=TRUE;
	TssGdtEntry->Bits.BaseHi=(Pcr[CurProc].Core.Tr.Base>>24)&0xFF;
	*(UINT32*)&TssGdtEntry[1]=Pcr[CurProc].Core.Tr.Base>>32;
	// As a matter of fact, there is nothing special in TSS required to be set up.
	// Debug Purpose: Overwrite the default IDT.
	NoirSetupDebugSupportPerProcessor(NULL);
	AsmWriteIdtr(&Pcr[CurProc].Core.IdtR);
	AsmWriteGdtr(&Pcr[CurProc].Core.GdtR);
	AsmWriteTr(Pcr[CurProc].Core.Tr.Selector);
}

// This function is invoked by Center HVM Core prior to subverting system,
// but do not load host's special processor state
EFI_STATUS NoirBuildHostEnvironment()
{
	UINTN EnabledProcessors=0;
	EFI_STATUS st=EFI_OUT_OF_RESOURCES;
	ZeroMem(&Host,sizeof(Host));
	if(MpServices)
	{
		// Query Number of Processors
		st=MpServices->GetNumberOfProcessors(MpServices,&Host.NumberOfProcessors,&EnabledProcessors);
	}
	else
	{
		// If MP-Services is unavailable, only one logical processor (the BSP) is accessible.
		Host.NumberOfProcessors=EnabledProcessors=1;
		st=EFI_SUCCESS;
	}
	if(st==EFI_SUCCESS)
	{
		// Initialize Host System.
		Host.ProcessorBlocks=AllocateRuntimePages(EFI_SIZE_TO_PAGES(sizeof(NHPCRP)*Host.NumberOfProcessors));
		if(sizeof(NHPCRP) & EFI_PAGE_MASK)Print(L"[Assertion] Size of NHPCR is not aligned at page! Size=0x%X\n",sizeof(NHPCRP));
		Host.Pml4Base=AllocateRuntimePages(1);
		Host.PdptBase=AllocateRuntimePages(1);
		if(Host.ProcessorBlocks && Host.Pml4Base && Host.PdptBase)
		{
			// Initialize Host Paging supporting 512GiB data.
			// Initialize Host Paging - Page Map Level 4 Entries.
			ZeroMem(Host.Pml4Base,EFI_PAGE_SIZE);
			Host.Pml4Base->Value=(UINT64)Host.PdptBase;
			Host.Pml4Base->Present=1;
			Host.Pml4Base->Write=1;
			// Initialize Host Paging - Page Directory Pointer Table Entries.
			for(UINT64 i=0;i<512;i++)
			{
				Host.PdptBase[i].Value=(i<<30);
				Host.PdptBase[i].Present=1;
				Host.PdptBase[i].Write=1;
				// Let's use 1GiB Huge Pages for simplest implementation.
				Host.PdptBase[i].PageSize=1;
			}
			Print(L"NoirVisor starts initialization per processor...\n");
			// Initialize Per-Processor Blocks by broadcasting.
			NoirPrepareHostProcedureAp(Host.ProcessorBlocks);
			if(MpServices)MpServices->StartupAllAPs(MpServices,NoirPrepareHostProcedureAp,TRUE,NULL,0,Host.ProcessorBlocks,NULL);
			// If it reaches here, the initialization is successful.
			Print(L"NoirVisor Host Context is initialized successfully! Host System=0x%p\n",&Host);
			system_cr3=(UINT64)Host.Pml4Base;
			st=EFI_SUCCESS;
		}
		else
		{
			NoirFreeHostEnvironment();
			st=EFI_OUT_OF_RESOURCES;
		}
	}
	return st;
}

void noir_set_host_interrupt_handler(IN UINT8 Vector,IN UINTN HandlerRoutine)
{
	UINTN CurProc=0;
	if(MpServices)MpServices->WhoAmI(MpServices,&CurProc);
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].OffsetLow=HandlerRoutine&0xFFFF;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].OffsetMid=(HandlerRoutine>>16)&0xFFFF;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].OffsetHigh=HandlerRoutine>>32;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].Selector=AsmReadCs();
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].IstIndex=0;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].Reserved0=0;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].Type=X64_GDT_INTERRUPT_GATE;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].DPL=0;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].Present=1;
	Host.ProcessorBlocks[CurProc].Core.Idt[Vector].Reserved1=0;
}

void __cdecl nv_dprintf(const char* format,...)
{
	CHAR8 TempBuffer[512]="[NoirVisor] ";
	CHAR8 FormatBuffer[320];
	UINTN Size,Start;
	va_list arg_list;
	for(UINTN i=0;i<sizeof(FormatBuffer);i++)
	{
		FormatBuffer[i]=format[i];
		if(format[i]=='\0')break;
		if(format[i]=='%' && format[i+1]=='s')FormatBuffer[++i]='a';
	}
	va_start(arg_list,format);
	Start=AsciiStrnLenS(TempBuffer,sizeof(TempBuffer));
	Size=AsciiVSPrint(&TempBuffer[Start],sizeof(TempBuffer)-Start,FormatBuffer,arg_list);
	va_end(arg_list);
	NoirSerialWrite(1,(UINT8*)TempBuffer,Start+Size);
	if(!NoirEfiInRuntimeStage)AsciiPrint(TempBuffer,Size);
}

void __cdecl nv_panicf(const char* format,...)
{
	CHAR8 TempBuffer[512]="[NoirVisor - Panic] ";
	CHAR8 FormatBuffer[320];
	UINTN Size,Start;
	va_list arg_list;
	for(UINTN i=0;i<sizeof(FormatBuffer);i++)
	{
		FormatBuffer[i]=format[i];
		if(format[i]=='%' && format[i+1]=='s')
		{
			FormatBuffer[i+1]='a';
			i+=2;
		}
		if(format[i]=='\0')break;
	}
	va_start(arg_list,format);
	Start=AsciiStrnLenS(TempBuffer,sizeof(TempBuffer));
	Size=AsciiVSPrint(&TempBuffer[Start],sizeof(TempBuffer)-Start,FormatBuffer,arg_list);
	va_end(arg_list);
	NoirSerialWrite(1,(UINT8*)TempBuffer,Start+Size);
	if(!NoirEfiInRuntimeStage)AsciiPrint(TempBuffer,Size);
}

void __cdecl NoirDebugPrint(IN CONST CHAR8 *Format,...)
{
	CPUID_VERSION_INFO_ECX Ecx;
	CHAR8 TempBuffer[320];
	CHAR8 FormatBuffer[320];
	UINTN Size,Start;
	va_list ArgList;
	for(UINTN i=0;i<sizeof(FormatBuffer);i++)
	{
		FormatBuffer[i]=Format[i];
		if(Format[i]=='%' && Format[i+1]=='s')
		{
			FormatBuffer[i+1]='a';
			i+=2;
		}
		if(Format[i]=='\0')break;
	}
	AsmCpuid(CPUID_VERSION_INFO,NULL,NULL,&Ecx.Uint32,NULL);
	AsciiSPrint(TempBuffer,sizeof(TempBuffer),"[NoirVisor - %a] ",Ecx.Bits.NotUsed?"Guest":"Host");
	va_start(ArgList,Format);
	Start=AsciiStrnLenS(TempBuffer,sizeof(TempBuffer));
	Size=AsciiVSPrint(&TempBuffer[Start],sizeof(TempBuffer)-Start,FormatBuffer,ArgList);
	va_end(ArgList);
	NoirSerialWrite(1,(UINT8*)TempBuffer,Start+Size);
	if(!NoirEfiInRuntimeStage)AsciiPrint(TempBuffer,Size);
}

void* noir_alloc_nonpg_memory(IN UINTN Length)
{
	return AllocateRuntimeZeroPool(Length);
}

void* noir_alloc_contd_memory(IN UINTN Length)
{
	void* p=AllocateRuntimePages(EFI_SIZE_TO_PAGES(Length));
	if(p)ZeroMem(p,Length);
	return p;
}

void* noir_alloc_2mb_page()
{
	void* p=AllocateAlignedRuntimePages(0x200,0x200000);
	if(p)ZeroMem(p,0x200000);
	return p;
}

void noir_free_nonpg_memory(IN VOID* VirtualAddress)
{
	FreePool(VirtualAddress);
}

void noir_free_contd_memory(IN VOID* VirtualAddress,IN UINTN Length)
{
	FreePages(VirtualAddress,EFI_SIZE_TO_PAGES(Length));
}

void noir_free_2mb_page(IN VOID* VirtualAddress)
{
	FreeAlignedPages(VirtualAddress,0x200);
}

UINT64 noir_get_physical_address(IN VOID* VirtualAddress)
{
	// Mapping is identical.
	return (UINT64)VirtualAddress;
}

VOID* noir_find_virt_by_phys(IN UINT64 PhysicalAddress)
{
	// Mapping is identical.
	return (VOID*)PhysicalAddress;
}

void noir_copy_memory(IN VOID* Destination,IN VOID* Source,IN UINT32 Length)
{
	CopyMem(Destination,Source,Length);
}

void static NoirGenericCallRT(IN VOID* ProcedureArgument)
{
	PNV_MP_SERVICE_GENERIC_INFO GenericInfo=(PNV_MP_SERVICE_GENERIC_INFO)ProcedureArgument;
	UINTN ProcId=0;
	if(MpServices)MpServices->WhoAmI(MpServices,&ProcId);
	GenericInfo->Worker(GenericInfo->Context,(UINT32)ProcId);
}

void noir_generic_call(noir_broadcast_worker worker,void* context)
{
	// Initialize Structure for Generic Call.
	NV_MP_SERVICE_GENERIC_INFO GenericInfo;
	GenericInfo.Worker=worker;
	GenericInfo.Context=context;
	if(MpServices)
	{
#if defined(_GPC)
		// We are going to do Generic Calls asynchronously.
		EFI_EVENT GenericCallEvent;
		EFI_STATUS st=gBS->CreateEvent(0,0,NULL,NULL,&GenericCallEvent);
		if(st==EFI_SUCCESS)
		{
			UINTN SignaledIndex;
			// Initiate Asynchronous & Simultaneous Generic Call.
			st=MpServices->StartupAllAPs(MpServices,NoirGenericCallRT,FALSE,GenericCallEvent,0,&GenericInfo,NULL);
			// BSP is not invoked during the Generic Call. Do it here.
			NoirGenericCallRT((VOID*)&GenericInfo);
			// BSP has finished its job for Generic Call. Wait for all APs.
			st=gBS->WaitForEvent(1,&GenericCallEvent,&SingaledIndex);
			gBS->CloseEvent(GenericCallEvent);
		}
#else
		// Do it in BSP before other APs.
		NoirGenericCallRT((VOID*)&GenericInfo);
		// Initiate Synchronous & Serialized Generic Call.
		MpServices->StartupAllAPs(MpServices,NoirGenericCallRT,TRUE,NULL,0,&GenericInfo,NULL);
#endif
	}
	else
	{
		// Only one core is accessible.
		NoirGenericCallRT((VOID*)&GenericInfo);
	}
}

UINT32 noir_get_current_processor()
{
	if(MpServices)
	{
		UINTN Num;
		EFI_STATUS st=MpServices->WhoAmI(MpServices,&Num);
		if(st==EFI_SUCCESS)return (UINT32)Num;
	}
	return 0;
}

UINT32 noir_get_processor_count()
{
	if(MpServices)
	{
		UINTN Num1,Num2;
		EFI_STATUS st=MpServices->GetNumberOfProcessors(MpServices,&Num1,&Num2);
		if(st==EFI_SUCCESS)return (UINT32)Num1;
	}
	return 1;
}

void noir_qsort(IN OUT VOID* Base,IN UINT32 Number,IN UINT32 Width,IN BASE_SORT_COMPARE CompareFunction)
{
	UINT8 Buffer[1024];		// 1024 bytes should be enough.
	if(Width<=sizeof(Buffer))QuickSort(Base,Number,Width,CompareFunction,Buffer);
}

UINT32 noir_get_instruction_length(IN VOID* code,IN BOOLEAN LongMode)
{
	if(LongMode)
		return NoirGetInstructionLength64(code,0);
	return NoirGetInstructionLength32(code,0);
}

UINT32 noir_get_instruction_length_ex(IN VOID* Code,IN UINT8 Bits)
{
	switch(Bits)
	{
	case 16:
		return NoirGetInstructionLength16(Code,0);
	case 32:
		return NoirGetInstructionLength32(Code,0);
	case 64:
		return NoirGetInstructionLength64(Code,0);
	default:
		return 0;
	}
}

UINT32 noir_disasm_instruction(IN VOID* Code,OUT CHAR8 *Mnemonic,IN UINTN MnemonicLength,IN UINT8 Bits,IN UINT64 VirtualAddress)
{
	switch(Bits)
	{
	case 16:
		return NoirDisasmCode16(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	case 32:
		return NoirDisasmCode32(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	case 64:
		return NoirDisasmCode64(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	default:
		return 0;
	}
}

UINT64 noir_get_system_time()
{
	// FIXME: Use HPET to get the time.
	// The return value has unit of 100ns.
	// Do not use Runtime Service by virtue of complex calendar calculation.
	return 0;
}

void NoirDisplayProcessorState()
{
	NOIR_PROCESSOR_STATE State;
	NoirSaveProcessorState(&State);
	Print(L"CS Segment | Selector=0x%04X | Attributes=0x%04X | Limit=0x%08X | Base=0x%p\n",State.Cs.Selector,State.Cs.Attributes,State.Cs.Limit,State.Cs.Base);
	Print(L"DS Segment | Selector=0x%04X | Attributes=0x%04X | Limit=0x%08X | Base=0x%p\n",State.Ds.Selector,State.Ds.Attributes,State.Ds.Limit,State.Ds.Base);
	Print(L"ES Segment | Selector=0x%04X | Attributes=0x%04X | Limit=0x%08X | Base=0x%p\n",State.Es.Selector,State.Es.Attributes,State.Es.Limit,State.Es.Base);
	Print(L"FS Segment | Selector=0x%04X | Attributes=0x%04X | Limit=0x%08X | Base=0x%p\n",State.Fs.Selector,State.Fs.Attributes,State.Fs.Limit,State.Fs.Base);
	Print(L"GS Segment | Selector=0x%04X | Attributes=0x%04X | Limit=0x%08X | Base=0x%p\n",State.Gs.Selector,State.Gs.Attributes,State.Gs.Limit,State.Gs.Base);
	Print(L"SS Segment | Selector=0x%04X | Attributes=0x%04X | Limit=0x%08X | Base=0x%p\n",State.Ss.Selector,State.Ss.Attributes,State.Ss.Limit,State.Ss.Base);
	Print(L"TR Segment | Selector=0x%04X | Attributes=0x%04X | Limit=0x%08X | Base=0x%p\n",State.Tr.Selector,State.Tr.Attributes,State.Tr.Limit,State.Tr.Base);
	Print(L"GDT Base=0x%p, Limit=0x%04X | IDT Base=0x%p, Limit=0x%04X\n",State.Gdtr.Base,State.Gdtr.Limit,State.Idtr.Base,State.Idtr.Limit);
	Print(L"CR0=0x%p | CR2=0x%p | CR3=0x%p | CR4=0x%p | CR8=0x%p\n",State.Cr0,State.Cr2,State.Cr3,State.Cr4,State.Cr8);
	Print(L"DR0=0x%p | DR1=0x%p | DR2=0x%p | DR3=0x%p | DR6=0x%p | DR7=0x%p\n",State.Dr0,State.Dr1,State.Dr2,State.Dr3,State.Dr6,State.Dr7);
	Print(L"Debug Control MSR=0x%p | PAT MSR=0x%p | EFER MSR=0x%p\n",State.DebugControl,State.Pat,State.Efer);
	Print(L"SysEnter_CS MSR=0x%p | SysEnter_ESP MSR=0x%p | SysEnter_EIP MSR=0x%p\n",State.SysEnter_Cs,State.SysEnter_Esp,State.SysEnter_Eip);
	Print(L"Star MSR=0x%p | LStar MSR=0x%p | CStar MSR=0x%p | SfMask=0x%p\n",State.Star,State.LStar,State.CStar,State.SfMask);
	Print(L"FS Base MSR=0x%p | GS Base MSR=0x%p | GS Swap Base MSR=0x%p\n",State.FsBase,State.GsBase,State.GsSwap);
}