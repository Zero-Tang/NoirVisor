/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the core engine of layered hypervisor for Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/layered.c
*/

#include <ntddk.h>
#include <windef.h>
#include "custom_vm.h"

NTSTATUS NoirCreateVirtualProcessor(IN PNOIR_CUSTOM_VIRTUAL_PROCESSOR CustomProcessor)
{
	return STATUS_NOT_IMPLEMENTED;
}