/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file defines the hardware of Intel VT-d.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_iommu.h
*/

#include <nvdef.h>

// Defined below are Intel VT-d Reporting Structure.
// See Chapter 8 "BIOS Considerations", Intel (R) Virtualization Technology for Directed I/O (June 2022)

#pragma pack(1)
typedef struct _intel_iommu_acpi_unit_common_header
{
	u16 type;
	u16 length;
}intel_iommu_acpi_unit_common_header,*intel_iommu_acpi_unit_common_header_p;

#define intel_iommu_acpi_type_drhd		0
#define intel_iommu_acpi_type_rmrr		1
#define intel_iommu_acpi_type_atsr		2
#define intel_iommu_acpi_type_rhsa		3
#define intel_iommu_acpi_type_andd		4
#define intel_iommu_acpi_type_satc		5
#define intel_iommu_acpi_type_sidp		6

typedef struct _intel_iommu_acpi_drhd
{
	intel_iommu_acpi_unit_common_header header;
	union
	{
		struct
		{
			u8 include_pci_all:1;
			u8 reserved:7;
		};
		u8 value;
	}flags;
	union
	{
		struct
		{
			u8 exponential:4;
			u8 reserved:4;
		};
		u8 value;
	}size;
	u16 segment_number;
	u64 base_address;
	u8 device_scope[0];
}intel_iommu_acpi_drhd,*intel_iommu_acpi_drhd_p;

typedef struct _intel_iommu_acpi_rmrr
{
	intel_iommu_acpi_unit_common_header header;
	u16 reserved;
	u16 segment_number;
	u64 base_address;
	u64 limit_address;
	u8 device_scope[0];
}intel_iommu_acpi_rmrr,*intel_iommu_acpi_rmrr_p;

typedef struct _intel_iommu_acpi_atsr
{
	intel_iommu_acpi_unit_common_header header;
	union
	{
		struct
		{
			u8 all_ports:1;
			u8 reserved:7;
		};
		u8 value;
	}flags;
	u8 reserved;
	u16 segment_number;
	u8 device_scope[0];
}intel_iommu_acpi_atsr,*intel_iommu_acpi_atsr_p;

typedef struct _intel_iommu_acpi_rhsa
{
	intel_iommu_acpi_unit_common_header header;
	u32 reserved;
	u64 base_address;
	u32 proximity_domain;
}intel_iommu_acpi_rhsa,*intel_iommu_acpi_rhsa_p;

typedef struct _intel_iommu_acpi_andd
{
	intel_iommu_acpi_unit_common_header header;
	u8 reserved[3];
	u8 acpi_device_number;
	char acpi_object_name[0];
}intel_iommu_acpi_andd,*intel_iommu_acpi_andd_p;

typedef struct _intel_iommu_acpi_satc
{
	intel_iommu_acpi_unit_common_header header;
	union
	{
		struct
		{
			u8 atc_required:1;
			u8 reserved:7;
		};
		u8 value;
	}flags;
	u8 reserved;
	u16 segment_number;
	u8 device_scope[0];
}intel_iommu_acpi_satc,*intel_iommu_acpi_satc_p;

typedef struct _intel_iommu_acpi_sidp
{
	intel_iommu_acpi_unit_common_header header;
	u16 reserved;
	u16 segment_number;
	u8 device_scope[0];
}intel_iommu_acpi_sidp,*intel_iommu_acpi_sidp_p;

typedef struct _intel_iommu_acpi_device_scope
{
	u8 type;
	u8 length;
	union
	{
		struct
		{
			u8 req_wo_pasid_nested_notallowed:1;
			u8 req_wo_pasid_pwsnp_notallowed:1;
			u8 req_wo_pasid_pgsnp_notallowed:1;
			u8 atc_hardened:1;
			u8 atc_required:1;
			u8 reserved:3;
		};
		u8 value;
	}flags;
	u8 reserved;
	u8 enum_id;
	u8 start_bus_number;
	u16 path[0];
}intel_iommu_acpi_device_scope,*intel_iommu_acpi_device_scope_p;

#define intel_iommu_drhd_devscope_pci_endpoint		0x01
#define intel_iommu_drhd_devscope_pci_subhierarchy	0x02
#define intel_iommu_drhd_devscope_ioapic			0x03
#define intel_iommu_drhd_devscope_msi_hpet			0x04
#define intel_iommu_drhd_devscope_acpi_namespace	0x05
#pragma pack()

// Defined below are Translation Structure Formats.
// See Chapter 9 "Translation Structure Formats", Intel(R) Virtualization Technology for Directed I/O (June 2022)

typedef struct _intel_iommu_root_entry
{
	union
	{
		struct
		{
			u64 present:1;				// Bit	0
			u64 reserved1:11;			// Bits	1-11
			u64 context_table_ptr:52;	// Bits	12-63
		};
		u64 value;
	}lo;
	union
	{
		u64 reserved;
		u64 value;
	}hi;
}intel_iommu_root_entry,*intel_iommu_root_entry_p;

typedef struct _intel_iommu_scalable_root_entry
{
	union
	{
		struct
		{
			u64 present:1;				// Bit	0
			u64 reserved1:11;			// Bits	1-11
			u64 context_table_ptr:52;	// Bits	12-63
		};
		u64 value;
	}lo;
	union
	{
		struct
		{
			u64 present:1;				// Bit	64
			u64 reserved1:11;			// Bits	65-75
			u64 context_table_ptr:52;	// Bits	76-127
		};
		u64 value;
	}hi;
}intel_iommu_scalable_root_entry,*intel_iommu_scalable_root_entry_p;

typedef struct _intel_iommu_context_entry
{
	union
	{
		struct
		{
			u64 present:1;				// Bit	0
			u64 fault_proc_disable:1;	// Bit	1
			u64 translation_type:2;		// Bits	2-3
			u64 reserved1:8;			// Bits	4-11
			u64 sspt_ptr:52;			// Bits	12-63
		};
		u64 value;
	}lo;
	union
	{
		struct
		{
			u64 address_width:3;		// Bits 64-66
			u64 ignored:4;				// Bits	67-70
			u64 reserved1:1;			// Bit	71
			u64 domain_id:16;			// Bits	72-87
			u64 reserved2:40;			// Bits	88-127
		};
		u64 value;
	}hi;
}intel_iommu_context_entry,*intel_iommu_context_entry_p;

