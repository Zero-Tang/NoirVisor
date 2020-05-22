/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/uefihvm.h
*/

#include <Uefi.h>

#if defined(MDE_CPU_X64)
#define EFI_IMAGE_NT_HEADERS	EFI_IMAGE_NT_HEADERS64
#else
#define EFI_IMAGE_NT_HEADERS	EFI_IMAGE_NT_HEADERS32
#endif

BOOLEAN noir_initialize_ci(PVOID section,ULONG size,BOOLEAN soft_ci,BOOLEAN hard_ci);
void noir_finalize_ci();