/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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
#define NOIR_VCPU_ALREADY_CREATED		0xC0000006
#define NOIR_BUFFER_TOO_SMALL			0xC0000007
#define NOIR_VCPU_NOT_EXIST				0xC0000008

typedef ULONG32 NOIR_STATUS;

typedef enum _NOIR_CVM_REGISTER_TYPE
{
	NoirCvmGeneralPurposeRegister,
	NoirCvmFlagsRegister,
	NoirCvmInstructionPointer,
	NoirCvmControlRegister,
	NoirCvmCr2Register,
	NoirCvmDebugRegister,
	NoirCvmDr67Register,
	NoirCvmSegmentRegister,
	NoirCvmFsGsRegister,
	NoirCvmDescriptorTable,
	NoirCvmTrLdtrRegister,
	NoirCvmSysCallMsrRegister,
	NoirCvmSysEnterMsrRegister,
	NoirCvmCr8Register,
	NoirCvmFxState,
	NoirCvmXsaveArea,
	NoirCvmXcr0Register,
	NoirCvmMaxmimumRegisterType
}NOIR_CVM_REGISTER_TYPE,*PNOIR_CVM_REGISTER_TYPE;

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

#define MemoryWorkingSetExInformation		4

typedef NTSTATUS (*ZWQUERYVIRTUALMEMORY)
(
 IN HANDLE ProcessHandle,
 IN PVOID BaseAddress,
 IN ULONG MemoryInformationClass,
 OUT PVOID MemoryInformation,
 IN SIZE_T MemoryInformationLength,
 OUT PSIZE_T ReturnLength
);

typedef union _MEMORY_WORKING_SET_EX_BLOCK
{
	struct
	{
		ULONG_PTR Valid:1;				// Bit	0
		ULONG_PTR ShareCount:3;			// Bits	1-3
		ULONG_PTR Win32Protection:11;	// Bits	4-14
		ULONG_PTR Shared:1;				// Bit	15
		ULONG_PTR Node:6;				// Bits	16-21
		ULONG_PTR Locked:1;				// Bit	22
		ULONG_PTR LargePage:1;			// Bit	23
		ULONG_PTR Priority:3;			// Bits	24-26
		ULONG_PTR Reserved:3;			// Bits	27-29
		ULONG_PTR SharedOriginal:1;		// Bit	30
		ULONG_PTR Bad:1;				// Bit	31
#if defined(_WIN64)
		ULONG64 Win32GraphicsProtection:4;	// Bits	32-35
		ULONG64 ReservedUlong:28;			// Bits	36-63
#endif
	};
	ULONG_PTR Value;
}MEMORY_WORKING_SET_EX_BLOCK,*PMEMORY_WORKING_SET_EX_BLOCK;

typedef struct _MEMORY_WORKING_SET_EX_INFORMATION
{
	PVOID VirtualAddress;
	MEMORY_WORKING_SET_EX_BLOCK VirtualAttributes;
}MEMORY_WORKING_SET_EX_INFORMATION,*PMEMORY_WORKING_SET_EX_INFORMATION;

/*
  NoirVisor chooses the similar data structure to Windows' handle table.

  Multi-level table is supported. Each table has a size of a page.
  Each handle of a VM is defined as 64-bit for any systems.
  Therefore, there are 512 entries, 9 bits per level.

  To be honest, multi-level table is actually a multi-branching tree.
  This design supports 4096 levels of tree depth at most, though 7 levels
  should have covered all possible values of 64-bit handles.
*/

#define HandleTableCapacity		512
#define HandleTableShiftBits	9

typedef ULONG64 CVM_HANDLE;
typedef PULONG64 PCVM_HANDLE;

typedef struct _NOIR_CVM_HANDLE_TABLE
{
	ULONG_PTR TableCode;
	ERESOURCE HandleTableLock;
	CVM_HANDLE MaximumHandleValue;
	SIZE_T HandleCount;
}NOIR_CVM_HANDLE_TABLE,*PNOIR_CVM_HANDLE_TABLE;

PVOID NoirAllocateNonPagedMemory(IN SIZE_T Length);
PVOID NoirAllocatePagedMemory(IN SIZE_T Length);
void NoirFreeNonPagedMemory(IN PVOID VirtualAddress);
void NoirFreePagedMemory(IN PVOID VirtualAddress);
void __cdecl NoirDebugPrint(const char* Format,...);

