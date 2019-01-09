/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the processor facility of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/processor.c
*/

#include <ntddk.h>
#include <windef.h>
#include "processor.h"

USHORT static NoirGetSegmentAttributes(IN ULONG_PTR GdtBase,IN USHORT Selector)
{
	PKGDTENTRY GdtEntry=(PKGDTENTRY)((ULONG_PTR)GdtBase+(Selector&RPLTI_MASK));
	USHORT ar=0;
	ar=(GdtEntry->Flag1.Value|(GdtEntry->Flag2.Value<<8))&0xF0FF;
	return ar;
}

void noir_save_processor_state(OUT PNOIR_PROCESSOR_STATE State)
{
	KPROCESSOR_STATE kState;
	//Save processor state via System API.
	//This reduces assembly code.
	KeSaveStateForHibernate(&kState);
	//Initialize State.
	RtlZeroMemory(State,sizeof(NOIR_PROCESSOR_STATE));
	//Save Control Register State.
	State->Cr0=kState.SpecialRegisters.Cr0;
	State->Cr2=kState.SpecialRegisters.Cr2;
	State->Cr3=kState.SpecialRegisters.Cr3;
	State->Cr4=kState.SpecialRegisters.Cr4;
#if defined(_WIN64)
	State->Cr8=kState.SpecialRegisters.Cr8;
#endif
	//Save Debug Register State.
	State->Dr0=kState.SpecialRegisters.KernelDr0;
	State->Dr1=kState.SpecialRegisters.KernelDr1;
	State->Dr2=kState.SpecialRegisters.KernelDr2;
	State->Dr3=kState.SpecialRegisters.KernelDr3;
	State->Dr6=kState.SpecialRegisters.KernelDr6;
	State->Dr7=kState.SpecialRegisters.KernelDr7;
	//Save Model Specific Register State.
	State->Star=__readmsr(MSR_STAR);
	State->LStar=__readmsr(MSR_LSTAR);
	State->CStar=__readmsr(MSR_CSTAR);
	State->SfMask=__readmsr(MSR_SYSCALL_MASK);
	State->GsBase=__readmsr(MSR_GSBASE);
	State->GsSwap=__readmsr(MSR_KERNEL_GSBASE);
	State->SysEnterCs=__readmsr(MSR_SYSENTER_CS);
	State->SysEnterEsp=__readmsr(MSR_SYSENTER_ESP);
	State->SysEnterEip=__readmsr(MSR_SYSENTER_EIP);
	State->DebugControl=__readmsr(MSR_DBGCTRL);
	State->Pat=__readmsr(MSR_PAT);
	State->Efer=__readmsr(MSR_EFER);
	State->FsBase=__readmsr(MSR_FSBASE);
	//Save Global Descriptor Table.
	State->Gdtr.Limit=kState.SpecialRegisters.Gdtr.Limit;
	State->Gdtr.Base.QuadPart=kState.SpecialRegisters.Gdtr.Base;
	//Save Interrupt Descriptor Table.
	State->Idtr.Limit=kState.SpecialRegisters.Idtr.Limit;
	State->Idtr.Base.QuadPart=kState.SpecialRegisters.Idtr.Base;
	//Save Segment Registers - Cs.
	State->Cs.Selector=kState.ContextFrame.SegCs;
	State->Cs.Attributes=NoirGetSegmentAttributes(kState.SpecialRegisters.Gdtr.Base,State->Cs.Selector);
	State->Cs.Limit=__segmentlimit(State->Cs.Selector);
	State->Cs.Base.QuadPart=0;
	//Save Segment Registers - Ds.
	State->Ds.Selector=kState.ContextFrame.SegDs;
	State->Ds.Attributes=NoirGetSegmentAttributes(kState.SpecialRegisters.Gdtr.Base,State->Ds.Selector);
	State->Ds.Limit=__segmentlimit(State->Ds.Selector);
	State->Ds.Base.QuadPart=0;
	//Save Segment Registers - Es.
	State->Es.Selector=kState.ContextFrame.SegEs;
	State->Es.Attributes=NoirGetSegmentAttributes(kState.SpecialRegisters.Gdtr.Base,State->Es.Selector);
	State->Es.Limit=__segmentlimit(State->Es.Selector);
	State->Es.Base.QuadPart=0;
	//Save Segment Registers - Fs.
	State->Fs.Selector=kState.ContextFrame.SegFs;
	State->Fs.Attributes=NoirGetSegmentAttributes(kState.SpecialRegisters.Gdtr.Base,State->Fs.Selector);
	State->Fs.Limit=__segmentlimit(State->Fs.Selector);
	State->Fs.Base.QuadPart=State->FsBase;
	//Save Segment Registers - Gs.
	State->Gs.Selector=kState.ContextFrame.SegGs;
	State->Gs.Attributes=NoirGetSegmentAttributes(kState.SpecialRegisters.Gdtr.Base,State->Gs.Selector);
	State->Gs.Limit=__segmentlimit(State->Gs.Selector);
	State->Gs.Base.QuadPart=State->GsBase;
	//Save Segment Registers - Ss.
	State->Ss.Selector=kState.ContextFrame.SegSs;
	State->Ss.Attributes=NoirGetSegmentAttributes(kState.SpecialRegisters.Gdtr.Base,State->Ss.Selector);
	State->Ss.Limit=__segmentlimit(State->Ss.Selector);
	State->Ss.Base.QuadPart=0;
	//Save Segment Registers - Tr.
	State->Tr.Selector=kState.SpecialRegisters.Tr;
	State->Tr.Attributes=NoirGetSegmentAttributes(kState.SpecialRegisters.Gdtr.Base,State->Tr.Selector);
	State->Tr.Limit=__segmentlimit(State->Tr.Selector);
	//The Base Address of Task Register is somewhat special.
	//It is located at KPCR.TssBase, which can be read via Fs or Gs Segment.
#if defined(_WIN64)
	State->Tr.Base.QuadPart=__readgsqword(0x8);
#else
	State->Tr.Base.LowPart=__readfsdword(0x40);
#endif
}

