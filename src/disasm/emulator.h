/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the emulator of NoirVisor, based on Zydis Library.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /disasm/emulator.h
*/

#include <nvdef.h>

extern ZydisDecoder ZyDec16;
extern ZydisDecoder ZyDec32;
extern ZydisDecoder ZyDec64;
extern ZydisFormatter ZyFmt;