/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the basic driver of Intel EPT.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_ept.h
*/

#include <nvdef.h>

// Specify the purpose of mapping.
#define noir_ept_mapped_for_default			0
#define noir_ept_mapped_for_variable_mtrr	1
#define noir_ept_mapped_for_fixed_mtrr		2

// Specify the top address of mapped GPA.
#define noir_ept_top_address				0x7FFFFFFFFF

typedef union _ia32_addr_translator
{
	struct
	{
		u64 page_offset:12;
		u64 pte_offset:9;
		u64 pde_offset:9;
		u64 pdpte_offset:9;
		u64 pml4e_offset:9;
		u64 canonical:16;
	};
	u64 value;
}ia32_addr_translator,*ia32_addr_translator_p;

typedef union _ia32_ept_pointer
{
	struct
	{
		u64 memory_type:3;		// bits	0-2
		u64 walk_length:3;		// bits	3-5
		u64 dirty_flag:1;		// bit	6
		u64 enable_sss:1;		// bit	7
		u64 reserved:4;			// bits	8-11
		u64 pml4e_offset:52;	// bits	12-63
	};
	u64 value;
}ia32_ept_pointer,*ia32_ept_pointer_p;

typedef union _ia32_hlat_pointer
{
	struct
	{
		u64 reserved0:3;			// bits	0-2
		u64 pwt:1;				// bit	3
		u64 pcd:1;				// bit	4
		u64 reserved1:7;			// bits	5-11
		u64 pml4e_offset:52;	// bits	12-63
	};
	u64 value;
}ia32_hlat_pointer,*ia32_hlat_pointer_p;

typedef union _ia32_ept_pml4e
{
	struct
	{
		u64 read:1;
		u64 write:1;
		u64 execute:1;
		u64 reserved0:5;
		u64 accessed:1;
		u64 ignored0:1;
		u64 umx:1;
		u64 ignored1:1;
		u64 pdpte_offset:40;
		u64 reserved1:12;
	};
	u64 value;
}ia32_ept_pml4e,*ia32_ept_pml4e_p;

typedef union _ia32_ept_huge_pdpte
{
	struct
	{
		u64 read:1;
		u64 write:1;
		u64 execute:1;
		u64 memory_type:3;
		u64 ignore_pat:1;
		u64 huge_pdpte:1;		// This bit must be set.
		u64 accessed:1;
		u64 dirty:1;
		u64 umx:1;
		u64 ignored0:1;
		u64 reserved:18;
		u64 page_offset:22;
		u64 ignored1:8;
		u64 s_shadow_stack:1;
		u64 ignored2:2;
		u64 suppress_ve:1;
	};
	u64 value;
}ia32_ept_huge_pdpte,*ia32_ept_huge_pdpte_p;

typedef union _ia32_ept_pdpte
{
	struct
	{
		u64 read:1;
		u64 write:1;
		u64 execute:1;
		u64 reserved0:4;
		u64 huge_pdpte:1;		// This bit must be reset.
		u64 accessed:1;
		u64 ignored0:1;
		u64 umx:1;
		u64 ignored1:1;
		u64 pde_offset:40;
		u64 ignored2:12;
	};
	u64 value;
}ia32_ept_pdpte,*ia32_ept_pdpte_p;

typedef union _ia32_ept_large_pde
{
	struct
	{
		u64 read:1;
		u64 write:1;
		u64 execute:1;
		u64 memory_type:3;
		u64 ignore_pat:1;
		u64 large_pde:1;		// This bit must be set.
		u64 accessed:1;
		u64 dirty:1;
		u64 umx:1;
		u64 ignored0:1;			// Indicate if set for variable MTRR.
		u64 reserved:9;
		u64 page_offset:31;
		u64 ignored1:8;
		u64 s_shadow_stack:1;
		u64 ignored:2;
		u64 suppress_ve:1;
	};
	u64 value;
}ia32_ept_large_pde,*ia32_ept_large_pde_p;

typedef union _ia32_ept_pde
{
	struct
	{
		u64 read:1;
		u64 write:1;
		u64 execute:1;
		u64 reserved0:4;
		u64 large_pde:1;		// This bit must be reset.
		u64 accessed:1;
		u64 ignored0:1;
		u64 umx:1;
		u64 ignored1:1;
		u64 pte_offset:40;
		u64 ignored2:12;
	};
	u64 value;
}ia32_ept_pde,*ia32_ept_pde_p;