#define intel_iommu_context_sup_untranslated		0
#define intel_iommu_context_sup_full				1
#define intel_iommu_context_pass_through			2

#define intel_iommu_context_agaw_39_bit		1
#define intel_iommu_context_agaw_48_bit		2
#define intel_iommu_context_agaw_57_bit		3

typedef struct _intel_iommu_scalable_context_entry
{
	union
	{
		struct
		{
			u64 present:1;				// Bit	0
			u64 fault_proc_disable:1;	// Bit	1
			u64 device_tlb_enable:1;	// Bit	2
			u64 pasid_enable:1;			// Bit	3
			u64 page_request_enable:1;	// Bit	4
			u64 reserved:4;				// Bits	5-8
			u64 pasid_dir_size:3;		// Bits	9-11
			u64 pasid_dir_ptr:52;		// Bits	12-63
		};
		u64 value;
	}v1;
	union
	{
		struct
		{
			u64 rid_pasid:20;			// Bits	64-83
			u64 rid_priv:1;				// Bit	84
			u64 reserved:43;			// Bits	85-127
		};
		u64 value;
	}v2;
	union
	{
		u64 reserved;
		u64 value;
	}v3;
	union
	{
		u64 reserved;
		u64 value;
	}v4;
}intel_iommu_scalable_context_entry,*intel_iommu_scalable_context_entry_p;

typedef union _intel_iommu_scalable_pasid_directory_entry
{
	struct
	{
		u64 present:1;					// Bit	0
		u64 fault_proc_disable:1;		// Bit	1
		u64 reserved:10;				// Bit	2-11
		u64 pasid_table_ptr:52;			// Bits	12-63
	};
	u64 value;
}intel_iommu_scalable_pasid_directory_entry,*intel_iommu_scalable_pasid_directory_entry_p;

typedef struct _intel_iommu_scalable_pasid_table_entry
{
	union
	{
		struct
		{
			u64 present:1;				// Bit	0
			u64 fault_proc_disable:1;	// Bit	1
			u64 address_width:3;		// Bits	2-4
			u64 ss_execute_enable:1;	// Bit	5
			u64 translation_type:3;		// Bits	6-8
			u64	ss_ad_enable:1;			// Bit	9
			u64 reserved:2;				// Bits	10-11
			u64 sspt_ptr:52;			// Bits	12-63
		};
		u64 value;
	}stage2_lo;
	union
	{
		struct
		{
			u64 domain_id:16;			// Bits	64-79
			u64 reserved1:7;			// Bits	80-86
			u64 page_walk_snoop:1;		// Bit	87
			u64 page_snoop:1;			// Bit	88
			u64 cache_disable:1;		// Bit	89
			u64 ext_memtype_enable:1;	// Bit	90
			u64 reserved2:5;			// Bits	91-95
			u64 pat0:3;					// Bits	96-98
			u64 par0:1;					// Bit	99
			u64 pat1:3;					// Bits	100-102
			u64 par1:1;					// Bit	103
			u64 pat2:3;					// Bits	104-106
			u64 par2:1;					// Bit	107
			u64 pat3:3;					// Bits	108-110
			u64 par3:1;					// Bit	111
			u64 pat4:3;					// Bits	112-114
			u64 par4:1;					// Bit	115
			u64 pat5:3;					// Bits	116-118
			u64 par5:1;					// Bit	119
			u64 pat6:3;					// Bits	120-122
			u64 par6:1;					// Bit	123
			u64 pat7:3;					// Bits	124-126
			u64 par7:1;					// Bit	127
		};
		u64 value;
	}stage2_hi;
	union
	{
		struct
		{
			u64 supervisor_req_en:1;	// Bit	128
			u64 execute_req_en:1;		// Bit	129
			u64 paging_mode:2;			// Bits	130-131
			u64 write_protect_enable:1;	// Bit	132
			u64 no_execute_enable:1;	// Bit	133
			u64 smep_enable:1;			// Bit	134
			u64 ext_a_flag_enable:1;	// Bit	135
			u64 reserved:4;				// Bits	136-139
			u64 fspt_ptr:52;			// Bits	140-191
		};
		u64 value;
	}stage1_lo;
	union
	{
		u64 reserved;
		u64 value;
	}stage1_hi;
	union
	{
		u64 reserved;
		u64 value;
	}v5;
	union
	{
		u64 reserved;
		u64 value;
	}v6;
	union
	{
		u64 reserved;
		u64 value;
	}v7;
	union
	{
		u64 reserved;
		u64 value;
	}v8;
}intel_iommu_scalable_pasid_table_entry,*intel_iommu_scalable_pasid_table_entry_p;

typedef union _intel_iommu_stage1_pml5e
{
	struct
	{
		u64 present:1;				// Bit	0
		u64 write:1;				// Bit	1
		u64 user:1;					// Bit	2
		u64 ignored1:2;				// Bits	3-4
		u64 accessed:1;				// Bit	5
		u64 ignored2:1;				// Bit	6
		u64 reserved1:1;			// Bit	7
		u64 ignored3:2;				// Bits	8-9
		u64 ext_accessed:1;			// Bit	10
		u64 ignored4:1;				// Bit	11
		u64 pml4_ptr:40;			// Bits	12-51
		u64 ignored5:11;			// Bits	52-62
		u64 no_execute:1;			// Bit	63
	};
	u64 value;
}intel_iommu_stage1_pml5e,*intel_iommu_stage1_pml5e_p;

typedef union _intel_iommu_stage1_pml4e
{
	struct
	{
		u64 present:1;				// Bit	0
		u64 write:1;				// Bit	1
		u64 user:1;					// Bit	2
		u64 ignored1:2;				// Bits	3-4
		u64 accessed:1;				// Bit	5
		u64 ignored2:1;				// Bit	6
		u64 reserved1:1;			// Bit	7
		u64 ignored3:2;				// Bits	8-9
		u64 ext_accessed:1;			// Bit	10
		u64 ignored4:1;				// Bit	11
		u64 pdpt_ptr:40;			// Bits	12-51
		u64 ignored5:11;			// Bits	52-62
		u64 no_execute:1;			// Bit	63
	};
	u64 value;
}intel_iommu_stage1_pml4e,*intel_iommu_stage1_pml4e_p;

