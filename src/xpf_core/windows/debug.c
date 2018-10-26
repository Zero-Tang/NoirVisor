/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is debug printing facility on Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/debug.c
*/

#include <ntddk.h>
#include <windef.h>
#include <stdarg.h>

void __cdecl nv_dprintf(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	vDbgPrintExWithPrefix("[NoirVisor] ",DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,format,arg_list);
	va_end(arg_list);
}

void __cdecl nv_tracef(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	vDbgPrintExWithPrefix("[NoirVisor - Trace] ",DPFLTR_IHVDRIVER_ID,DPFLTR_TRACE_LEVEL,format,arg_list);
	va_end(arg_list);
}

void __cdecl nv_panicf(const char* format,...)
{
	va_list arg_list;
	va_start(arg_list,format);
	vDbgPrintExWithPrefix("[NoirVisor - Panic] ",DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,format,arg_list);
	va_end(arg_list);
}