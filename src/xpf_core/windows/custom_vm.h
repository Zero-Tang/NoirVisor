/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file defines customizable VM of NoirVisor for Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/custom_vm.h
*/

#include <ntddk.h>
#include <windef.h>

typedef union _NOIR_XMM_REGISTER
{
	float f[4];
	double d[2];
}NOIR_XMM_REGISTER,*PNOIR_XMM_REGISTER;

typedef union _NOIR_YMM_REGISTER
{
	float f[8];
	double d[4];
}NOIR_YMM_REGISTER,*PNOIR_YMM_REGISTER;

typedef union _NOIR_ZMM_REGISTER
{
	float f[16];
	double d[8];
}NOIR_ZMM_REGISTER,*PNOIR_ZMM_REGISTER;

typedef struct _NOIR_GPR_STATE
{
	ULONG64 Rax;
	ULONG64 Rcx;
	ULONG64 Rdx;
	ULONG64 Rbx;
	ULONG64 Rsp;
	ULONG64 Rbp;
	ULONG64 Rsi;
	ULONG64 Rdi;
	ULONG64 R8;
	ULONG64 R9;
	ULONG64 R10;
	ULONG64 R11;
	ULONG64 R12;
	ULONG64 R13;
	ULONG64 R14;
	ULONG64 R15;
}NOIR_GPR_STATE,*PNOIR_GPR_STATE;

typedef struct _NOIR_FXSAVE_STATE
{
	USHORT Fcw;			// Bytes	0-1
	USHORT Fsw;			// Bytes	2-3
	BYTE Ftw;			// Byte		4
	BYTE Reserved0;		// Byte		5
	USHORT Fop;			// Bytes	6-7
	ULONG64 Fip;		// Bytes	8-15
	ULONG64 Fdp;		// Bytes	16-23
	ULONG64 MxCsr;		// Bytes	24-27
	ULONG64 MxCsrMask;	// Bytes	28-31
	// Bytes 32-159
	ULONG64 Mm0;
	ULONG64 Rs0;
	ULONG64 Mm1;
	ULONG64 Rs1;
	ULONG64 Mm2;
	ULONG64 Rs2;
	ULONG64 Mm3;
	ULONG64 Rs3;
	ULONG64 Mm4;
	ULONG64 Rs4;
	ULONG64 Mm5;
	ULONG64 Rs5;
	ULONG64 Mm6;
	ULONG64 Rs6;
	ULONG64 Mm7;
	ULONG64 Rs7;
	// Bytes 160-415
	NOIR_XMM_REGISTER Xmm[16];
	// Bytes 416-463
	ULONG64 Reserved1[6];
	// Bytes 464-511
	ULONG64 Available[6];	// If lied at offset=464 is zero value, Fast FXSR is disabled.
}NOIR_FXSAVE_STATE,*PNOIR_FXSAVE_STATE;

typedef struct _NOIR_CR_STATE
{
	ULONG64 Cr0;
	ULONG64 Cr2;
	ULONG64 Cr3;
	ULONG64 Cr4;
	ULONG64 Cr8;
	ULONG64 XCr0;
}NOIR_CR_STATE,*PNOIR_CR_STATE;

typedef struct _NOIR_DR_STATE
{
	ULONG64 Dr0;
	ULONG64 Dr1;
	ULONG64 Dr2;
	ULONG64 Dr3;
	ULONG64 Dr6;
	ULONG64 Dr7;
}NOIR_DR_STATE,*PNOIR_DR_STATE;

typedef struct _NOIR_MSR_STATE
{
	ULONG64 SysEnterCs;
	ULONG64 SysEnterEsp;
	ULONG64 SysEnterEip;
	ULONG64 Efer;
	ULONG64 Star;
	ULONG64 LStar;
	ULONG64 CStar;
	ULONG64 SfMask;
}NOIR_MSR_STATE,*PNOIR_MSR_STATE;

typedef union _NOIR_CUSTOM_VCPU_OPTIONS
{
	struct
	{
		ULONG32 InterceptCpuid:1;
		ULONG32 InterceptRdmsr:1;
		ULONG32 InterceptWrmsr:1;
		ULONG32 InterceptIo:1;
		ULONG32 InterceptExceptions:1;
		ULONG32 Reserved:27;
	};
	ULONG32 Value;
}NOIR_CUSTOM_VCPU_OPTIONS,*PNOIR_CUSTOM_VCPU_OPTIONS;

typedef struct _NOIR_ADDRESS_MAPPING
{
	ULONG64 GPA;
	ULONG64 HVA;
	ULONG32 NumberOfPages;
	union
	{
		struct
		{
			ULONG32 Present:1;
			ULONG32 Write:1;
			ULONG32 Execute:1;
			ULONG32 User:1;
			ULONG32 Caching:3;
			ULONG32 PageSize:2;		// 0=4KiB Page, 1=2MiB Page, 2=1GiB Page, 3=Reserved (Maybe 512GiB Page)
			ULONG32 Reserved:23;
		};
		ULONG32 Value;
	}Attributes;
}NOIR_ADDRESS_MAPPING,*PNOIR_ADDRESS_MAPPING;

typedef struct _NOIR_CUSTOM_VIRTUAL_PROCESSOR
{
	NOIR_GPR_STATE GprState;
	NOIR_FXSAVE_STATE FprState;
	NOIR_CR_STATE CrState;
	NOIR_DR_STATE DrState;
	NOIR_MSR_STATE MsrState;
	PVOID Core;		// The vCPU Structure for NoirVisor's VT/SVM Core.
	ULONG64 Rip;
	NOIR_CUSTOM_VCPU_OPTIONS VcpuOptions;
}NOIR_CUSTOM_VIRTUAL_PROCESSOR,*PNOIR_CUSTOM_VIRTUAL_PROCESSOR;

typedef struct _NOIR_CUSTOM_VIRTUAL_MACHINE
{
	ULONG32 NumberOfVirtualProcessors;
	ULONG32 NumberOfMappings;
	PNOIR_CUSTOM_VIRTUAL_PROCESSOR VirtualProcessors;
	NOIR_ADDRESS_MAPPING Mappings[1];
}NOIR_CUSTOM_VIRTUAL_MACHINE,*PNOIR_CUSTOM_VIRTUAL_MACHINE;