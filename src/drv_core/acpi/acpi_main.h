/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the ACPI driver header for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/acpi/acpi_main.h
*/

#include <nvdef.h>

void* noir_locate_acpi_rsdt(size_t *length);

void* acpi_rsdt_ptr;
size_t acpi_rsdt_len;