typedef union _intel_iommu_stage1_pdpte
{
	struct
	{
		u64 present:1;				// Bit	0
		u64 write:1;				// Bit	1
		u64 user:1;					// Bit	2
		u64 ignored1:2;				// Bits	3-4
		u64 accessed:1;				// Bit	5
		u64 ignored2:1;				// Bit	6
		u64 huge_page:1;			// Bit	7
		u64 ignored3:2;				// Bits	8-9
		u64 ext_accessed:1;			// Bit	10
		u64 ignored4:1;				// Bit	11
		u64 pde_ptr:40;				// Bits	12-51
		u64 ignored5:11;			// Bits	52-62
		u64 no_execute:1;			// Bit	63
	};
	u64 value;
}intel_iommu_stage1_pdpte,*intel_iommu_stage1_pdpte_p;

typedef union _intel_iommu_stage1_huge_pdpte
{
	struct
	{
		u64 present:1;				// Bit	0
		u64 write:1;				// Bit	1
		u64 user:1;					// Bit	2
		u64 pwt:1;					// Bit	3
		u64 pcd:1;					// Bit	4
		u64 accessed:1;				// Bit	5
		u64 dirty:1;				// Bit	6
		u64 huge_page:1;			// Bit	7
		u64 global:1;				// Bit	8
		u64 ignored1:1;				// Bit	9
		u64 ext_accessed:1;			// Bit	10
		u64 ignored2:1;				// Bit	11
		u64 pat:1;					// Bit	12
		u64 reserved1:17;			// Bits	13-29
		u64 page_ptr:22;			// Bits	30-51
		u64 ignored3:11;			// Bits	52-62
		u64 no_execute:1;			// Bit	63
	};
	u64 value;
}intel_iommu_stage1_huge_pdpte,*intel_iommu_stage1_huge_pdpte_p;

typedef union _intel_iommu_stage1_pde
{
	struct
	{
		u64 present:1;				// Bit	0
		u64 write:1;				// Bit	1
		u64 user:1;					// Bit	2
		u64 ignored1:2;				// Bits	3-4
		u64 accessed:1;				// Bit	5
		u64 ignored2:1;				// Bit	6
		u64 large_page:1;			// Bit	7
		u64 ignored3:2;				// Bits	8-9
		u64 ext_accessed:1;			// Bit	10
		u64 ignored4:1;				// Bit	11
		u64 pte_ptr:40;				// Bits	12-51
		u64 ignored5:11;			// Bits	52-62
		u64 no_execute:1;			// Bit	63
	};
	u64 value;
}intel_iommu_stage1_pde,*intel_iommu_stage1_pde_p;

typedef union _intel_iommu_stage1_large_pde
{
	struct
	{
		u64 present:1;				// Bit	0
		u64 write:1;				// Bit	1
		u64 user:1;					// Bit	2
		u64 pwt:1;					// Bit	3
		u64 pcd:1;					// Bit	4
		u64 accessed:1;				// Bit	5
		u64 dirty:1;				// Bit	6
		u64 large_page:1;			// Bit	7
		u64 global:1;				// Bit	8
		u64 ignored1:1;				// Bit	9
		u64 ext_accessed:1;			// Bit	10
		u64 ignored2:1;				// Bit	11
		u64 pat:1;					// Bit	12
		u64 reserved1:8;			// Bits	13-20
		u64 page_ptr:31;			// Bits	21-51
		u64 ignored3:11;			// Bits	52-62
		u64 no_execute:1;			// Bit	63
	};
	u64 value;
}intel_iommu_stage1_large_pde,*intel_iommu_stage1_lareg_pde_p;

typedef union _intel_iommu_stage1_pte
{
	struct
	{
		u64 present:1;				// Bit	0
		u64 write:1;				// Bit	1
		u64 user:1;					// Bit	2
		u64 pwt:1;					// Bit	3
		u64 pcd:1;					// Bit	4
		u64 accessed:1;				// Bit	5
		u64 dirty:1;				// Bit	6
		u64 pat:1;					// Bit	7
		u64 global:1;				// Bit	8
		u64 ignored1:1;				// Bit	9
		u64 ext_accessed:1;			// Bit	10
		u64 ignored2:1;				// Bit	11
		u64 page_ptr:40;			// Bits	12-51
		u64 ignored3:11;			// Bits	52-62
		u64 no_execute:1;			// Bit	63
	};
	u64 value;
}intel_iommu_stage1_pte,*intel_iommu_stage1_pte_p;

typedef union _intel_iommu_stage2_pml5e
{
	struct
	{
		u64 read:1;					// Bit	0
		u64 write:1;				// Bit	1
		u64 execute:1;				// Bit	2
		u64 reserved1:5;			// Bits	3-7
		u64 accessed:1;				// Bit	8
		u64 reserved2:3;			// Bits	9-11
		u64 pml4_ptr:40;			// Bits	12-51
		u64 reserved3:12;			// Bits	52-63
	};
	u64 value;
}intel_iommu_stage2_pml5e,*intel_iommu_stage2_pml5e_p;

typedef union _intel_iommu_stage2_pml4e
{
	struct
	{
		u64 read:1;					// Bit	0
		u64 write:1;				// Bit	1
		u64 execute:1;				// Bit	2
		u64 reserved1:5;			// Bits	3-7
		u64 accessed:1;				// Bit	8
		u64 reserved2:3;			// Bits	9-11
		u64 pdpt_ptr:40;			// Bits	12-51
		u64 reserved3:12;			// Bits	52-63
	};
	u64 value;
}intel_iommu_stage2_pml4e,*intel_iommu_stage2_pml4e_p;

typedef union _intel_iommu_stage2_huge_pdpte
{
	struct
	{
		u64 read:1;					// Bit	0
		u64 write:1;				// Bit	1
		u64 execute:1;				// Bit	2
		u64 memory_type:3;			// Bits	3-5
		u64 ignore_pat:1;			// Bit	6
		u64 huge_page:1;			// Bit	7
		u64 accessed:1;				// Bit	8
		u64 dirty:1;				// Bit	9
		u64 ignored1:1;				// Bit	10
		u64 snoop:1;				// Bit	11
		u64 reserved1:18;			// Bits	12-29
		u64 page_ptr:22;			// Bits	30-51
		u64 reserved2:12;			// Bits	52-63
	};
	u64 value;
}intel_iommu_stage2_huge_pdpte,*intel_iommu_stage2_huge_pdpte_p;