ULONG32 noir_get_processor_count()
{
	KAFFINITY af;
	return KeQueryActiveProcessorCount(&af);
}

ULONG32 noir_get_current_processor()
{
	return KeGetCurrentProcessorNumber();
}

void static NoirDpcRT(IN PKDPC Dpc,IN PVOID DeferedContext OPTIONAL,IN PVOID SystemArgument1 OPTIONAL,IN PVOID SystemArgument2 OPTIONAL)
{
	noir_broadcast_worker worker=(noir_broadcast_worker)SystemArgument1;
	PLONG32 volatile GlobalOperatingNumber=(PLONG32)SystemArgument2;
	ULONG Pn=KeGetCurrentProcessorNumber();
	worker(DeferedContext,Pn);
	InterlockedDecrement(GlobalOperatingNumber);
}

void noir_generic_call(noir_broadcast_worker worker,void* context)
{
	ULONG32 Num=noir_get_processor_count();
	PVOID IpbBuffer=ExAllocatePool(NonPagedPool,Num*sizeof(KDPC)+4);
	if(IpbBuffer)
	{
		PLONG32 volatile GlobalOperatingNumber=(PULONG32)((ULONG_PTR)IpbBuffer+Num*sizeof(KDPC));
		PKDPC pDpc=(PKDPC)IpbBuffer;
		ULONG i=0;
		*GlobalOperatingNumber=Num;
		for(;i<Num;i++)
		{
			KeInitializeDpc(&pDpc[i],NoirDpcRT,context);
			KeSetTargetProcessorDpc(&pDpc[i],(BYTE)i);
			KeSetImportanceDpc(&pDpc[i],HighImportance);
			KeInsertQueueDpc(&pDpc[i],(PVOID)worker,(PVOID)GlobalOperatingNumber);
		}
		while(*GlobalOperatingNumber)__nop();
		ExFreePool(IpbBuffer);
	}
}