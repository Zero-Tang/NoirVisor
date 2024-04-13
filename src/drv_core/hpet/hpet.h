/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the HPET driver core for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/hpet/hpet.h
*/

#include <nvdef.h>

// Offsets to registers from MMIO Base
#define hpet_general_capability_id					0x000
#define hpet_general_counter_clock_period			0x004
#define hpet_general_configuration					0x010
#define hpet_general_interrupt_status				0x020
#define hpet_main_counter_value						0x0F0
#define hpet_timer_configuration_capability_lo(n)	(0x100+(n<<5)+0x00)
#define hpet_timer_configuration_capability_hi(n)	(0x100+(n<<5)+0x04)
#define hpet_timer_comparator_value_lo(n)			(0x100+(n<<5)+0x08)
#define hpet_timer_comparator_value_hi(n)			(0x100+(n<<5)+0x0C)
#define hpet_timer_fsb_interrupt_value(n)			(0x100+(n<<5)+0x10)
#define hpet_timer_fsb_interrupt_address(n)			(0x100+(n<<5)+0x14)

typedef union _hpet_general_cap_id_register
{
	struct
	{
		u32 revision_id:8;
		u32 timer_count:5;
		u32 counter_size:1;
		u32 reserved:1;
		u32 legacy_replacement_route:1;
		u32 vendor_id:16;
	};
	u32 value;
}hpet_general_cap_id_register,*hpet_general_cap_id_register_p;

typedef union _hpet_general_config_register
{
	struct
	{
		u64 enable:1;
		u64 legacy_replacement_route:1;
		u64 reserved0:6;
		u64 reserved_non_os:8;
		u64 reserved1:48;
	};
	u64 value;
}hpet_general_config_register,*hpet_general_config_register_p;

typedef union _hpet_timer_config_register
{
	struct
	{
		u64 reserved0:1;
		u64 int_type:1;
		u64 int_enable:1;
		u64 timer_type:1;
		u64 can_be_periodic:1;
		u64 timer_size:1;
		u64 timer_value_set:1;
		u64 reserved1:1;
		u64 timer_32_bit_mode:1;
		u64 int_route:5;
		u64 fsb_enable:1;
		u64 reserved2:16;
		u64 route_cap:32;
	};
	u64 value;
}hpet_timer_config_register,*hpet_timer_config_register_p;

acpi_generic_address_structure hpet_base={0};
u32 hpet_period=0xFFFFFFFF;