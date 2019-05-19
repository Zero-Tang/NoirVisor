/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the processor facility of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/processor.h
*/

#include <ntddk.h>
#include <windef.h>

#define MSR_SYSENTER_CS			0x174
#define MSR_SYSENTER_ESP		0x174
#define MSR_SYSENTER_EIP		0x176
#define MSR_DBGCTRL				0x1D9
#define MSR_PAT					0x277
#define MSR_EFER				0xC0000080
#define MSR_STAR				0xC0000081
#define MSR_LSTAR				0xC0000082
#define MSR_CSTAR				0xC0000083
#define MSR_SYSCALL_MASK		0xC0000084
#define MSR_FSBASE				0xC0000100
#define MSR_GSBASE				0xC0000101
#define MSR_KERNEL_GSBASE		0xC0000102

#define RPLTI_MASK		0xFFF8

typedef struct _SEGMENT_REGISTER
{
	USHORT Selector;
	USHORT Attributes;
	ULONG32 Limit;
	LARGE_INTEGER Base;
}SEGMENT_REGISTER,*PSEGMENT_REGISTER;

typedef struct _NOIR_PROCESSOR_STATE
{
	//Segment Registers
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
	//Control Registers
	ULONG_PTR Cr0;
	ULONG_PTR Cr2;
	ULONG_PTR Cr3;
	ULONG_PTR Cr4;
#if defined(_WIN64)
	ULONG64 Cr8;	//CR8 is only available on 64-Bit Mode.
#endif
	//Debug Registers
	ULONG_PTR Dr0;
	ULONG_PTR Dr1;
	ULONG_PTR Dr2;
	ULONG_PTR Dr3;
	ULONG_PTR Dr6;
	ULONG_PTR Dr7;
	//Model Specific Registers
	ULONG64 SysEnterCs;
	ULONG64 SysEnterEsp;
	ULONG64 SysEnterEip;
	ULONG64 DebugControl;
	ULONG64 Pat;
	ULONG64 Efer;
	ULONG64 Star;
	ULONG64 LStar;
	ULONG64 CStar;
	ULONG64 SfMask;
	ULONG64 FsBase;
	ULONG64 GsBase;
	ULONG64 GsSwap;
}NOIR_PROCESSOR_STATE,*PNOIR_PROCESSOR_STATE;

typedef struct _KGDTENTRY
{
	USHORT LimitLow;
	USHORT BaseLow;
	BYTE BaseMid;
	union
	{
		struct
		{
			BYTE Type:4;
			BYTE System:1;
			BYTE Dpl:2;
			BYTE Present:1;
		};
		BYTE Value;
	}Flag1;
	union
	{
		struct
		{
			BYTE LimitHi:4;
			BYTE Available:1;
			BYTE LongMode:1;
			BYTE DefaultBig:1;
			BYTE Granularity:1;
		};
		BYTE Value;
	}Flag2;
	BYTE BaseHi;
#if defined(_WIN64)
	ULONG32 BaseUpper;
#endif
}KGDTENTRY,*PKGDTENTRY;

typedef struct _KDESCRIPTOR
{
#if defined(_WIN64)
	ULONG Pad0;
#endif
	USHORT Pad1;
	USHORT Limit;
	ULONG_PTR Base;
}KDESCRIPTOR,*PKDESCRIPTOR;

typedef struct _KSPECIAL_REGISTERS
{
	ULONG_PTR Cr0;
	ULONG_PTR Cr2;
	ULONG_PTR Cr3;
	ULONG_PTR Cr4;
	ULONG_PTR KernelDr0;
	ULONG_PTR KernelDr1;
	ULONG_PTR KernelDr2;
	ULONG_PTR KernelDr3;
	ULONG_PTR KernelDr6;
	ULONG_PTR KernelDr7;
	KDESCRIPTOR Gdtr;
	KDESCRIPTOR Idtr;
	USHORT Tr;
	USHORT Ldtr;
#if defined(_WIN64)
	ULONG32 MxCsr;
	ULONG64 DebugControl;
	ULONG64 LastBranchToRip;
	ULONG64 LastBranchFromRip;
	ULONG64 LastExceptionToRip;
	ULONG64 LastExceptionFromRip;
	ULONG64 Cr8;
	ULONG64 MsrGsBase;
	ULONG64 MsrGsSwap;
	ULONG64 MsrStar;
	ULONG64 MsrLStar;
	ULONG64 MsrCStar;
	ULONG64 MsrSyscallMask;
#else
	ULONG Reserved;
#endif
}KSPECIAL_REGISTERS,*PKSPECIAL_REGISTERS;

typedef struct _KPROCESSOR_STATE
{
#if defined(_WIN64)
	KSPECIAL_REGISTERS SpecialRegisters;
	CONTEXT ContextFrame;
#else
	CONTEXT ContextFrame;
	KSPECIAL_REGISTERS SpecialRegisters;
#endif
}KPROCESSOR_STATE,*PKPROCESSOR_STATE;

typedef void(*noir_broadcast_worker)(void* context,ULONG ProcessorNumber);

NTSYSAPI void NTAPI RtlCaptureContext(OUT PCONTEXT ContextRecord);
ULONG_PTR __readcr2();
void __sgdt(OUT PUSHORT dest);
USHORT __str();