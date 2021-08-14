/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

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

EFI_STATUS NoirLocateDebugSupportProtocol()
{
	EFI_HANDLE *HandleList=NULL;
	UINTN HandleCount=0;
	EFI_STATUS st=gBS->LocateHandleBuffer(ByProtocol,&gEfiDebugSupportProtocolGuid,NULL,&HandleCount,&HandleList);
	if(st==EFI_SUCCESS)
	{
		EFI_DEBUG_SUPPORT_PROTOCOL *Protocol=NULL;
		for(UINTN i=0;i<HandleCount;i++)
		{
			st=gBS->HandleProtocol(HandleList[i],&gEfiDebugSupportProtocolGuid,(VOID**)&Protocol);
			if(st==EFI_SUCCESS)
			{
#if defined(MDE_CPU_X64)
				// There might be EBC debugger support as well as x64.
				if(Protocol->Isa==IsaX64)
				{
					DebugSupport=Protocol;
					break;
				}
#endif
			}
		}
		if(DebugSupport==NULL)st=EFI_NOT_FOUND;
		FreePool(HandleList);
	}
	return st;
}

#if defined(MDE_CPU_X64)
void static NoirX64PrintProcessorState(IN EFI_SYSTEM_CONTEXT_X64* Context)
{
	// Determine the column size to optimize for number of rows.
	UINTN MaxCol,MaxRow;
	StdOut->QueryMode(StdOut,StdOut->Mode->Mode,&MaxCol,&MaxRow);
	if(MaxCol>=70 && MaxCol<95)			// Column size allows at lease 3 GPRs to be displayed. 13 rows will be printed. (e.g: 80x25)
	{
		Print(L"rax=0x%016X rcx=0x%016X rdx=0x%016X\n",Context->Rax,Context->Rcx,Context->Rdx);
		Print(L"rbx=0x%016X rsp=0x%016X rbp=0x%016X\n",Context->Rbx,Context->Rsp,Context->Rbp);
		Print(L"rsi=0x%016X rdi=0x%016X r8 =0x%016X\n",Context->Rsi,Context->Rdi,Context->R8);
		Print(L"r9 =0x%016X r10=0x%016X r11=0x%016X\n",Context->R9,Context->R10,Context->R11);
		Print(L"r12=0x%016X r13=0x%016X r14=0x%016X\n",Context->R12,Context->R13,Context->R14);
		Print(L"r15=0x%016X rfl=0x%016X rip=0x%016X\n",Context->R15,Context->Rflags,Context->Rip);
		Print(L"es=0x%04X cs=0x%04X ss=0x%04X ds=0x%04X fs=0x%04X gs=0x%04X\n",Context->Es,Context->Cs,Context->Ss,Context->Ds,Context->Fs,Context->Gs);
		Print(L"GDTR.Limit=0x%04X GDTR.Base=0x%016X LDTR=0x%04X\n",Context->Gdtr[0],Context->Gdtr[1],Context->Ldtr);
		Print(L"IDTR.Limit=0x%04X IDTR.Base=0x%016X   TR=0x%04X\n",Context->Idtr[0],Context->Idtr[1],Context->Tr);
		Print(L"cr0=0x%016X cr2=0x%016X cr3=0x%016X\n",Context->Cr0,Context->Cr2,Context->Cr3);
		Print(L"cr4=0x%016X cr8=0x%016X\n",Context->Cr4,Context->Cr8);
		Print(L"dr0=0x%016X dr1=0x%016X dr2=0x%016X\n",Context->Dr0,Context->Dr1,Context->Dr2);
		Print(L"dr3=0x%016X dr6=0x%016X dr7=0x%016X\n",Context->Dr3,Context->Dr6,Context->Dr7);
	}
	else if(MaxCol>=95 && MaxCol<115)	// Column size allows at least 4 GPRs to be displayed. 11 rows will be printed. (e.g: 100x31)
	{
		Print(L"rax=0x%016X rcx=0x%016X rdx=0x%016X rbx=0x%016X\n",Context->Rax,Context->Rcx,Context->Rdx,Context->Rbx);
		Print(L"rsp=0x%016X rbp=0x%016X rsi=0x%016X rdi=0x%016X\n",Context->Rsp,Context->Rbp,Context->Rsi,Context->Rdi);
		Print(L"r8 =0x%016X r9 =0x%016X r10=0x%016X r11=0x%016X\n",Context->R8,Context->R9,Context->R10,Context->R11);
		Print(L"r12=0x%016X r13=0x%016X r14=0x%016X r15=0x%016X\n",Context->R12,Context->R13,Context->R14,Context->R15);
		Print(L"rip=0x%016X rflags=0x%016X\n",Context->Rip,Context->Rflags);
		Print(L"es=0x%04X cs=0x%04X ss=0x%04X ds=0x%04X fs=0x%04X gs=0x%04X ldtr=0x%04X tr=0x%04X\n",Context->Es,Context->Cs,Context->Ss,Context->Ds,Context->Fs,Context->Gs,Context->Ldtr,Context->Tr);
		Print(L"GDTR.Limit=0x%04X GDTR.Base=0x%016X IDTR.Limit=0x%04X IDTR.Base=0x%016X\n",Context->Gdtr[0],Context->Gdtr[1],Context->Idtr[0],Context->Idtr[1]);
		Print(L"cr0=0x%016X cr2=0x%016X cr3=0x%016X\n",Context->Cr0,Context->Cr2,Context->Cr3);
		Print(L"cr4=0x%016X cr8=0x%016X\n",Context->Cr4,Context->Cr8);
		Print(L"dr0=0x%016X dr1=0x%016X dr2=0x%016X\n",Context->Dr0,Context->Dr1,Context->Dr2);
		Print(L"dr3=0x%016X dr6=0x%016X dr7=0x%016X\n",Context->Dr3,Context->Dr6,Context->Dr7);
	}
	else if(MaxCol>=115 && MaxCol<140)	// Column size allows at least 5 GPRs to be displayed.  9 rows will be printed. (e.g: 125x40)
	{
		Print(L"rax=0x%016X rcx=0x%016X rdx=0x%016X rbx=0x%016X rsp=0x%016X\n",Context->Rax,Context->Rcx,Context->Rdx,Context->Rbx,Context->Rsp);
		Print(L"rbp=0x%016X rsi=0x%016X rdi=0x%016X r8 =0x%016X r9 =0x%016X\n",Context->Rbp,Context->Rsi,Context->Rdi,Context->R8,Context->R9);
		Print(L"r10=0x%016X r11=0x%016X r12=0x%016X r13=0x%016X r14=0x%016X\n",Context->R10,Context->R11,Context->R12,Context->R13,Context->R14);
		Print(L"r15=0x%016X rip=0x%016X rflags=0x%016X\n",Context->R15,Context->Rip,Context->Rflags);
		Print(L"es=0x%04X cs=0x%04X ss=0x%04X ds=0x%04X fs=0x%04X gs=0x%04X ldtr=0x%04X tr=0x%04X\n",Context->Es,Context->Cs,Context->Ss,Context->Ds,Context->Fs,Context->Gs,Context->Ldtr,Context->Tr);
		Print(L"GDTR.Limit=0x%04X GDTR.Base=0x%016X IDTR.Limit=0x%04X IDTR.Base=0x%016X\n",Context->Gdtr[0],Context->Gdtr[1],Context->Idtr[0],Context->Idtr[1]);
		Print(L"cr0=0x%016X cr2=0x%016X cr3=0x%016X cr4=0x%016X cr8=0x%016X\n",Context->Cr0,Context->Cr2,Context->Cr3,Context->Cr4,Context->Cr8);
		Print(L"dr0=0x%016X dr1=0x%016X dr2=0x%016X\n",Context->Dr0,Context->Dr1,Context->Dr2);
		Print(L"dr3=0x%016X dr6=0x%016X dr7=0x%016X\n",Context->Dr3,Context->Dr6,Context->Dr7);
	}
	else if(MaxCol>=140 && MaxCol<185)	// Column size allows at least 6 GPRs to be displayed.  7 rows will be printed.
	{
		Print(L"rax=0x%016X rcx=0x%016X rdx=0x%016X rbx=0x%016X rsp=0x%016X rbp=0x%016X\n",Context->Rax,Context->Rcx,Context->Rdx,Context->Rbx,Context->Rsp,Context->Rbp);
		Print(L"rsi=0x%016X rdi=0x%016X r8 =0x%016X r9 =0x%016X r10=0x%016X r11=0x%016X\n",Context->Rsi,Context->Rdi,Context->R8,Context->R9,Context->R10,Context->R11);
		Print(L"r12=0x%016X r13=0x%016X r14=0x%016X r15=0x%016X rfl=0x%016X rip=0x%016X\n",Context->R12,Context->R13,Context->R14,Context->R15,Context->Rflags,Context->Rip);
		Print(L"es=0x%04X cs=0x%04X ss=0x%04X ds=0x%04X fs=0x%04X gs=0x%04X ldtr=0x%04X tr=0x%04X\n",Context->Es,Context->Cs,Context->Ss,Context->Ds,Context->Fs,Context->Gs,Context->Ldtr,Context->Tr);
		Print(L"GDTR.Limit=0x%04X GDTR.Base=0x%016X IDTR.Limit=0x%04X IDTR.Base=0x%016X\n",Context->Gdtr[0],Context->Gdtr[1],Context->Idtr[0],Context->Idtr[1]);
		Print(L"cr0=0x%016X cr2=0x%016X cr3=0x%016X cr4=0x%016X cr8=0x%016X\n",Context->Cr0,Context->Cr2,Context->Cr3,Context->Cr4,Context->Cr8);
		Print(L"dr0=0x%016X dr1=0x%016X dr2=0x%016X dr3=0x%016X dr6=0x%016X dr7=0x%016X\n",Context->Dr0,Context->Dr1,Context->Dr2);
	}
	else if(MaxCol>=185 && MaxCol<210)	// Column size allows at least 8 GPRs to be displayed.  6 rows will be printed.
	{
		Print(L"rax=0x%016X rcx=0x%016X rdx=0x%016X rbx=0x%016X rsp=0x%016X rbp=0x%016X rsi=0x%016X rdi=0x%016X\n",Context->Rax,Context->Rcx,Context->Rdx,Context->Rbx,Context->Rsp,Context->Rbp,Context->Rsi,Context->Rdi);
		Print(L"r8 =0x%016X r9 =0x%016X r10=0x%016X r11=0x%016X r12=0x%016X r13=0x%016X r14=0x%016X r15=0x%016X\n",Context->R8,Context->R9,Context->R10,Context->R11,Context->R12,Context->R13,Context->R14,Context->R15);
		Print(L"rip=0x%016X rflags=0x%016X\n",Context->Rip,Context->Rflags);
		Print(L"es=0x%04X cs=0x%04X ss=0x%04X ds=0x%04X fs=0x%04X gs=0x%04X ldtr=0x%04X tr=0x%04X GDTR.Limit=0x%04X GDTR.Base=0x%016X IDTR.Limit=0x%04X IDTR.Base=0x%016X\n",Context->Es,Context->Cs,Context->Ss,Context->Ds,Context->Fs,Context->Gs,Context->Ldtr,Context->Tr,Context->Gdtr[0],Context->Gdtr[1],Context->Idtr[0],Context->Idtr[1]);
		Print(L"cr0=0x%016X cr2=0x%016X cr3=0x%016X cr4=0x%016X cr8=0x%016X\n",Context->Cr0,Context->Cr2,Context->Cr3,Context->Cr4,Context->Cr8);
		Print(L"dr0=0x%016X dr1=0x%016X dr2=0x%016X dr3=0x%016X dr6=0x%016X dr7=0x%016X\n",Context->Dr0,Context->Dr1,Context->Dr2);
	}
	else if(MaxCol>=210)				// Column size allows at least 9 GPRs to be displayed.  5 rows will be printed. (e.g: 240x56)
	{
		Print(L"rax=0x%016X rcx=0x%016X rdx=0x%016X rbx=0x%016X rsp=0x%016X rbp=0x%016X rsi=0x%016X rdi=0x%016X r8 =0x%016X\n",Context->Rax,Context->Rcx,Context->Rdx,Context->Rbx,Context->Rsp,Context->Rbp,Context->Rsi,Context->Rdi,Context->R8);
		Print(L"r9 =0x%016X r10=0x%016X r11=0x%016X r12=0x%016X r13=0x%016X r14=0x%016X r15=0x%016X rip=0x%016X rflags=0x%016X\n",Context->R9,Context->R10,Context->R11,Context->R12,Context->R13,Context->R14,Context->R15,Context->Rip,Context->Rflags);
		Print(L"es=0x%04X cs=0x%04X ss=0x%04X ds=0x%04X fs=0x%04X gs=0x%04X ldtr=0x%04X tr=0x%04X GDTR.Limit=0x%04X GDTR.Base=0x%016X IDTR.Limit=0x%04X IDTR.Base=0x%016X\n",Context->Es,Context->Cs,Context->Ss,Context->Ds,Context->Fs,Context->Gs,Context->Ldtr,Context->Tr,Context->Gdtr[0],Context->Gdtr[1],Context->Idtr[0],Context->Idtr[1]);
		Print(L"cr0=0x%016X cr2=0x%016X cr3=0x%016X cr4=0x%016X cr8=0x%016X\n",Context->Cr0,Context->Cr2,Context->Cr3,Context->Cr4,Context->Cr8);
		Print(L"dr0=0x%016X dr1=0x%016X dr2=0x%016X dr3=0x%016X dr6=0x%016X dr7=0x%016X\n",Context->Dr0,Context->Dr1,Context->Dr2);
	}
}