typedef union _intel_iommu_stage2_pdpte
{
	struct
	{
		u64 read:1;					// Bit	0
		u64 write:1;				// Bit	1
		u64 execute:1;				// Bit	2
		u64 reserved1:4;			// Bits	3-6
		u64 huge_page:1;			// Bit	7
		u64 accessed:1;				// Bit	8
		u64 reserved2:3;			// Bits	9-11
		u64 pde_ptr:40;				// Bits	12-51
		u64 reserved3:12;			// Bits	52-63
	};
	u64 value;
}intel_iommu_stage2_pdpte,*intel_iommu_stage2_pdpte_p;

typedef union _intel_iommu_stage2_large_pde
{
	struct
	{
		u64 read:1;					// Bit	0
		u64 write:1;				// Bit	1
		u64 execute:1;				// Bit	2
		u64 memory_type:3;			// Bits	3-5
		u64 ignore_pat:1;			// Bit	6
		u64 large_page:1;			// Bit	7
		u64 accessed:1;				// Bit	8
		u64 dirty:1;				// Bit	9
		u64 ignored1:1;				// Bit	10
		u64 snoop:1;				// Bit	11
		u64 reserved1:9;			// Bits	12-20
		u64 page_ptr:31;			// Bits	21-51
		u64 reserved2:12;			// Bits	52-63
	};
	u64 value;
}intel_iommu_stage2_large_pde,*intel_iommu_stage2_large_pde_p;

typedef union _intel_iommu_stage2_pde
{
	struct
	{
		u64 read:1;					// Bit	0
		u64 write:1;				// Bit	1
		u64 execute:1;				// Bit	2
		u64 reserved1:4;			// Bits	3-6
		u64 large_page:1;			// Bit	7
		u64 accessed:1;				// Bit	8
		u64 reserved2:3;			// Bits	9-11
		u64 pte_ptr:40;				// Bits	12-51
		u64 reserved3:12;			// Bits	52-63
	};
	u64 value;
}intel_iommu_stage2_pde,*intel_iommu_stage2_pde_p;

typedef union _intel_iommu_stage2_pte
{
	struct
	{
		u64 read:1;					// Bit	0
		u64 write:1;				// Bit	1
		u64 execute:1;				// Bit	2
		u64 memory_type:3;			// Bits	3-5
		u64 ignore_pat:1;			// Bit	6
		u64 ignored0:1;				// Bit	7
		u64 accessed:1;				// Bit	8
		u64 dirty:1;				// Bit	9
		u64 ignored1:1;				// Bit	10
		u64 snoop:1;				// Bit	11
		u64 page_ptr:40;			// Bits	12-51
		u64 reserved1:12;			// Bits	52-63
	};
	u64 value;
}intel_iommu_stage2_pte,*intel_iommu_stage2_pte_p;

typedef struct _intel_iommu_remapped_intremap_entry
{
	union
	{
		struct
		{
			u64 present:1;				// Bit	0
			u64 fault_proc_disable:1;	// Bit	1
			u64 destination_mode:1;		// Bit	2
			u64 redirection_hint:1;		// Bit	3
			u64 trigger_mode:1;			// Bit	4
			u64 delivery_mode:3;		// Bits	5-7
			u64 available:4;			// Bits	8-11
			u64 reserved1:3;			// Bits	12-14
			u64 intremap_mode:1;		// Bit	15
			u64 vector:8;				// Bits	16-23
			u64 reserved2:8;			// Bits	24-31
			u64 destination_id:32;		// Bits	32-63
		};
		u64 value;
	}lo;
	union
	{
		struct
		{
			u64 source_id:16;			// Bits	64-79
			u64 sid_qualifier:2;		// Bits	80-81
			u64 src_validation_type:2;	// Bits	82-83
			u64 reserved:44;			// Bits	84-127
		};
		u64 value;
	}hi;
}intel_iommu_remapped_intremap_entry,*intel_iommu_remapped_intremap_entry_p;

#define intel_iommu_intremap_destination_physical		0
#define intel_iommu_intremap_destination_logical		1

#define intel_iommu_intremap_redirection_specific		0
#define intel_iommu_intremap_redirection_any			1

#define intel_iommu_intremap_trigger_edge				0
#define intel_iommu_intremap_trigger_level				1

#define intel_iommu_intremap_deliver_fixed				0
#define intel_iommu_intremap_deliver_lowest_prio		1
#define intel_iommu_intremap_deliver_smi				2
#define intel_iommu_intremap_deliver_nmi				4
#define intel_iommu_intremap_deliver_init				5
#define intel_iommu_intremap_deliver_extint				7

#define intel_iommu_intremap_irte_remapped				0
#define intel_iommu_intremap_irte_posted				1

#define intel_iommu_intremap_sq_verify_all				0
#define intel_iommu_intremap_sq_verify_msb13_lsb2		1
#define intel_iommu_intremap_sq_verify_msb13_lsb1		2
#define intel_iommu_intremap_sq_verify_msb13_lsb0		3

#define intel_iommu_intremap_svt_no_verify				0
#define intel_iommu_intremap_svt_verify_sid_sq			1
#define intel_iommu_intremap_svt_verify_bus				2

typedef struct _intel_iommu_posted_intremap_entry
{
	union
	{
		struct
		{
			u64 present:1;				// Bit	0
			u64 fault_proc_disable:1;	// Bit	1
			u64 reserved1:6;			// Bits	2-7
			u64 available:4;			// Bits	8-11
			u64 reserved2:2;			// Bits	12-13
			u64 urgent:1;				// Bit	14
			u64 intremap_mode:1;		// Bit	15
			u64 virtual_vector:8;		// Bits	16-23
			u64 reserved:14;			// Bits	24-37
			u64 posted_descaddr_lo:26;	// Bits	38-63
		};
		u64 value;
	}lo;
	union
	{
		struct
		{
			u64 source_id:16;			// Bits	64-79
			u64 sid_qualifier:2;		// Bits	80-81
			u64 src_validation_type:2;	// Bits	82-83
			u64 reserved:12;			// Bits	84-95
			u64 posted_descaddr_hi:32;	// Bits	96-127
		};
		u64 value;
	}hi;
}intel_iommu_posted_intremap_entry,*intel_iommu_posted_intremap_entry_p;

