/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file defines the ACPI tables for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/acpi/acpi_def.h
*/

#include <nvdef.h>

#pragma pack(1)		// All ACPI structure members do not necessarily align.
typedef struct _acpi_common_description_header
{
	u32 signature;
	u32 length;
	u8 revision;
	u8 checksum;
	char oem_id[6];
	char oem_table_id[8];
	u32 oem_revision;
	u32 creator_id;
	u32 creator_revision;
}acpi_common_description_header,*acpi_common_description_header_p;

typedef struct _acpi_generic_address_structure
{
	u8 address_space_id;
	u8 register_bit_width;
	u8 register_bit_offset;
	u8 access_size;
	u64 address;
}acpi_generic_address_structure,*acpi_generic_address_structure_p;

typedef struct _acpi_root_system_description_table
{
	acpi_common_description_header header;
	u32 entries[1];
}acpi_root_system_description_table,*acpi_root_system_description_table_p;

typedef struct _acpi_extended_system_description_table
{
	acpi_common_description_header header;
	u64 entries[1];
}acpi_extended_system_description_table,*acpi_extended_system_description_table_p;

typedef struct _acpi_fixed_acpi_description_table
{
	acpi_common_description_header header;
	u32 firmware_ctrl;
	u32 dsdt;
	u8 reserved0;
	u8 preferred_pm_profile;
	u16 sci_int;
	u32 smi_cmd;
	u8 acpi_enable;
	u8 acpi_disable;
	u8 s4bios_req;
	u8 pstate_cnt;
	u32 pm1a_evt_blk;
	u32 pm1b_evt_blk;
	u32 pm1a_cnt_blk;
	u32 pm1b_cnt_blk;
	u32 pm2_cnt_lbk;
	u32 pm_tmr_blk;
	u32 gpe0_blk;
	u32 gpe1_blk;
	u8 pm1_evt_len;
	u8 pm1_cnt_len;
	u8 pm2_cnt_len;
	u8 pm_tmr_len;
	u8 gpe0_blk_len;
	u8 gpe1_blk_len;
	u8 gpe1_base;
	u8 cst_cnt;
	u16 p_lvl2_lat;
	u16 p_lvl3_lat;
	u16 flush_size;
	u16 flush_stride;
	u8 duty_offset;
	u8 duty_width;
	u8 day_alrm;
	u8 mon_alrm;
	u8 century;
	union
	{
		struct
		{
			u16 legacy_devices:1;
			u16 support_8042:1;
			u16 vga_not_present:1;
			u16 msi_not_supported:1;
			u16 pcie_aspm_controls:1;
			u16 cmos_rtc_not_present:1;
			u16 reserved:10;
		};
		u16 value;
	}iapc_boot_arch;
	u8 reserved1;
	union
	{
		struct
		{
			u32 wbinvd:1;
			u32 wbinvd_flush:1;
			u32 proc_c1:1;
			u32 p_lvl2_up:1;
			u32 pwr_button:1;
			u32 slp_button:1;
			u32 fix_rtc:1;
			u32 rtc_s4:1;
			u32 tmr_val_ext:1;
			u32 dck_cap:1;
			u32 reset_reg_sup:1;
			u32 sealed_case:1;
			u32 headless:1;
			u32 cpu_sw_slp:1;
			u32 pci_exp_wak:1;
			u32 use_platform_clock:1;
			u32 s4_rtc_sts_valid:1;
			u32 remote_power_on_capable:1;
			u32 force_apic_cluster_model:1;
			u32 force_apic_physical_destination_mode:1;
			u32 hw_reduced_acpi:1;
			u32 low_power_s0_idle_capable:1;
			u32 persistent_cpu_caches:2;
			u32 reserved:8;
		};
		u32 value;
	}flags;
	acpi_generic_address_structure reset_reg;
	u8 reset_value;
	union
	{
		struct
		{
			u16 psci_comliant:1;
			u16 psci_use_hvc:1;
			u16 reserved:14;
		};
		u16 value;
	}arm_boot_arch;
	u8 fadt_minor_version;
	u64 x_firmware_ctrl;
	u64 x_dsdt;
	acpi_generic_address_structure x_pm1a_evt_blk;
	acpi_generic_address_structure x_pm1b_evt_blk;
	acpi_generic_address_structure x_pm1a_cnt_blk;
	acpi_generic_address_structure x_pm1b_cnt_blk;
	acpi_generic_address_structure x_pm2_cnt_lbk;
	acpi_generic_address_structure x_pm_tmr_blk;
	acpi_generic_address_structure x_gpe0_blk;
	acpi_generic_address_structure x_gpe1_blk;
	acpi_generic_address_structure sleep_control_reg;
	acpi_generic_address_structure sleep_status_reg;
	u64 hypervisor_vendor_identity;
}acpi_fixed_acpi_description_table,*acpi_fixed_acpi_description_table_p;

