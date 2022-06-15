/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/uefihvm.h
*/

#include <Uefi.h>
#include <Protocol/LoadedImage.h>

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

#define NOIR_HVM_FEATURE_STEALTH_MSR_HOOK_BIT		0
#define NOIR_HVM_FEATURE_STEALTH_INLINE_HOOK_BIT	1
#define NOIR_HVM_FEATURE_CPUID_PRESENCE_BIT			2
#define NOIR_HVM_FEATURE_NESTED_VIRTUALIZATION_BIT	4
#define NOIR_HVM_FEATURE_KVA_SHADOW_PRESENCE_BIT	5

#if defined(MDE_CPU_X64)
#define EFI_IMAGE_NT_HEADERS	EFI_IMAGE_NT_HEADERS64
#else
#define EFI_IMAGE_NT_HEADERS	EFI_IMAGE_NT_HEADERS32
#endif

VOID* NvImageBase=NULL;
UINT32 NvImageSize=0;

void __cdecl NoirDebugPrint(IN CONST CHAR8 *Format,...);
void NoirSerialWrite(IN UINTN ComPort,IN UINT8 *Buffer,IN UINTN Length);

BOOLEAN noir_initialize_ci(VOID* section,UINT32 size,BOOLEAN soft_ci,BOOLEAN hard_ci);
void noir_finalize_ci();
UINT32 nvc_build_hypervisor();
void nvc_teardown_hypervisor();