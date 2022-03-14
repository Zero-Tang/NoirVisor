/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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
#include <intrin.h>
#include "debug.h"

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
	NoirDebugPrint("Breakpoint Trap occured!\n");
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
	NoirDebugPrint("Page Fault occured!\n");
	NoirDebugPrint("Faulting RIP=0x%p, Rsp=0x%p\n",ExceptionStack->ReturnRip,ExceptionStack->ReturnRsp);
	NoirDebugPrint("Faulting Address=0x%p, Error Code=0x%X\n",AsmReadCr2(),ExceptionStack->ErrorCode);
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