#define acpi_facs_global_lock_pending		0
#define acpi_facs_global_lock_owned			1

#define acpi_facs_global_lock_pending_bit	0x1
#define acpi_facs_global_lock_owned_bit		0x2

typedef struct _acpi_firmware_acpi_control_structure
{
	u32 signature;
	u32 length;
	u32 hardware_signature;
	u32 firmware_waking_vector;
	u32v global_lock;
	union
	{
		struct
		{
			u32 s4bios_f:1;
			u32 wake_supported_f_64bit:1;
			u32 reserved:30;
		};
		u32 value;
	}flags;
	u64 x_firmware_waking_vector;
	u8 version;
	u8 reserved0[3];
	union
	{
		struct
		{
			u32 wake_f_64bit:1;
			u32 reserved:31;
		};
		u32 value;
	}ospm_flags;
	u8 reserved[24];
}acpi_firmware_acpi_control_structure,*acpi_firmware_acpi_control_structure_p;

typedef struct _acpi_differentiated_system_description_table
{
	acpi_common_description_header header;
	u8 definition_block[1];
}acpi_differentiated_system_description_table,*acpi_differentiated_system_description_table_p;

typedef struct _acpi_secondary_system_description_table
{
	acpi_common_description_header header;
	u8 definition_block[1];
}acpi_secondary_system_description_table,*acpi_secondary_system_description_table_p;

#define acpi_madt_type_local_apic					0x0
#define acpi_madt_type_io_apic						0x1
#define acpi_madt_type_interrupt_source_override	0x2
#define acpi_madt_type_nmi_source					0x3
#define acpi_madt_type_local_apic_nmi				0x4
#define acpi_madt_type_local_apic_addrress_override	0x5
#define acpi_madt_type_io_sapic						0x6
#define acpi_madt_type_local_sapic					0x7
#define acpi_madt_type_platform_interrupt_sources	0x8
#define acpi_madt_type_local_x2apic					0x9
#define acpi_madt_type_local_x2apic_nmi				0xA
#define acpi_madt_type_gic_cpu_interface			0xB
#define acpi_madt_type_gic_distributor				0xC
#define acpi_madt_type_gic_msi_frame				0xD
#define acpi_madt_type_gic_redistributor			0xE
#define acpi_madt_type_gic_itc						0xF
#define acpi_madt_type_multiprocessor_wakeup		0x10
#define acpi_madt_type_core_pic						0x11
#define acpi_madt_type_legacy_io_pic				0x12
#define acpi_madt_type_hypertransport_pic			0x13
#define acpi_madt_type_extended_io_pic				0x14
#define acpi_madt_type_msi_pic						0x15
#define acpi_madt_type_bridge_io_pic				0x16
#define acpi_madt_type_low_pin_count_pic			0x17
#define acpi_madt_type_oem_reserved_start			0x80

typedef struct _acpi_madt_ic_header
{
	u8 type;
	u8 length;
}acpi_madt_ic_header,*acpi_madt_ic_header_p;

typedef union _acpi_madt_local_apic_flags
{
	struct
	{
		u32 enabled:1;
		u32 online_capable:1;
		u32 reserved:30;
	};
	u32 value;
}acpi_madt_local_apic_flags,*acpi_madt_local_apic_flags_p;

typedef struct _acpi_madt_local_apic
{
	acpi_madt_ic_header header;
	u8 acpi_processor_uid;
	u8 apic_id;
	acpi_madt_local_apic_flags flags;
}acpi_madt_local_apic,*acpi_madt_local_apic_p;

typedef struct _acpi_madt_io_apic
{
	acpi_madt_ic_header header;
	u8 io_apic_id;
	u8 reserved;
	u32 io_apic_address;
	u32 global_system_interrupt_base;
}acpi_madt_io_apic,*acpi_madt_io_apic_p;