typedef struct _intel_iommu_posted_interrupt_descriptor
{
	u32 posted_interrupt_requests[8];			// Bits	0-255
	union
	{
		struct
		{
			u32 outstanding_notification:1;		// Bit	256
			u32 suppress_notification:1;		// Bit	257
			u32 reserved1:14;					// Bits	258-271
			u32 notification_vector:8;			// Bits	272-279
			u32 reserved2:8;					// Bits	280-287
		};
		u32 value;
	}nofication_control;
	union
	{
		struct
		{
			u32 reserved1:8;					// Bits	288-295
			u32 destination_id:8;				// Bits	296-303
			u32 reserved2:16;					// Bits	304-319
		}apic;
		struct
		{
			u32 destination_id:32;				// Bits	288-319
		}x2apic;
		u32 value;
	}notification_destination;
	u32 reserved[6];							// Bits	320-511
}intel_iommu_posted_interrupt_descriptor,*intel_iommu_posted_interrupt_descriptor_p;

// Defined below are Intel VT-d Registers.
// See Chapter 11 "Register Descriptions", Intel(R) Virtualization Technology for Directed I/O (June 2022)

// Here lists the MMIO offsets of Intel VT-d Hardware
#define mmio_offset_version					0x000
#define mmio_offset_capability				0x008
#define mmio_offset_extended_capability		0x010
#define mmio_offset_global_command			0x018
#define mmio_offset_global_status			0x01C
#define mmio_offset_root_table_ptr			0x020
#define mmio_offset_context_command			0x028
#define mmio_offset_fault_status			0x034
#define mmio_offset_fault_control			0x038
#define mmio_offset_fault_data				0x03C
#define mmio_offset_fault_address_lo		0x040
#define mmio_offset_fault_address_hi		0x044
#define mmio_offset_protected_memory_enable	0x064
#define mmio_offset_protected_base_lo		0x068
#define mmio_offset_protected_limit_lo		0x06C
#define mmio_offset_protected_base_hi		0x070
#define mmio_offset_protected_limit_hi		0x078
#define mmio_offset_inval_queue_head		0x080
#define mmio_offset_inval_queue_tail		0x088
#define mmio_offset_inval_queue_address		0x090
#define mmio_offset_inval_cpl_status		0x09C
#define mmio_offset_inval_cpl_evt_ctrl		0x0A0
#define mmio_offset_inval_cpl_evt_data		0x0A4
#define mmio_offset_inval_cpl_evt_addr_lo	0x0A8
#define mmio_offset_inval_cpl_evt_addr_hi	0x0AC
#define mmio_offset_inval_queue_err_record	0x0B0
#define mmio_offset_intremap_table_ptr		0x0B8
#define mmio_offset_page_req_queue_head		0x0C0
#define mmio_offset_page_req_queue_tail		0x0C8
#define mmio_offset_page_req_queue_ptr		0x0D0
#define mmio_offset_page_req_status			0x0DC
#define mmio_offset_page_req_evt_ctrl		0x0E0
#define mmio_offset_page_req_evt_data		0x0E4
#define mmio_offset_page_req_evt_addr_lo	0x0E8
#define mmio_offset_page_req_evt_addr_hi	0x0EC
#define mmio_offset_mtrr_cap				0x100
#define mmio_offset_mtrr_def_type			0x108
#define mmio_offset_mtrr_fixed_00000		0x120
#define mmio_offset_mtrr_fixed_80000		0x128
#define mmio_offset_mtrr_fixed_a0000		0x130
#define mmio_offset_mtrr_fixed_c0000		0x138
#define mmio_offset_mtrr_fixed_c8000		0x140
#define mmio_offset_mtrr_fixed_d0000		0x148
#define mmio_offset_mtrr_fixed_d8000		0x150
#define mmio_offset_mtrr_fixed_e0000		0x158
#define mmio_offset_mtrr_fixed_e8000		0x160
#define mmio_offset_mtrr_fixed_f0000		0x168
#define mmio_offset_mtrr_fixed_f8000		0x170
#define mmio_offset_mtrr_var_base0			0x180
#define mmio_offset_mtrr_var_mask0			0x188
#define mmio_offset_mtrr_var_base1			0x190
#define mmio_offset_mtrr_var_mask1			0x198
#define mmio_offset_mtrr_var_base2			0x1A0
#define mmio_offset_mtrr_var_mask2			0x1A8
#define mmio_offset_mtrr_var_base3			0x1B0
#define mmio_offset_mtrr_var_mask3			0x1B8
#define mmio_offset_mtrr_var_base4			0x1C0
#define mmio_offset_mtrr_var_mask4			0x1C8
#define mmio_offset_mtrr_var_base5			0x1D0
#define mmio_offset_mtrr_var_mask5			0x1D8
#define mmio_offset_mtrr_var_base6			0x1E0
#define mmio_offset_mtrr_var_mask6			0x1E8
#define mmio_offset_mtrr_var_base7			0x1F0
#define mmio_offset_mtrr_var_mask7			0x1F8
#define mmio_offset_mtrr_var_base8			0x200
#define mmio_offset_mtrr_var_mask8			0x208
#define mmio_offset_mtrr_var_base9			0x210
#define mmio_offset_mtrr_var_mask9			0x218
#define mmio_offset_perfmon_cap				0x300
#define mmio_offset_perfmon_config_offset	0x310
#define mmio_offset_perfmon_freeze_offset	0x314
#define mmio_offset_perfmon_overflow_offset	0x318
#define mmio_offset_perfmon_counter_offset	0x31C
#define mmio_offset_perfmon_int_status		0x324
#define mmio_offset_perfmon_int_ctrl		0x328
#define mmio_offset_perfmon_int_data		0x32C
#define mmio_offset_perfmon_int_addr_lo		0x330
#define mmio_offset_perfmon_int_addr_hi		0x334
#define mmio_offset_perfmon_int_evt_cap(n)	(0x380+(n<<3))
#define mmio_offset_enhanced_cmd			0x400
#define mmio_offset_enhanced_cmd_ext_op		0x408
#define mmio_offset_enhanced_cmd_response	0x410
#define mmio_offset_enhanced_status			0x420
#define mmio_offset_enhanced_cmd_cap		0x430
#define mmio_offset_virt_cmd				0xE00
#define mmio_offset_virt_cmd_ext_op			0xE08
#define mmio_offset_virt_cmd_response		0xE10
#define mmio_offset_virt_cmd_cap			0xE30

