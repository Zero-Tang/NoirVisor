/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

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
#include "host.h"

void NoirFreeHostEnvironment()
{
	if(Host.ProcessorBlocks)
	{
		for(UINTN i=0;i<Host.NumberOfProcessors;i++)
			FreePages(Host.ProcessorBlocks[i].DescriptorTables,1);
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

EFI_STATUS NoirBuildHostProcessor(OUT PNHPCR Processor)
{
	EFI_STATUS st=EFI_OUT_OF_RESOURCES;
	Processor->DescriptorTables=AllocateRuntimePages(1);
	if(Processor->DescriptorTables)
	{
		ZeroMem(Processor->DescriptorTables,EFI_PAGE_SIZE);
		// Initialize Structures of Necessary Pointers.
		Processor->Self=Processor;
		Processor->Gdt=(PNHGDTENTRY64)Processor->DescriptorTables;
		Processor->Idt=(PNHIDTENTRY64)((UINTN)Processor->DescriptorTables+0x400);
		Processor->Tss=(PNHTSS64)((UINTN)Processor->DescriptorTables+0x100);
		// Initialize Host Context - CS.
		Processor->ProcessorState.Cs.Selector=NH_X64_CS_SELECTOR;
		Processor->ProcessorState.Cs.Attributes=NH_X64_CODE_ATTRIBUTES;
		Processor->ProcessorState.Cs.Limit=0xFFFFFFFF;
		Processor->ProcessorState.Cs.Base=0;
		// Initialize Host Context - DS
		Processor->ProcessorState.Ds.Selector=NH_X64_DS_SELECTOR;
		Processor->ProcessorState.Ds.Attributes=NH_X64_DATA_ATTRIBUTES;
		Processor->ProcessorState.Ds.Limit=0xFFFFFFFF;
		Processor->ProcessorState.Ds.Base=0;
		// Initialize Host Context - ES
		Processor->ProcessorState.Es.Selector=NH_X64_ES_SELECTOR;
		Processor->ProcessorState.Es.Attributes=NH_X64_DATA_ATTRIBUTES;
		Processor->ProcessorState.Es.Limit=0xFFFFFFFF;
		Processor->ProcessorState.Es.Base=0;
		// Initialize Host Context - FS
		Processor->ProcessorState.Fs.Selector=NH_X64_FS_SELECTOR;
		Processor->ProcessorState.Fs.Attributes=NH_X64_DATA_ATTRIBUTES;
		Processor->ProcessorState.Fs.Limit=0xFFFFFFFF;
		Processor->ProcessorState.Fs.Base=0;
		// Initialize Host Context - GS
		Processor->ProcessorState.Gs.Selector=NH_X64_GS_SELECTOR;
		Processor->ProcessorState.Gs.Attributes=NH_X64_DATA_ATTRIBUTES;
		Processor->ProcessorState.Gs.Limit=0xFFFFFFFF;
		Processor->ProcessorState.Gs.Base=0;
		// Initialize Host Context - SS
		Processor->ProcessorState.Ss.Selector=NH_X64_SS_SELECTOR;
		Processor->ProcessorState.Ss.Attributes=NH_X64_DATA_ATTRIBUTES;
		Processor->ProcessorState.Ss.Limit=0xFFFFFFFF;
		Processor->ProcessorState.Ss.Base=0;
		// Initialize Host Context - TR
		Processor->ProcessorState.Tr.Selector=NH_X64_TR_SELECTOR;
		Processor->ProcessorState.Tr.Attributes=NH_X64_TASK_ATTRIBUTES;
		Processor->ProcessorState.Tr.Limit=sizeof(NHTSS64);
		Processor->ProcessorState.Tr.Base=(UINT64)Processor->Tss;
		// Initialize Host Context - GDTR
		Processor->ProcessorState.Gdtr.Limit=0x50;
		Processor->ProcessorState.Gdtr.Base=(UINT64)Processor->Gdt;
		// Initialize Host Context - IDTR
		Processor->ProcessorState.Idtr.Limit=0xFFF;
		Processor->ProcessorState.Idtr.Base=(UINT64)Processor->Idt;
		// Initialize Global Descriptor Table
		PNHGDTENTRY64 CodeEntry=(PNHGDTENTRY64)((UINTN)Processor->Gdt+NH_X64_CS_SELECTOR);
		CodeEntry->Bits.LimitLo=0xFFFF;
		CodeEntry->Bytes.Flags=NH_X64_CODE_ATTRIBUTES;
		CodeEntry->Bits.LimitHi=0xF;
		PNHGDTENTRY64 DataEntry=(PNHGDTENTRY64)((UINTN)Processor->Gdt+NH_X64_DS_SELECTOR);
		DataEntry->Bits.LimitLo=0xFFFF;
		DataEntry->Bytes.Flags=NH_X64_DATA_ATTRIBUTES;
		DataEntry->Bits.LimitHi=0xF;
		PNHGDTENTRY64 TaskEntry=(PNHGDTENTRY64)((UINTN)Processor->Gdt+NH_X64_TR_SELECTOR);
		TaskEntry->Bits.LimitLo=sizeof(NHTSS64);
		TaskEntry->Bytes.Flags=NH_X64_TASK_ATTRIBUTES;
		/*
		  Even though we try not to generate any exceptions in NoirVisor, we
		  should set up IDT in order to process NMI and (maybe) we'll throw
		  the NMI right to the Guest.
		  Since we require IDT, TSS is required to be setup correspondingly,
		  in that stack will be switched according to the setup of TSS.
		*/
		// Initialize Control Registers...
		Processor->ProcessorState.Cr0=0x80000033;	// PE MP ET NE PG
		Processor->ProcessorState.Cr3=(UINTN)Host.Pml4Base;
		Processor->ProcessorState.Cr4=0x406F8;		// DE PSE PAE MCE PGE OSFXSR OSXMMEXCEPT OSXSAVE
		// No debug-register initializations...
		st=EFI_SUCCESS;
	}
	return st;
}

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
			// Initialize Per-Processor Blocks.
			for(UINTN i=0;i<Host.NumberOfProcessors;i++)
			{
				st=NoirBuildHostProcessor(&Host.ProcessorBlocks[i]);
				Host.ProcessorBlocks[i].ProcessorNumber=i;
				if(st!=EFI_SUCCESS)goto AllocFailure;
			}
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
			// If it reaches here, the initialization is successful.
			Print(L"NoirVisor Host Context is initialized successfully! Host System=0x%p\n",&Host);
			st=EFI_SUCCESS;
		}
		else
		{
AllocFailure:
			NoirFreeHostEnvironment();
			st=EFI_OUT_OF_RESOURCES;
		}
	}
	return st;
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