typedef union _ia32_ept_pte
{
	struct
	{
		u64 read:1;			// Bit	0
		u64 write:1;		// Bit	1
		u64 execute:1;		// Bit	2
		u64 memory_type:3;	// Bits	3-5
		u64 ignore_pat:1;	// Bit	6
		u64 ignored0:1;		// Bit	7
		u64 accessed:1;		// Bit	8
		u64 dirty:1;		// Bit	9
		u64 umx:1;			// Bit	10
		u64 ignored1:1;		// Bit	11
		u64 page_offset:40;
		u64 ignored2:8;
		u64 s_shadow_stack:1;
		u64 subpage_write:1;
		u64 ignored3:1;
		u64 suppress_ve:1;
	};
	u64 value;
}ia32_ept_pte,*ia32_ept_pte_p;

// Notice that EPT PDPTE Descriptor is describing
// 512 1GiB-Pages in a 512GiB Page.
typedef struct _noir_ept_pdpte_descriptor
{
	struct _noir_ept_pdpte_descriptor* next;
	ia32_ept_pdpte_p virt;
	u64 phys;
	u64 gpa_start;
}noir_ept_pdpte_descriptor,*noir_ept_pdpte_descriptor_p;

// Notice that EPT PDE Descriptor is describing
// 512 2MiB-Pages in a 1GiB Page.
typedef struct _noir_ept_pde_descriptor
{
	struct _noir_ept_pde_descriptor* next;
	ia32_ept_pde_p virt;
	u64 phys;
	u64 gpa_start;
}noir_ept_pde_descriptor,*noir_ept_pde_descriptor_p;

// Notice that EPT PTE Descriptor is describing
// 512 4KB-Pages in a 2MB Page.
typedef struct _noir_ept_pte_descriptor
{
	struct _noir_ept_pte_descriptor* next;
	ia32_ept_pte_p virt;
	u64 phys;
	u64 gpa_start;
}noir_ept_pte_descriptor,*noir_ept_pte_descriptor_p;

typedef struct _noir_ept_manager
{
	struct
	{
		ia32_ept_pml4e_p virt;
		ia32_ept_pointer phys;
	}eptp;			//512 PML4E Entries
	struct
	{
		ia32_ept_pdpte_p virt;
		u64 phys;
	}pdpt;
	struct
	{
		ia32_ept_large_pde_p virt;
		u64 phys;
	}pde;
	struct
	{
		noir_ept_pte_descriptor_p head;
		noir_ept_pte_descriptor_p tail;
	}pte;
	memory_descriptor blank_page;
	ia32_mtrr_def_type_msr def_type;
	u8 phys_addr_size;
	u8 virt_addr_size;
#if !defined(_hv_type1)
	u32 pending_hook_index;
	noir_hook_page hook_pages[1];
#endif
}noir_ept_manager,*noir_ept_manager_p;

typedef union _ia32_ept_violation_qualification
{
	struct
	{
		ulong_ptr read:1;
		ulong_ptr write:1;
		ulong_ptr execute:1;
		ulong_ptr readable:1;
		ulong_ptr writable:1;
		ulong_ptr executable:1;
		ulong_ptr umx_allowed:1;
		ulong_ptr gva_valid:1;
		ulong_ptr translation_violation:1;
		ulong_ptr um_address:1;
		ulong_ptr rw_address:1;
		ulong_ptr ex_address:1;
		ulong_ptr iret_nmi_block:1;
		ulong_ptr shadow_stack:1;
		ulong_ptr sss:1;
		ulong_ptr undefined:1;
		ulong_ptr async_instruction:1;
#if defined(_amd64)
		ulong_ptr reserved:47;
#else
		ulong_ptr reserved:15;
#endif
	};
	ulong_ptr value;
}ia32_ept_violation_qualification,*ia32_ept_violation_qualification_p;

bool nvc_ept_protect_hypervisor(noir_hypervisor_p hvm,noir_ept_manager_p eptm);
noir_ept_manager_p nvc_ept_build_identity_map();
void nvc_ept_cleanup(noir_ept_manager_p eptm);
void nvc_ept_update_by_mtrr(noir_ept_manager_p eptm);