void static NoirX64ExceptionCallback(IN EFI_EXCEPTION_TYPE ExceptionType,IN OUT EFI_SYSTEM_CONTEXT SystemContext)
{
	NoirX64PrintProcessorState(SystemContext.SystemContextX64);
}
#endif

EFI_STATUS NoirInitializeDebugAgent()
{
	EFI_STATUS st=NoirLocateDebugSupportProtocol();
	if(st==EFI_SUCCESS)
	{
		UINTN CpuCount;
		st=DebugSupport->GetMaximumProcessorIndex(DebugSupport,&CpuCount);
		if(st==EFI_SUCCESS)
		{
			Print(L"Number of Processors: %u\n",++CpuCount);
			for(UINTN i=0;i<CpuCount;i++)
			{
#if defined(MDE_CPU_X64)
				EFI_STATUS stx[0x20];
				ZeroMem(stx,sizeof(stx));
				stx[EXCEPT_X64_DIVIDE_ERROR]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_DIVIDE_ERROR);
				stx[EXCEPT_X64_DEBUG]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_DEBUG);
				stx[EXCEPT_X64_BREAKPOINT]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_BREAKPOINT);
				stx[EXCEPT_X64_OVERFLOW]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_OVERFLOW);
				stx[EXCEPT_X64_BOUND]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_BOUND);
				stx[EXCEPT_X64_INVALID_OPCODE]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_INVALID_OPCODE);
				stx[EXCEPT_X64_DOUBLE_FAULT]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_DOUBLE_FAULT);
				stx[EXCEPT_X64_INVALID_TSS]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_INVALID_TSS);
				stx[EXCEPT_X64_SEG_NOT_PRESENT]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_SEG_NOT_PRESENT);
				stx[EXCEPT_X64_STACK_FAULT]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_STACK_FAULT);
				stx[EXCEPT_X64_GP_FAULT]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_GP_FAULT);
				stx[EXCEPT_X64_PAGE_FAULT]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_PAGE_FAULT);
				stx[EXCEPT_X64_FP_ERROR]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_FP_ERROR);
				stx[EXCEPT_X64_ALIGNMENT_CHECK]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_ALIGNMENT_CHECK);
				stx[EXCEPT_X64_MACHINE_CHECK]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_MACHINE_CHECK);
				stx[EXCEPT_X64_SIMD]=DebugSupport->RegisterExceptionCallback(DebugSupport,i,NoirX64ExceptionCallback,EXCEPT_X64_SIMD);
#endif
			}
		}
	}
	return st;
}