#define mmio_offset_iotlb_inval_addr		0x0
#define mmio_offset_iotlb_inval_cmd			0x8

typedef union _intel_iommu_version_register
{
	struct
	{
		u32 minor:4;				// Bits	0-3
		u32 major:4;				// Bits	4-7
		u32 reserved:24;			// Bits	8-31
	};
	u32 value;
}intel_iommu_version_register,*intel_iommu_version_register_p;

typedef union _intel_iommu_capability_register
{
	struct
	{
		u64 supported_domains:3;		// Bits	0-2
		u64 reserved1:1;				// Bit	3
		u64 required_wb_flushing:1;		// Bit	4
		u64 protected_lo_mem:1;			// Bit	5
		u64 protected_hi_mem:1;			// Bit	6
		u64 caching_mode:1;				// Bit	7
		u64 reserved2:1;				// Bit	8
		u64 support_39bit_agaw:1;		// Bit	9
		u64 support_48bit_agaw:1;		// Bit	10
		u64 support_57bit_agaw:1;		// Bit	11
		u64 reserved3:1;				// Bit	12
		u64 reserved4:3;				// Bits	13-15
		u64 maximum_gaw:6;				// Bits	16-21
		u64 zero_length_read:1;			// Bit	22
		u64 deprecated:1;				// Bit	23
		u64 fault_record_offset:10;		// Bits	24-33
		u64 support_ss_large_page:1;	// Bit	34
		u64 support_ss_huge_page:1;		// Bit	35
		u64 reserved5:2;				// Bits	36-37
		u64 reserved6:1;				// Bit	38
		u64 page_selective_inval:1;		// Bit	39
		u64 fault_record_count:8;		// Bits	40-47
		u64 maximum_address_mask:6;		// Bits	48-53
		u64 write_draining:1;			// Bit	54
		u64 read_draining:1;			// Bit	55
		u64 support_fs_huge_page:1;		// Bit	56
		u64 reserved7:2;				// Bits	57-58
		u64 support_posted_int:1;		// Bit	59
		u64 support_fs_la57:1;			// Bit	60
		u64 support_enhanced_cmd:1;		// Bit	61
		u64 support_enhanced_intremap:1;// Bit	62
		u64 support_enhanced_root_ptr:1;// Bit	63
	};
	u64 value;
}intel_iommu_capability_register,*intel_iommu_capability_register_p;

typedef union _intel_iommu_extcap_register
{
	struct
	{
		u64 page_walk_coherency:1;			// Bit	0
		u64 support_queued_inval:1;			// Bit	1
		u64 support_device_tlb:1;			// Bit	2
		u64 support_intremap:1;				// Bit	3
		u64 support_ext_int_mode:1;			// Bit	4
		u64 deprecated1:1;					// Bit	5
		u64 support_pass_through:1;			// Bit	6
		u64 snoop_control:1;				// Bit	7
		u64 iotlb_offset:10;				// Bits	8-17
		u64 reserved1:2;					// Bits	18-19
		u64 maximum_handle_mask:4;			// Bits	20-23
		u64 deprecated2:1;					// Bit	24
		u64 support_memory_type:1;			// Bit	25
		u64 support_nested_translation:1;	// Bit	26
		u64 reserved2:1;					// Bit	27
		u64 deprecated3:1;					// Bit	28
		u64 support_page_request:1;			// Bit	29
		u64 support_execute_request:1;		// Bit	30
		u64 support_supervisor_request:1;	// Bit	31
		u64 reserved3:1;					// Bit	32
		u64 support_no_write_flag:1;		// Bit	33
		u64 support_ext_accessed_flag:1;	// Bit	34
		u64 supported_pasid_size:5;			// Bits	35-39
		u64 support_pasid:1;				// Bit	40
		u64 device_tlb_inval_throttle:1;	// Bit	41
		u64 support_page_request_drain:1;	// Bit	42
		u64 support_scalable_mode:1;		// Bit	43
		u64 support_virt_cmd:1;				// Bit	44
		u64 support_ss_ad_bits:1;			// Bit	45
		u64 support_ss_scalable:1;			// Bit	46
		u64 support_fs_translation:1;		// Bit	47
		u64 scalable_coherency:1;			// Bit	48
		u64 support_rid_pasid:1;			// Bit	49
		u64 reserved4:1;					// Bit	50
		u64 support_perfmon:1;				// Bit	51
		u64 support_abort_dma_mode:1;		// Bit	52
		u64 support_rid_priv:1;				// Bit	53
		u64 reserved5:4;					// Bits	54-57
		u64 support_stop_marker:1;			// Bit	58
		u64 reserved6:5;					// Bits	59-63
	};
	u64 value;
}intel_iommu_extcap_register,*intel_iommu_extcap_register_p;

typedef union _intel_iommu_global_command_register
{
	struct
	{
		u32 reserved:23;					// Bits	0-22
		u32 compat_format_interrupt:1;		// Bit	23
		u32 set_intremap_ptr:1;				// Bit	24
		u32 enable_intremap:1;				// Bit	25
		u32 enable_queue_inval:1;			// Bit	26
		u32 flush_write_buffer:1;			// Bit	27
		u32 reserved2:2;					// Bits	28-29
		u32 set_root_ptr:1;					// Bit	30
		u32 enable_translation:1;			// Bit	31
	};
	u32 value;
}intel_iommu_global_command_register,*intel_iommu_global_command_register_p;

typedef union _intel_iommu_global_status_register
{
	struct
	{
		u32 reserved:23;					// Bits	0-22
		u32 compat_format_int_processed:1;	// Bit	23
		u32 intremap_table_status:1;		// Bit	24
		u32 intremap_table_enabled:1;		// Bit	25
		u32 queued_inval_enabled:1;			// Bit	26
		u32 write_buffer_flush_status:1;	// Bit	27
		u32 reserved2:2;					// Bits	28-29
		u32 root_ptr_status:1;				// Bit	30
		u32 translation_enabled:1;			// Bit	31
	};
	u32 value;
}intel_iommu_global_status_register,*intel_iommu_global_status_register_p;

