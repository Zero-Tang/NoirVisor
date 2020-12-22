/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

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
#define NOIR_HVM_FEATURE_STEALTH_MSR_HOOK		0x1
#define NOIR_HVM_FEATURE_STEALTH_INLINE_HOOK	0x2
#define NOIR_HVM_FEATURE_CPUID_PRESENCE			0x4

#define NOIR_HVM_FEATURE_STEALTH_MSR_HOOK_BIT		0
#define NOIR_HVM_FEATURE_STEALTH_INLINE_HOOK_BIT	1
#define NOIR_HVM_FEATURE_CPUID_PRESENCE_BIT			2

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

void NoirBuildHookedPages();
void NoirTeardownHookedPages();
void __cdecl NoirDebugPrint(const char* Format,...);
ULONG nvc_build_hypervisor();
void nvc_teardown_hypervisor();
ULONG noir_visor_version();
void noir_get_vendor_string(char* vendor_string);
void noir_get_processor_name(char* processor_name);
ULONG noir_get_virtualization_supportability();
BOOLEAN noir_initialize_ci(PVOID section,ULONG size,BOOLEAN soft_ci,BOOLEAN hard_ci);
void noir_finalize_ci();

BOOLEAN NoirHypervisorStarted=FALSE;
PVOID NoirPowerCallbackObject=NULL;
PVOID NvImageBase=NULL;
ULONG NvImageSize=0;