typedef union _acpi_madt_mps_inti_flags
{
	struct
	{
		u16 polarity:2;
		u16 trigger_mode:2;
		u16 reserved;
	};
	u16 value;
}acpi_madt_mps_inti_flags,*acpi_madt_mps_inti_flags_p;

typedef struct _acpi_madt_interrupt_source_override
{
	acpi_madt_ic_header header;
	u8 bus;
	u8 source;
	u32 global_system_interrupt;
	acpi_madt_mps_inti_flags flags;
}acpi_madt_interrupt_source_override,*acpi_madt_interrupt_source_override_p;

typedef struct _acpi_madt_nmi_source
{
	acpi_madt_ic_header header;
	acpi_madt_mps_inti_flags flags;
	u32 global_system_interrupt;
}acpi_madt_nmi_source,*acpi_madt_nmi_source_p;

typedef struct _acpi_madt_local_apic_address_override
{
	acpi_madt_ic_header header;
	u16 reserved;
	u64 local_apic_address;
}acpi_madt_local_apic_address_override,*acpi_madt_local_apic_address_override_p;

typedef struct _acpi_madt_io_sapic
{
	acpi_madt_ic_header header;
	u8 io_sapic_id;
	u8 reserved;
	u32 global_system_interrupt_base;
	u64 io_sapic_address;
}acpi_madt_io_sapic,*acpi_madt_io_sapic_p;

typedef struct _acpi_madt_local_sapic
{
	acpi_madt_ic_header header;
	u8 acpi_processor_id;
	u8 local_sapic_id;
	u8 local_sapic_eid;
	u8 reserved[3];
	acpi_madt_local_apic_flags flags;
	u32 acpi_processor_uid;
	char acpi_processor_uid_string[1];
}acpi_madt_local_sapic,*acpi_madt_local_sapic_p;

typedef struct _acpi_madt_platform_interrupt_source
{
	acpi_madt_ic_header header;
	acpi_madt_mps_inti_flags flags;
	u8 interrupt_type;
	u8 processor_id;
	u8 processor_eid;
	u8 io_sapic_vector;
	u32 global_system_interrupt;
	union
	{
		struct
		{
			u32 cpei_processor_override:1;
			u32 reserved:31;
		};
		u32 value;
	}platform_interrupt_source_flags;
}acpi_madt_platform_interrupt_source,*acpi_madt_platform_interrupt_source_p;

typedef struct _acpi_madt_local_x2apic
{
	acpi_madt_ic_header header;
	u8 reserved[2];
	u32 x2apic_id;
	acpi_madt_local_apic_flags flags;
	u32 acpi_processor_uid;
}acpi_madt_local_x2apic,*acpi_madt_local_x2apic_p;

typedef struct _acpi_madt_local_x2apic_nmi
{
	acpi_madt_ic_header header;
	acpi_madt_mps_inti_flags flags;
	u32 acpi_processor_uid;
	u8 local_x2apic_lint;
	u8 reserved[3];
}acpi_madt_local_x2apic_nmi,*acpi_madt_local_x2apic_nmi_p;

// ARM-related (GIC) definitions are currently ignored in NoirVisor ACPI driver.

typedef struct _acpi_madt_multiprocessor_wakeup
{
	acpi_madt_ic_header header;
	u16 mailbox_version;
	u32 reserved;
	u64 mailbox_address;
}acpi_madt_multiprocessor_wakeup,*acpi_madt_multiprocessor_wakeup_p;

typedef struct _acpi_mp_wakeup_mailbox
{
	u16 command;
	u16 reserved;
	u32 apic_id;
	u64 wakeup_vector;
	u8 reserved_for_os[2032];
	u8 reserved_for_fw[2048];
}acpi_mp_wakeup_mailbox,*acpi_mp_wakeup_mailbox_p;

// LoongArch/Loongson-related (Core, LIO, HT, EIO, BIO and LPC PIC) definitions are currently ignored in NoirVisor ACPI driver.

typedef struct _acpi_multiple_apic_description_table
{
	acpi_common_description_header header;
	u32 local_interrupt_controller_address;
	union
	{
		struct
		{
			u32 pcat_compat:1;
			u32 reserved:31;
		};
		u32 value;
	}flags;
	u8 interrupt_controllers[1];
}acpi_multiple_apic_description_table,*acpi_multiple_apic_description_table_p;

typedef struct _acpi_srat_affinity_header
{
	u8 type;
	u8 length;
}acpi_srat_affinity_header,*acpi_srat_affinity_header_p;

