/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file builds the debug agent for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/debug.c
*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include "debug.h"

void NoirPrintGprState(IN X64_GPR_STATE *State)
{
	NoirDebugPrint("rax=0x%016X rcx=0x%016X rdx=0x%016X rbx=0x%016X\n",State->Rax,State->Rcx,State->Rdx,State->Rbx);
	NoirDebugPrint("rsp=0x%016X rbp=0x%016X rsi=0x%016X rdi=0x%016X\n",State->Rsp,State->Rbp,State->Rsi,State->Rdi);
	NoirDebugPrint("r8 =0x%016X r9 =0x%016X r10=0x%016X r11=0x%016X\n",State->R8,State->R9,State->R10,State->R11);
	NoirDebugPrint("r12=0x%016X r13=0x%016X r14=0x%016X r15=0x%016X\n",State->R12,State->R13,State->R14,State->R15);
}

void NoirDivideErrorFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Divide Error Fault occured!\n");
	CpuDeadLoop();
}

void NoirDebugFaultTrapHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Debug Fault/Trap occured!\n");
	CpuDeadLoop();
}

void NoirBreakpointTrapHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Breakpoint Trap occured! Rip=0x%p\n",ExceptionStack->ReturnRip);
	CpuDeadLoop();
}

void NoirOverflowTrapHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Overflow Trap occured!\n");
	CpuDeadLoop();
}

void NoirBoundRangeFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Bound-Range Fault occured!\n");
	CpuDeadLoop();
}

void NoirInvalidOpcodeFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Invalid Opcode Fault occured!\n");
	NoirDebugPrint("Faulting RIP=0x%p, Rsp=0x%p\n",ExceptionStack->ReturnRip,ExceptionStack->ReturnRsp);
	CpuDeadLoop();
}

void NoirUnavailableDeviceFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Device Not Available Fault occured!\n");
	CpuDeadLoop();
}

void NoirDoubleFaultAbortHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITH_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Double-Fault Abort occured!\n");
	CpuDeadLoop();
}

void NoirInvalidTssFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITH_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Invalid TSS Fault occured!\n");
	CpuDeadLoop();
}

void NoirAbsentSegmentFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITH_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Segment Not Present Fault occured!\n");
	CpuDeadLoop();
}

void NoirStackFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITH_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Stack Fault occured!\n");
	CpuDeadLoop();
}

void NoirGeneralProtectionFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITH_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("General Protection Fault occured!\n");
	NoirDebugPrint("Faulting RIP=0x%p, Rsp=0x%p, Error Code: 0x%X\n",ExceptionStack->ReturnRip,ExceptionStack->ReturnRsp,ExceptionStack->ErrorCode);
	CpuDeadLoop();
}

void NoirPageFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITH_ERROR_CODE *ExceptionStack)
{
	CHAR8 Mnemonic[64];
	NoirDebugPrint("Page Fault occured!\n");
	NoirDebugPrint("Faulting RIP=0x%p, Rsp=0x%p\n",ExceptionStack->ReturnRip,ExceptionStack->ReturnRsp);
	NoirDebugPrint("Faulting Address=0x%p, Error Code=0x%X\n",AsmReadCr2(),ExceptionStack->ErrorCode);
	NoirPrintGprState(State);
	NoirDisasmCode64(Mnemonic,sizeof(Mnemonic),(UINT8*)ExceptionStack->ReturnRip,15,ExceptionStack->ReturnRip);
	NoirDebugPrint("Faulting Instruction: %a\n",Mnemonic);
	CpuDeadLoop();
}

void NoirFloatingPointFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Floating Point Fault occured!\n");
	CpuDeadLoop();
}

void NoirAlignmentCheckFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITH_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Alignment Check Fault occured!\n");
	CpuDeadLoop();
}

void NoirMachineCheckAbortHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Machine Check Abort occured!\n");
	CpuDeadLoop();
}

void NoirSimdFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("SIMD Fault occured!\n");
	CpuDeadLoop();
}

void NoirControlProtectionFaultHandler(IN OUT X64_GPR_STATE *State,IN OUT X64_EXCEPTION_STACK_WITHOUT_ERROR_CODE *ExceptionStack)
{
	NoirDebugPrint("Control Protection Fault occured!\n");
	CpuDeadLoop();
}

void NoirSetupDebugSupportPerProcessor(IN VOID *ProcedureArgument)
{
	NoirSetHostInterruptHandler(EXCEPT_X64_DIVIDE_ERROR,(UINTN)AsmDivideErrorFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_DEBUG,(UINTN)AsmDebugFaultTrapHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_BREAKPOINT,(UINTN)AsmBreakpointTrapHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_OVERFLOW,(UINTN)AsmOverflowTrapHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_BOUND,(UINTN)AsmBoundRangeFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_INVALID_OPCODE,(UINTN)AsmInvalidOpcodeFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_DOUBLE_FAULT,(UINTN)AsmDoubleFaultAbortHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_INVALID_TSS,(UINTN)AsmInvalidTssFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_SEG_NOT_PRESENT,(UINTN)AsmAbsentSegmentFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_STACK_FAULT,(UINTN)AsmStackFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_GP_FAULT,(UINTN)AsmGeneralProtectionFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_PAGE_FAULT,(UINTN)AsmPageFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_FP_ERROR,(UINTN)AsmFloatingPointFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_ALIGNMENT_CHECK,(UINTN)AsmAlignmentCheckFaultHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_MACHINE_CHECK,(UINTN)AsmMachineCheckAbortHandler);
	NoirSetHostInterruptHandler(EXCEPT_X64_SIMD,(UINTN)AsmSimdFaultHandler);
}

EFI_STATUS NoirSetupDebugSupport()
{
	EFI_STATUS st=EFI_SUCCESS;
	if(MpServices)
	{
		EFI_EVENT MpEvent;
		st=gBS->CreateEvent(0,0,NULL,NULL,&MpEvent);
		if(st==EFI_SUCCESS)
		{
			UINTN SignalIndex;
			st=MpServices->StartupAllAPs(MpServices,NoirSetupDebugSupportPerProcessor,FALSE,MpEvent,0,NULL,NULL);
			NoirSetupDebugSupportPerProcessor(NULL);
			gBS->WaitForEvent(1,&MpEvent,&SignalIndex);
			gBS->CloseEvent(MpEvent);
		}
	}
	else
		NoirSetupDebugSupportPerProcessor(NULL);
	return st;
}