PVOID NoirLocateImageBaseByName(IN PWSTR ImageName);
PVOID NoirLocateExportedProcedureByName(IN PVOID ImageBase,IN PSTR ProcedureName);

// Functions from NoirVisor XPF-Core for CVM.
ULONG32 nvc_query_physical_asid_limit(IN PSTR vendor_string);
void noir_get_vendor_string(OUT PSTR vendor_string);
NOIR_STATUS nvc_query_hypervisor_status(IN ULONG64 StatusType,OUT PVOID Status);
NOIR_STATUS nvc_create_vm(OUT PVOID *VirtualMachine,IN HANDLE ProcessId);
NOIR_STATUS nvc_create_vm_ex(OUT PVOID *VirtualMachine,IN HANDLE ProcessId,IN ULONG32 Properties,IN ULONG32 NumberOfAsid);
NOIR_STATUS nvc_release_vm(IN PVOID VirtualMachine);
NOIR_STATUS nvc_set_mapping(IN PVOID VirtualMachine,IN PNOIR_ADDRESS_MAPPING MappingInformation);
NOIR_STATUS nvc_set_mapping_ex(IN PVOID VirtualMachine,IN ULONG32 MappingId,IN PNOIR_ADDRESS_MAPPING MappingInformation);
NOIR_STATUS nvc_query_gpa_accessing_bitmap(IN PVOID VirtualMachine,IN ULONG64 GpaStart,IN ULONG32 NumberOfPages,OUT PVOID Bitmap,IN ULONG32 BitmapSize);
NOIR_STATUS nvc_query_gpa_accessing_bitmap_ex(IN PVOID VirtualMachine,IN ULONG32 MappingId,IN ULONG64 GpaStart,IN ULONG32 NumberOfPages,OUT PVOID Bitmap,IN ULONG32 BitmapSize);
NOIR_STATUS nvc_clear_gpa_accessing_bits(IN PVOID VirtualMachine,IN ULONG64 GpaStart,IN ULONG32 NumberOfPages);
NOIR_STATUS nvc_clear_gpa_accessing_bits_ex(IN PVOID VirtualMachine,IN ULONG32 MappingId,IN ULONG64 GpaStart,IN ULONG32 NumberOfPages);
NOIR_STATUS nvc_create_vcpu(IN PVOID VirtualMachine,OUT PVOID *VirtualProcessor,IN ULONG32 VpIndex);
NOIR_STATUS nvc_release_vcpu(IN PVOID VirtualProcessor);
NOIR_STATUS nvc_run_vcpu(IN PVOID VirtualProcessor,OUT PVOID ExitContext);
NOIR_STATUS nvc_rescind_vcpu(IN PVOID VirtualProcessor);
NOIR_STATUS nvc_select_mapping_for_vcpu(IN PVOID VirtualProcessor,IN ULONG32 MappingId);
NOIR_STATUS nvc_retrieve_mapping_id_for_vcpu(IN PVOID VirtualProcessor,OUT PULONG32 MappingId);
NOIR_STATUS nvc_query_vcpu_statistics(IN PVOID VirtualProcessor,OUT PVOID Buffer,IN ULONG32 BufferSize);
NOIR_STATUS nvc_view_vcpu_registers(IN PVOID VirtualProcessor,IN NOIR_CVM_REGISTER_TYPE RegisterType,OUT PVOID Buffer,IN ULONG32 BufferSize);
NOIR_STATUS nvc_edit_vcpu_registers(IN PVOID VirtualProcessor,IN NOIR_CVM_REGISTER_TYPE RegisterType,IN PVOID Buffer,IN ULONG32 BufferSize);
NOIR_STATUS nvc_set_event_injection(IN PVOID VirtualProcessor,IN ULONG64 InjectedEvent);
NOIR_STATUS nvc_set_guest_vcpu_options(IN PVOID VirtualProcessor,IN ULONG32 OptionType,IN ULONG32 Options);
PVOID nvc_reference_vcpu(IN PVOID VirtualMachine,IN ULONG32 VpIndex);
HANDLE nvc_get_vm_pid(IN PVOID VirtualMachine);

NOIR_CVM_HANDLE_TABLE NoirCvmHandleTable={0};

ZWQUERYVIRTUALMEMORY ZwQueryVirtualMemory=NULL;