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