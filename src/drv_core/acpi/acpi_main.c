/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the ACPI driver core for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/acpi/acpi_main.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <nv_intrin.h>
#include "acpi_def.h"
#include "acpi_main.h"

noir_status nvc_acpi_initialize()
{
	noir_status st=noir_unsuccessful;
	acpi_rsdt_ptr=noir_locate_acpi_rsdt(&acpi_rsdt_len);
	if(acpi_rsdt_ptr)st=noir_success;
	return st;
}

void nvc_acpi_finalize()
{
#if !defined(_hv_type1)
	if(acpi_rsdt_ptr)noir_free_nonpg_memory(acpi_rsdt_ptr);
#endif
}