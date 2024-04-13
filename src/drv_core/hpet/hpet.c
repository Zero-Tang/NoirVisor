/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the HPET driver core for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/hpet/hpet.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <nv_intrin.h>
#include <acpi.h>
#include "hpet.h"

u32 static nvc_hpet_read_register32(u32 offset)
{
	// MMIO requires strong memory order.
	u32 value=0;
	noir_memory_fence();
	value=*(u32p)(hpet_base.address+offset);
	noir_memory_fence();
	return value;
}

u64 static nvc_hpet_read_register64(u32 offset)
{
	// MMIO requires strong memory order.
	u64 value=0;
	noir_memory_fence();
	value=*(u64p)(hpet_base.address+offset);
	noir_memory_fence();
	return value;
}

u64 nvc_hpet_read_counter()
{
	return nvc_hpet_read_register64(hpet_main_counter_value);
}

noir_status nvc_hpet_initialize()
{
	acpi_high_precision_event_timer_table_p hpet_table=null;
	noir_status st=nvc_acpi_search_table('TEPH',null,(acpi_common_description_header_p*)&hpet_table);
	if(st==noir_success)
	{
		nvd_printf("Located HPET from ACPI at 0x%p!\n",hpet_table);
		hpet_base=hpet_table->block;
		hpet_period=nvc_hpet_read_register32(hpet_general_counter_clock_period);
		nvd_printf("HPET Tick Period: %u femptoseconds\n",hpet_period);
	}
	return st;
}