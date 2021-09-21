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

// Definitions of Status Codes of NoirVisor.
#define NOIR_SUCCESS					0
#define NOIR_UNSUCCESSFUL				0xC0000000
#define NOIR_INSUFFICIENT_RESOURCES		0xC0000001
#define NOIR_NOT_IMPLEMENTED			0xC0000002
#define NOIR_UNKNOWN_PROCESSOR			0xC0000003
#define NOIR_INVALID_PARAMETER			0xC0000004
#define NOIR_HYPERVISION_ABSENT			0xC0000005

typedef ULONG32 NOIR_STATUS;

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
	ULONG64 Rflags;
	ULONG64 Rip;
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

/*
  NoirVisor chooses the similar data structure to Windows' handle table.

  Multi-level table is supported. Each table has a size of a page.
  Therefore, there are 512 entries for 64-bit, and 1024 entries for 32-bit.
  In this regard, 9 bits per level for 64-bit, and 10 bits per level for 32 bit.
  Two levels for table should have covered all Intel VT-x VPIDs since there would
  be 18 bits for 64-bit and 20 bits for 32-bit, whereas VPID is a 16-bit field.

  To be honest, multi-level table is actually a multi-branching tree.
  This design supports 4096 levels of tree depth at most, though 7 levels should
  have covered all possible values of 64-bit handles.
*/

#if defined(_WIN64)
#define HandleTableCapacity		512
#define HandleTableShiftBits	9
#else
#define HandleTableCapacity		1024
#define HandleTableShiftBits	10
#endif

typedef ULONG_PTR CVM_HANDLE;
typedef PULONG_PTR PCVM_HANDLE;

typedef struct _NOIR_CVM_HANDLE_TABLE
{
	ULONG_PTR TableCode;
	ERESOURCE HandleTableLock;
	CVM_HANDLE MaximumHandleValue;
	ULONG32 HandleCount;
}NOIR_CVM_HANDLE_TABLE,*PNOIR_CVM_HANDLE_TABLE;

PVOID NoirAllocateNonPagedMemory(IN SIZE_T Length);
PVOID NoirAllocatePagedMemory(IN SIZE_T Length);
void NoirFreeNonPagedMemory(IN PVOID VirtualAddress);
void NoirFreePagedMemory(IN PVOID VirtualAddress);
void __cdecl NoirDebugPrint(const char* Format,...);

// Functions from NoirVisor XPF-Core for CVM.
ULONG32 nvc_query_physical_asid_limit(IN PSTR vendor_string);
void noir_get_vendor_string(OUT PSTR vendor_string);
NOIR_STATUS nvc_create_vm(OUT PVOID *VirtualMachine,HANDLE ProcessId);
NOIR_STATUS nvc_release_vm(IN PVOID VirtualMachine);
HANDLE nvc_get_vm_pid(IN PVOID VirtualMachine);

NOIR_CVM_HANDLE_TABLE NoirCvmHandleTable={0};