#define intel_iommu_global_translation_enable			31
#define intel_iommu_global_set_root_ptr					30
#define intel_iommu_global_write_buffer_flush			27
#define intel_iommu_global_queue_invalidation_enable	26
#define intel_iommu_global_intremap_enable				25
#define intel_iommu_global_set_intremap_ptr				24
#define intel_iommu_global_compat_format_interrupt		23

typedef union _intel_iommu_root_table_address_register
{
	struct
	{
		u64 reserved:10;				// Bits	0-9
		u64 translation_table_mode:2;	// Bits	10-11
		u64 root_table_ptr:52;			// Bits	12-63
	};
	u64 value;
}intel_iommu_root_table_address_register,*intel_iommu_root_table_address_register_p;

#define intel_iommu_translation_table_mode_legacy		0
#define intel_iommu_translation_table_mode_scalable		1
#define intel_iommu_translation_table_mode_abort_dma	3

typedef union _intel_iommu_context_command_register
{
	struct
	{
		u64 domain_id:16;				// Bits	0-15
		u64 source_id:16;				// Bits	16-31
		u64 function_mask:2;			// Bits	32-33
		u64 reserved:25;				// Bits	34-58
		u64 inval_granularity:2;		// Bits	59-60
		u64 request_granularity:2;		// Bits	61-62
		u64 inval_context_cache:1;		// Bit	63
	};
	u64 value;
}intel_iommu_context_command_register,*intel_iommu_context_command_register_p;

#define intel_iommu_context_inval_global		1
#define intel_iommu_context_inval_domain		2
#define intel_iommu_context_inval_device		3

typedef union _intel_iommu_iotlb_inval_cmd_register
{
	struct
	{
		u64 reserved1:32;				// Bits	0-31
		u64 domain_id:16;				// Bits	32-47
		u64 drain_write:1;				// Bit	48
		u64 drain_read:1;				// Bit	49
		u64 reserved2:7;				// Bits	50-56
		u64 inval_granularity:2;		// Bits	57-58
		u64 reserved3:1;				// Bit	59
		u64 request_granularity:2;		// Bits	60-61
		u64 reserved4:1;				// Bit	62
		u64 inval_iotlb:1;				// Bit	63
	};
	u64 value;
}intel_iommu_iotlb_inval_cmd_register,*intel_iommu_iotlb_inval_cmd_register_p;

#define intel_iommu_iotlb_inval_global			1
#define intel_iommu_iotlb_inval_domain			2
#define intel_iommu_iotlb_inval_page			3

typedef union _intel_iommu_iotlb_inval_addr_register
{
	struct
	{
		u64 addr_mask:6;				// Bits	0-5
		u64 inval_hint:1;				// Bit	6
		u64 reserved:5;					// Bits	7-11
		u64 address:52;					// Bits	12-63
	};
	u64 value;
}intel_iommu_iotlb_inval_addr_register,*intel_iommu_iotlb_addr_register_p;

typedef union _intel_iommu_fault_status_register
{
	struct
	{
		u32 primary_fault_overflow:1;		// Bit	0
		u32 primary_pending_fault:1;		// Bit	1
		u32 reserved1:2;					// Bits	2-3
		u32 inval_queue_error:1;			// Bit	4
		u32 inval_cpl_error:1;				// Bit	5
		u32 inval_timout_error:1;			// Bit	6
		u32 deprecated:1;					// Bit	7
		u32 fault_record_index:8;			// Bits	8-15
		u32 reserved2:16;					// Bits	16-31
	};
	u32 value;
}intel_iommu_fault_status_register,*intel_iommu_fault_status_register_p;

typedef struct _intel_iommu_fault_record_register
{
	union
	{
		struct
		{
			u64 reserved:12;				// Bits	0-11
			u64 fault_info:52;				// Bits 12-63
		};
		u64 value;
	}lo;
	union
	{
		struct
		{
			u64 source_id:16;				// Bits	64-79
			u64 reserved:12;				// Bits	80-91
			u64 type_b2:1;					// Bit	92
			u64 req_privilege:1;			// Bit	93
			u64 req_execute:1;				// Bit	94
			u64 pasid_present:1;			// Bit	95
			u64 fault_reason:8;				// Bits	96-103
			u64 pasid_value:20;				// Bits	104-123
			u64 address_type:2;				// Bits	124-125
			u64 type_b1:1;					// Bit	126
			u64 fault:1;					// Bit	127
		};
		u64 value;
	}hi;
}intel_iommu_fault_record_register,*intel_iommu_fault_record_register_p;

// Note: Interrupts (e.g.: Fault Event) generated by Intel VT-d generally
// follows the Message-Signaled Interrupt (MSI) mechanism.
// NoirVisor avoids repeating the definitions for all IOMMU's interrupts.

typedef union _intel_iommu_msi_ctrl_register
{
	struct
	{
		u32 reserved:30;		// Bits	0-29
		u32 int_pending:1;		// Bit	30
		u32 int_mask:1;			// Bit	31
	};
	u32 value;
}intel_iommu_msi_ctrl_register,*intel_iommu_msi_ctrl_register_p;

typedef union _intel_iommu_msi_data_register
{
	struct
	{
		u32 vector:8;			// Bits	0-7
		u32 delivery_mode:1;	// Bit	8
		u32 reserved:23;		// Bits	9-31
	};
	u32 value;
}intel_iommu_msi_data_register,*intel_iommu_msi_data_register_p;

typedef union _intel_iommu_msi_addr_lo_register
{
	struct
	{
		u32 reserved1:2;		// Bits	0-1
		u32 dest_mode:1;		// Bit	2
		u32 redir_hint:1;		// Bit	3
		u32 reserved2:1;		// Bits	4-11
		u32 dest_id:8;			// Bits	12-19
		u32 signature:12;		// Bits	20-31
	};
	u32 value;
}intel_iommu_msi_addr_lo_register,*intel_iommu_msi_addr_lo_register_p;

typedef union _intel_iommu_msi_addr_hi_register
{
	struct
	{
		u32 reserved:8;			// Bits	0-7
		u32 dest_id_hi:24;		// Bits	8-31
	};
	u32 value;
}intel_iommu_msi_addr_hi_register,*intel_iommu_msi_addr_hi_register_p;

