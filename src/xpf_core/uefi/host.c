/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file builds the host environment for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/host.c
*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>
#include <intrin.h>
#include "host.h"

void NoirFreeHostEnvironment()
{
	if(Host.ProcessorBlocks)
	{
		for(UINTN i=0;i<Host.NumberOfProcessors;i++)
		{
			FreePool(Host.ProcessorBlocks[i].Idt);
			FreePool(Host.ProcessorBlocks[i].Gdt);
			FreePool(Host.ProcessorBlocks[i].Tss);
		}
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

void NoirBuildInterruptDescriptorTable(OUT PNHIDTENTRY64 Idt)
{
	UINTN Handler=(UINTN)NoirUnexpectedInterruptHandler;
	for(UINT8 i=0;i<=255;i++)
	{
		// Set the address.
		Idt[i].OffsetLow=(UINT16)Handler;
		Idt[i].OffsetMid=(UINT16)((Handler>>16)&0xFFFF);
		Idt[i].OffsetHigh=(UINT32)(Handler>>32);
		// Miscellaneous.
		Idt[i].Selector=NH_X64_CS_SELECTOR;
		Idt[i].IstIndex=1;
		Idt[i].Type=NH_INTERRUPT_GATE_TYPE;
		Idt[i].Present=1;
	}
}

UINT16 NoirAllocateSegmentSelector(IN PNHGDTENTRY64 GdtBase,IN UINT16 Limit)
{
	for(UINT16 i=8;i<Limit;i+=8)
	{
		if(GdtBase[i>>3].Bits.Present==0)
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
	PNHPCR Pcr=(PNHPCR)ProcedureArgument;
	VOID* NewGdt=NULL;
	VOID* NewIdt=NULL;
	UINTN CurProc=0;
	AsmReadGdtr(&Pcr[CurProc].GdtR);
	AsmReadIdtr(&Pcr[CurProc].IdtR);
	NewGdt=AllocateRuntimePool(Pcr[CurProc].GdtR.Limit+1);
	NewIdt=AllocateRuntimePool(Pcr[CurProc].IdtR.Limit+1);
	if(MpServices)MpServices->WhoAmI(MpServices,&CurProc);
	Pcr[CurProc].ProcessorNumber=CurProc;
	if(NewGdt && NewIdt)
	{
		CopyMem(NewGdt,(VOID*)Pcr[CurProc].GdtR.Base,Pcr[CurProc].GdtR.Limit+1);
		CopyMem(NewIdt,(VOID*)Pcr[CurProc].IdtR.Base,Pcr[CurProc].IdtR.Limit+1);
	}
	else
	{
		if(!NewGdt)
			Print(L"Failed to allocate new GDT for Processor %d!\n",CurProc);
		else
			FreePool(NewGdt);
		if(!NewIdt)
			Print(L"Failed to allocate new IDT for Processor %d!\n",CurProc);
		else
			FreePool(NewIdt);
	}
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
		// If MP-Services is unavailable, only one logical processor is accessible.
		Host.NumberOfProcessors=EnabledProcessors=1;
		st=EFI_SUCCESS;
	}
	if(st==EFI_SUCCESS)
	{
		// Initialize Host System.
		Host.ProcessorBlocks=AllocateRuntimeZeroPool(Host.NumberOfProcessors*sizeof(NHPCR));
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
				Host.PdptBase[i].PageSize=1;
			}
			Print(L"NoirVisor starts initialization per processor...\n");
			// Initialize Per-Processor Blocks by broadcasting.
			NoirPrepareHostProcedureAp(Host.ProcessorBlocks);
			if(MpServices)MpServices->StartupAllAPs(MpServices,NoirPrepareHostProcedureAp,TRUE,NULL,0,Host.ProcessorBlocks,NULL);
			// If it reaches here, the initialization is successful.
			Print(L"NoirVisor Host Context is initialized successfully! Host System=0x%p\n",&Host);
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

void noir_load_host_processor_state()
{
	// Locate processor block.
	UINTN CurProc=0;
	if(MpServices)MpServices->WhoAmI(MpServices,&CurProc);
	// Load Descriptor Tables...
	AsmWriteGdtr(&Host.ProcessorBlocks[CurProc].GdtR);
	AsmWriteIdtr(&Host.ProcessorBlocks[CurProc].IdtR);
	// Load Paging Tables...
	AsmWriteCr3((UINTN)Host.Pml4Base);
}

void* noir_alloc_nonpg_memory(IN UINTN Length)
{
	void* p=AllocateRuntimePool(Length);
	if(p)ZeroMem(p,Length);
	return p;
}

void* noir_alloc_contd_memory(IN UINTN Length)
{
	void* p=AllocateRuntimePages(EFI_SIZE_TO_PAGES(Length));
	if(p)ZeroMem(p,Length);
	return p;
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
		EFI_STATUS st=gBS->CreateEvent(EVT_NOTIFY_WAIT,TPL_APPLICATION,NULL,NULL,&GenericCallEvent);
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

void NoirGetSegmentAttributes(IN UINTN GdtBase,IN UINT16 Selector,OUT PSEGMENT_REGISTER Segment)
{
	PNHGDTENTRY64 GdtEntry=(PNHGDTENTRY64)(GdtBase+(Selector&GDT_SELECTOR_MASK));
	Segment->Attributes=GdtEntry->Bytes.Flags;
	Segment->Limit=(UINT32)(GdtEntry->Bits.LimitLo|(GdtEntry->Bits.LimitHi<<16))+1;
	if(GdtEntry->Bits.Granularity)Segment->Limit<<=EFI_PAGE_SHIFT;
	Segment->Limit-=1;
	Segment->Base=(UINT64)((GdtEntry->Bytes.BaseHi<<24)|(GdtEntry->Bytes.BaseMid<<16)|GdtEntry->Bytes.BaseLo);
	Segment->Base|=((UINT64)(*(UINT32*)(GdtBase+8))<<32);
}

void noir_save_processor_state(OUT PNOIR_PROCESSOR_STATE State)
{
	IA32_DESCRIPTOR Gdtr,Idtr;
	// Save IDTR/GDTR...
	AsmReadGdtr(&Gdtr);
	AsmReadIdtr(&Idtr);
	State->Gdtr.Limit=Gdtr.Limit;
	State->Gdtr.Base=Gdtr.Base;
	State->Idtr.Limit=Idtr.Limit;
	State->Idtr.Base=Idtr.Base;
	// Save Segment Selectors...
	State->Cs.Selector=AsmReadCs();
	State->Ds.Selector=AsmReadDs();
	State->Es.Selector=AsmReadEs();
	State->Fs.Selector=AsmReadFs();
	State->Gs.Selector=AsmReadGs();
	State->Ss.Selector=AsmReadSs();
	State->Tr.Selector=AsmReadTr();
	// Save Segment Attributes...
	NoirGetSegmentAttributes(Gdtr.Base,State->Cs.Selector,&State->Cs);
	NoirGetSegmentAttributes(Gdtr.Base,State->Ds.Selector,&State->Ds);
	NoirGetSegmentAttributes(Gdtr.Base,State->Es.Selector,&State->Es);
	NoirGetSegmentAttributes(Gdtr.Base,State->Fs.Selector,&State->Fs);
	NoirGetSegmentAttributes(Gdtr.Base,State->Gs.Selector,&State->Gs);
	NoirGetSegmentAttributes(Gdtr.Base,State->Ss.Selector,&State->Ss);
	NoirGetSegmentAttributes(Gdtr.Base,State->Tr.Selector,&State->Tr);
	State->Cs.Base=State->Ds.Base=State->Es.Base=State->Ss.Base=0;
	// Save Control Registers...
	State->Cr0=AsmReadCr0();
	State->Cr2=AsmReadCr2();
	State->Cr3=AsmReadCr3();
	State->Cr4=AsmReadCr4();
	// Save Debug Registers...
	State->Dr0=AsmReadDr0();
	State->Dr1=AsmReadDr1();
	State->Dr2=AsmReadDr2();
	State->Dr3=AsmReadDr3();
	State->Dr6=AsmReadDr6();
	State->Dr7=AsmReadDr7();
	// Save Model Specific Registers...
	State->SysEnter_Cs=AsmReadMsr64(MSR_SYSENTER_CS);
	State->SysEnter_Esp=AsmReadMsr64(MSR_SYSENTER_ESP);
	State->SysEnter_Eip=AsmReadMsr64(MSR_SYSENTER_EIP);
	State->DebugControl=AsmReadMsr64(MSR_DEBUG_CONTROL);
	State->Pat=AsmReadMsr64(MSR_PAT);
	State->Efer=AsmReadMsr64(MSR_EFER);
	State->Star=AsmReadMsr64(MSR_STAR);
	State->LStar=AsmReadMsr64(MSR_LSTAR);
	State->CStar=AsmReadMsr64(MSR_CSTAR);
	State->SfMask=AsmReadMsr64(MSR_SFMASK);
	State->FsBase=AsmReadMsr64(MSR_FSBASE);
	State->GsBase=AsmReadMsr64(MSR_GSBASE);
	State->GsSwap=AsmReadMsr64(MSR_GSSWAP);
	// Save Segment Bases...
	State->Fs.Base=State->FsBase;
	State->Gs.Base=State->GsBase;
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