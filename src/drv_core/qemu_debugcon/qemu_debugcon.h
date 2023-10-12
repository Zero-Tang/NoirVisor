/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the driver of QEMU Debug Console.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/qemu_debugcon.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>

#define qemu_debugcon_readback	0xE9

// The default I/O port for QEMU's debug console is 0x402.
u16 qemu_debugcon_port=0x0402;