typedef union _intel_iommu_inval_queue_head_register
{
	struct
	{
		u64 reserved1:4;
		u64 head:15;
		u64 reserved2:45;
	};
	u64 value;
}intel_iommu_inval_queue_head_register,*intel_iommu_inval_queue_head_register_p;

typedef union _intel_iommu_inval_queue_tail_register
{
	struct
	{
		u64 reserved1:4;
		u64 tail:15;
		u64 reserved2:45;
	};
	u64 value;
}intel_iommu_inval_queue_tail_register,*intel_iommu_inval_queue_tail_register_p;

typedef union _intel_iommu_inval_queue_address_register
{
	struct
	{
		u64 queue_size:3;
		u64 reserved:8;
		u64 width:1;
		u64 base:52;
	};
	u64 value;
}intel_iommu_inval_queue_address_register,*intel_iommu_inval_queue_address_register_p;

typedef union _intel_iommu_queued_inval_legacy_descriptor
{
	struct
	{
		union
		{
			struct
			{
				u64 type_lo:4;
				u64 granularity:2;
				u64 reserved1:3;
				u64 type_hi:3;
				u64 reserved2:4;
				u64 domain_id:16;
				u64 source_id:16;
				u64 function_mask:2;
				u64 reserved3:14;
			};
			u64 lo;
		};
		u64 hi;
	}context_cache;
	struct
	{
		union
		{
			struct
			{
				u64 type_lo:4;
				u64 granularity:2;
				u64 drain_write:1;
				u64 drain_read:1;
				u64 reserved1:1;
				u64 type_hi:3;
				u64 reserved2:4;
				u64 domain_id:16;
				u64 reserved3:32;
			};
			u64 lo;
		};
		union
		{
			struct
			{
				u64 address_mask:6;
				u64 inval_hint:1;
				u64 reserved4:5;
				u64 address:52;
			};
			u64 hi;
		};
	}iotlb;
	struct
	{
		union
		{
			struct
			{
				u64 type_lo:4;
				u64 reserved1:5;
				u64 type_hi:3;
				u64 pfsid_lo:4;
				u64 mip:5;
				u64 reserved2:11;
				u64 source_id:16;
				u64 reserved3:4;
				u64 pfsid_hi:12;
			};
			u64 lo;
		};
		union
		{
			struct
			{
				u64 size:1;
				u64 reserved4:11;
				u64 address:52;
			};
			u64 hi;
		};
	}device_tlb;
	struct
	{
		union
		{
			struct
			{
				u64 type_lo:4;
				u64 granularity:1;
				u64 reserved1:4;
				u64 type_hi:3;
				u64 reserved2:15;
				u64 index_mask:5;
				u64 interrupt_index:16;
				u64 reserved3:16;
			};
			u64 lo;
		};
		u64 hi;
	}intentry_cache;
	struct
	{
		union
		{
			struct
			{
				u64 type_lo:4;
				u64 interrupt_flag:1;
				u64 status_write:1;
				u64 fence_flag:1;
				u64 pagereq_drain:1;
				u64 reserved1:1;
				u64 type_hi:3;
				u64 reserved2:20;
				u64 status_data:32;
			};
			u64 lo;
		};
		union
		{
			struct
			{
				u64 reserved3:2;
				u64 status_address:62;
			};
			u64 hi;
		};
	}inval_wait;
}intel_iommu_queued_inval_legacy_descriptor,*intel_iommu_queued_inval_legacy_descriptor_p;

typedef union _intel_iommu_queued_inval_pasid_descriptor
{
	struct
	{
		struct
		{
			u64 type_lo:4;
			u64 granularity:2;
			u64 reserved1:3;
			u64 type_hi:3;
			u64 reserved2:4;
			u64 domain_id:16;
			u64 pasid:20;
			u64 reserved3:12;
		};
		u64 reserved[3];
	}context_cache;
	struct
	{
		struct
		{
			u64 type_lo:4;
			u64 granularity:2;
			u64 reserved1:3;
			u64 type_hi:3;
			u64 reserved2:4;
			u64 domain_id:16;
			u64 pasid:20;
			u64 reserved3:12;
		};
		struct
		{
			u64 address_mask:6;
			u64 inval_hint:1;
			u64 reserved4:5;
			u64 address:52;
		};
		u64 reserved[2];
	}iotlb;
	struct
	{
		struct
		{
			u64 type_lo:4;
			u64 mip:5;
			u64 type_hi:3;
			u64 pfsid_lo:4;
			u64 source_id:16;
			u64 pasid:20;
			u64 pfsid_hi:12;
		};
		struct
		{
			u64 global:1;
			u64 reserved4:11;
			u64 size:1;
			u64 address:52;
		};
		u64 reserved[2];
	}device_tlb;
	struct
	{
		struct
		{
			u64 type_lo:4;
			u64 granularity:1;
			u64 reserved1:4;
			u64 type_hi:3;
			u64 reserved2:15;
			u64 index_mask:5;
			u64 interrupt_index:16;
			u64 reserved3:16;
		};
		u64 reserved[3];
	}intentry_cache;
	struct
	{
		struct
		{
			u64 type_lo:4;
			u64 interrupt_flag:1;
			u64 status_write:1;
			u64 fence_flag:1;
			u64 pagereq_drain:1;
			u64 reserved1:1;
			u64 type_hi:3;
			u64 reserved2:20;
			u64 status_data:32;
		};
		struct
		{
			u64 reserved3:2;
			u64 status_address:62;
		};
		u64 reserved[2];
	}inval_wait;
}intel_iommu_queued_inval_pasid_descriptor,*intel_iommu_queued_inval_pasid_descriptor_p;

#define intel_iommu_queued_inval_type_context_cache			1
#define intel_iommu_queued_inval_type_iotlb					2
#define intel_iommu_queued_inval_type_device_tlb			3
#define intel_iommu_queued_inval_type_intentry_cache		4
#define intel_iommu_queued_inval_type_inval_wait			5
#define intel_iommu_queued_inval_type_pasid_context_cache	6
#define intel_iommu_queued_inval_type_pasid_iotlb			7
#define intel_iommu_queued_inval_type_pasid_device_tlb		8