typedef union _acpi_srat_apic_affinity_flags
{
	struct
	{
		u32 enabled:1;
		u32 reserved:31;
	};
	u32 value;
}acpi_srat_apic_affinity_flags,*acpi_srat_apic_affinity_flags_p;

typedef struct _acpi_srat_local_apic_affinity
{
	acpi_srat_affinity_header header;
	u8 proximity_domain;
	u8 apic_id;
	acpi_srat_apic_affinity_flags flags;
	u8 local_sapic_eid;
	u8 proximity_domain_high[3];
	u32 clock_domain;
}acpi_srat_local_apic_affinity;

typedef struct _acpi_srat_memory_affinity
{
	acpi_srat_affinity_header header;
	u32 proximity_domain;
	u16 reserved0;
	u64 base_address;
	u64 length;
	u32 reserved1;
	union
	{
		struct
		{
			u32 enabled:1;
			u32 hot_pluggable:1;
			u32 non_volatile:1;
			u32 reserved:29;
		};
		u32 value;
	}flags;
	u64 reserved2;
}acpi_srat_memory_affinity,*acpi_srat_memory_affinity_p;

typedef struct _acpi_srat_local_x2apic_affinity
{
	acpi_srat_affinity_header header;
	u16 reserved0;
	u32 proximity_domain;
	u32 x2apic_id;
	acpi_srat_apic_affinity_flags flags;
	u32 clock_domain;
	u32 reserved1;
}acpi_srat_local_x2apic_affinity,*acpi_srat_local_x2apic_affinity_p;

typedef struct _acpi_system_resource_affinity_table
{
	acpi_common_description_header header;
	u32 reserved0;
	u64 reserved1;
	u8 affinity_table[1];
}acpi_system_resource_affinity_table,*acpi_system_resource_affinity_table_p;

typedef struct _acpi_system_locality_information_table
{
	acpi_common_description_header header;
	u64 number_of_system_localities;
	u8 matrix_entry[1];
}acpi_system_locality_information_table,*acpi_system_locality_information_table_p;

typedef struct _acpi_maximum_proximity_domain_information
{
	u8 revision;
	u8 length;
	u64 proximity_domain_range;
	u32 maximum_processor_capacity;
	u64 mximum_memory_capacity;
}acpi_maximum_proximity_domain_information,*acpi_maximum_proximity_domain_information_p;

typedef struct _acpi_maximum_system_characteristics_table
{
	acpi_common_description_header header;
	u32 offset;
	u32 proximity_domain_maximum;
	u32 clock_domain_maximum;
	u64 physical_address_maximum;
}acpi_maximum_system_characteristics_table,*acpi_maxmimum_system_characteristics_table_p;

typedef struct _acpi_dma_remapping_reporting_structure
{
	acpi_common_description_header header;
	u8 host_address_width;
	union
	{
		struct
		{
			u8 intr_remap:1;
			u8 x2apic_opt_out:1;
			u8 dma_ctrl_platform_opt_in_flags:1;
			u8 reserved:5;
		};
		u8 value;
	}flags;
	u8 reserved[10];
	u8 remapping_structures[1];
}acpi_dma_remapping_reporting_structure,*acpi_dma_remapping_reporting_structure_p;

typedef struct _acpi_io_virtualization_reporting_structure
{
	acpi_common_description_header header;
	union
	{
		struct
		{
			u32 efr_support:1;
			u32 dma_remap_support:1;
			u32 reserved0:3;
			u32 gva_size:3;
			u32 pa_size:7;
			u32 va_size:7;
			u32 ht_ats_reserved:1;
			u32 reserved1:9;
		};
		u32 value;
	}iv_info;
	u8 reserved;
	u8 ivdb[1];
}acpi_io_virtualization_reporting_structure,*acpi_io_virtualization_reporting_structure_p;

typedef struct _acpi_high_precision_event_timer_table
{
	acpi_common_description_header header;
	union
	{
		struct
		{
			u32 hardware_rev_id:8;
			u32 comparators:5;
			u32 count_size_cap:1;
			u32 reserved:1;
			u32 legacy_replacement:1;
			u32 pci_vendor_id:16;
		};
		u32 value;
	}hardware_id;
	acpi_generic_address_structure block;
	u8 hpet_number;
	u16 minimum_tick;
	u8 page_protection;
}acpi_high_precision_event_timer_table,*acpi_high_precision_event_timer_table_p;
#pragma pack()