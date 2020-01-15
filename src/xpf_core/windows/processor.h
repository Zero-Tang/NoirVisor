/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the processor facility of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/processor.h
*/

#include <ntddk.h>
#include <windef.h>

typedef void(*noir_broadcast_worker)(void* context,ULONG ProcessorNumber);