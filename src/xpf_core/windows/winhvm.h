/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/winhvm.h
*/

#include <ntddk.h>
#include <windef.h>

// MSR Constants
#define HV_X64_MSR_GUEST_OS_ID		0x40000000
#define HV_MICROSOFT_VENDOR_ID		0x0001
#define HV_WINDOWS_NT_OS_ID			4

// CPUID Constants
#define CPUID_LEAF_HV_VENDOR_ID			0x40000000
#define CPUID_LEAF_HV_VENDOR_NEUTRAL	0x40000001

// Constants for Enabled Feature
#define NOIR_HVM_FEATURE_STEALTH_MSR_HOOK		0x01
#define NOIR_HVM_FEATURE_STEALTH_INLINE_HOOK	0x02
#define NOIR_HVM_FEATURE_CPUID_PRESENCE			0x04
#define NOIR_HVM_FEATURE_NESTED_VIRTUALIZATION	0x10
#define NOIR_HVM_FEATURE_KVA_SHADOW_PRESENCE	0x20
#define NOIR_HVM_FEATURE_HIDE_FROM_IPT			0x40

#define NOIR_HVM_FEATURE_STEALTH_MSR_HOOK_BIT		0
#define NOIR_HVM_FEATURE_STEALTH_INLINE_HOOK_BIT	1
#define NOIR_HVM_FEATURE_CPUID_PRESENCE_BIT			2
#define NOIR_HVM_FEATURE_NESTED_VIRTUALIZATION_BIT	4
#define NOIR_HVM_FEATURE_KVA_SHADOW_PRESENCE_BIT	5
#define NOIR_HVM_FEATURE_HIDE_FROM_IPT_BIT			6

typedef union _HV_MSR_PROPRIETARY_GUEST_OS_ID
{
	struct
	{
		ULONG64 BuildNumber:16;		// Bits	0-15
		ULONG64 ServiceVersion:8;	// Bits	15-23
		ULONG64 MinorVersion:8;		// Bits	24-31
		ULONG64 MajorVersion:8;		// Bits	32-39
		ULONG64 OsId:8;				// Bits	40-47
		ULONG64 VendorId:15;		// Bits	48-62
		ULONG64 OsType:1;			// Bit	63
	};
	ULONG64 Value;
}HV_MSR_PROPRIETARY_GUEST_OS_ID,*PHV_MSR_PROPRIETARY_GUEST_OS_ID;

#define EFI_VARIABLE_NON_VOLATILE			0x01
#define EFI_VARIABLE_BOOTSERVICE_ACCESS		0x02
#define EFI_VARIABLE_RUNTIME_ACCESS			0x04

typedef struct _HYPERVISOR_SYSTEM_IDENTITY
{
	ULONG32 BuildNumber;
	union
	{
		struct
		{
			ULONG32 MinorVersion:16;
			ULONG32 MajorVersion:16;
		};
		ULONG32 Version;
	};
	ULONG32 ServicePack;
	union
	{
		struct
		{
			ULONG32 ServiceNumber:24;
			ULONG32 ServiceBranch:8;
		};
		ULONG32 Service;
	};
}HYPERVISOR_SYSTEM_IDENTITY,*PHYPERVISOR_SYSTEM_IDENTITY;

typedef struct _HYPERVISOR_LAYERING_PASSCODE
{
	BYTE Length;
	CHAR Text[255];
}HYPERVISOR_LAYERING_PASSCODE,*PHYPERVISOR_LAYERING_PASSCODE;

// Albeit the HalGetEnvironmentVariableEx is undocumented in MSDN, this function
// actually follows the prototype definition of RuntimeServices->GetVariable in UEFI.
// HAL will do some BIOS/UEFI checks and EFI_STATUS to NTSTATUS mappings in this function.
typedef NTSTATUS (*HALGETENVIRONMENTVARIABLEEX)
(
 IN PWSTR VariableName,
 IN CONST GUID *VendorGuid,
 OUT PVOID Data,
 OUT PSIZE_T Size,
 IN ULONG32 Attributes
);

typedef NTSTATUS (*EXGETFIRMWAREENVIRONMENTVARIABLE)
(
 IN PUNICODE_STRING VariableName,
 IN LPGUID VendorGuid,
 OUT PVOID Value OPTIONAL,
 IN OUT PULONG ValueLength,
 OUT PULONG Attributes OPTIONAL
);

HALGETENVIRONMENTVARIABLEEX HalGetEnvironmentVariableEx=NULL;
EXGETFIRMWAREENVIRONMENTVARIABLE NoirExGetFirmwareEnvironmentVariable=NULL;

PVOID NoirLocateImageBaseByName(IN PWSTR ImageName);
PVOID NoirLocateExportedProcedureByName(IN PVOID ImageBase,IN PSTR ProcedureName);

PVOID NoirAllocateNonPagedMemory(IN SIZE_T Length);
void NoirFreeNonPagedMemory(IN PVOID VirtualAddress);
PVOID NoirAllocatePagedMemory(IN SIZE_T Length);
void NoirFreePagedMemory(IN PVOID VirtualAddress);

BOOLEAN NoirDetectKvaShadow();
NTSTATUS NoirInitializeCvmModule();
NTSTATUS NoirFinalizeCvmModule();
void NoirBuildHookedPages();
void NoirTeardownHookedPages();
void __cdecl NoirDebugPrint(const char* Format,...);
ULONG nvc_build_hypervisor();
void nvc_teardown_hypervisor();
ULONG noir_configure_serial_port_debugger(IN BYTE PortNumber,IN USHORT PortBase,IN ULONG32 BaudRate);
ULONG nvc_acpi_initialize();
void nvc_acpi_finalize();
ULONG noir_visor_version();
void noir_get_vendor_string(char* vendor_string);
void noir_get_processor_name(char* processor_name);
ULONG noir_get_virtualization_supportability();
BOOLEAN noir_is_virtualization_enabled();
BOOLEAN noir_initialize_ci(BOOLEAN soft_ci,BOOLEAN hard_ci);
BOOLEAN noir_add_section_to_ci(PVOID base,ULONG32 size,BOOLEAN enable_scan);
BOOLEAN noir_activate_ci();
void noir_finalize_ci();

GUID EfiNoirVisorVendorGuid={0x2B1F2A1E,0xDBDF,0x44AC,0xDA,0xBC,0xC7,0xA1,0x30,0xE2,0xE7,0x1E};

BOOLEAN NoirHypervisorStarted=FALSE;
PVOID NoirPowerCallbackObject=NULL;
PVOID NvImageBase=NULL;
ULONG NvImageSize=0;