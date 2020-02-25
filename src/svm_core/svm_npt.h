/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the basic driver of Intel EPT.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_npt.h
*/

#include <nvdef.h>

typedef union _amd64_addr_translator
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
}amd64_addr_translator,*amd_addr_translator_p;

typedef union _amd64_npt_pml4e
{
	struct
	{
		u64 present:1;		// Bit	0
		u64 write:1;		// Bit	1
		u64 user:1;			// Bit	2
		u64 pwt:1;			// Bit	3
		u64 pcd:1;			// Bit	4
		u64 accessed:1;		// Bit	5
		u64 reserved1:6;	// Bits 6-11
		u64 pdpte_base:40;	// Bits	12-51
		u64 reserved2:11;	// Bits 52-62
		u64 no_execute:1;	// Bit	63
	};
	u64 value;
}amd64_npt_pml4e,*amd64_npt_pml4e_p;

typedef union _amd64_npt_huge_pdpte
{
	struct
	{
		u64 present:1;		// Bit	0
		u64 write:1;		// Bit	1
		u64 user:1;			// Bit	2
		u64 pwt:1;			// Bit	3
		u64 pcd:1;			// Bit	4
		u64 accessed:1;		// Bit	5
		u64 dirty:1;		// Bit	6
		u64 huge_pdpte:1;	// Bit	7
		u64 global:1;		// Bit	8
		u64 reserved1:3;	// Bits 9-11
		u64 pat:1;			// Bit	12
		u64 reserved2:17;	// Bits 13-29
		u64 page_base:22;	// Bits	30-51
		u64 reserved3:11;	// Bits 52-62
		u64 no_execute:1;	// Bit	63
	};
	u64 value;
}amd64_npt_huge_pdpte,*amd64_npt_huge_pdpte_p;

typedef union _amd64_npt_pdpte
{
	struct
	{
		u64 present:1;		// Bit	0
		u64 write:1;		// Bit	1
		u64 user:1;			// Bit	2
		u64 pwt:1;			// Bit	3
		u64 pcd:1;			// Bit	4
		u64 accessed:1;		// Bit	5
		u64 reserved1:6;	// Bits 6-11
		u64 pde_base:40;	// Bits	12-51
		u64 reserved2:11;	// Bits 52-62
		u64 no_execute:1;	// Bit	63
	};
	u64 value;
}amd64_npt_pdpte,*amd64_npt_pdpte_p;

typedef union _amd64_npt_large_pde
{
	struct
	{
		u64 present:1;		// Bit	0
		u64 write:1;		// Bit	1
		u64 user:1;			// Bit	2
		u64 pwt:1;			// Bit	3
		u64 pcd:1;			// Bit	4
		u64 accessed:1;		// Bit	5
		u64 dirty:1;		// Bit	6
		u64 large_pde:1;	// Bit	7
		u64 global:1;		// Bit	8
		u64 reserved1:3;	// Bits	9-11
		u64 pat:1;			// Bit	12
		u64 reserved2:8;	// Bits 13-20
		u64 page_base:31;	// Bits	21-51
		u64 reserved3:11;	// Bits 52-62
		u64 no_execute:1;	// Bit	63
	};
	u64 value;
}amd64_npt_large_pde,*amd64_npt_large_pde_p;

typedef union _amd64_npt_pde
{
	struct
	{
		u64 present:1;		// Bit	0
		u64 write:1;		// Bit	1
		u64 user:1;			// Bit	2
		u64 pwt:1;			// Bit	3
		u64 pcd:1;			// Bit	4
		u64 accessed:1;		// Bit	5
		u64 reserved1:6;	// Bits 6-11
		u64 pte_base:40;	// Bits	12-51
		u64 reserved2:11;	// Bits 52-62
		u64 no_execute:1;	// Bit	63
	};
	u64 value;
}amd64_npt_pde,*amd64_npt_pde_p;

typedef union _amd64_npt_pte
{
	struct
	{
		u64 present:1;		// Bit	0
		u64 write:1;		// Bit	1
		u64 user:1;			// Bit	2
		u64 pwt:1;			// Bit	3
		u64 pcd:1;			// Bit	4
		u64 accessed:1;		// Bit	5
		u64 dirty:1;		// Bit	6
		u64 pat:1;			// Bit	7
		u64 global:1;		// Bit	8
		u64 reserved1:3;	// Bits 9-11
		u64 page_base:40;		// Bits	12-51
		u64 reserved2:11;	// Bits 52-62
		u64 no_execute:1;	// Bit	63
	};
	u64 value;
}amd64_npt_pte,*amd64_npt_pte_p;

// Notice that NPT PTE Descriptor is describing
// 512 4KB-Pages in a 2MB Page.
typedef struct _noir_npt_pte_descriptor
{
	struct _noir_npt_pte_descriptor* next;
	amd64_npt_pte_p virt;
	u64 phys;
	u64 gpa_start;
}noir_npt_pte_descriptor,*noir_npt_pte_descriptor_p;

typedef struct _noir_npt_manager
{
	struct
	{
		amd64_npt_pml4e_p virt;
		u64 phys;
	}ncr3;
	struct
	{
		amd64_npt_pdpte_p virt;
		u64 phys;
	}pdpt;
	struct
	{
		amd64_npt_large_pde_p virt;
		u64 phys;
	}pde;
	struct
	{
		noir_npt_pte_descriptor_p head;
		noir_npt_pte_descriptor_p tail;
	}pte;
}noir_npt_manager,*noir_npt_manager_p;

typedef union _amd64_npt_fault_code
{
	struct
	{
		u64 present:1;		// Bit	0
		u64 write:1;		// Bit	1
		u64 user:1;			// Bit	2
		u64 rsv_b:1;		// Bit	3
		u64 execute:1;		// Bit	4
		u64 reserved1:27;	// Bits	5-31
		u64 npf_addr:1;		// Bit	32
		u64 npf_table:1;	// Bit	33
		u64 reserved2:30;	// Bits	34-63
	};
	u64 value;
}amd64_npt_fault_code,*amd64_npt_fault_code_p;

bool nvc_npt_protect_critical_hypervisor(noir_hypervisor_p hvm);
bool nvc_npt_initialize_ci(noir_npt_manager_p nptm);
noir_npt_manager_p nvc_npt_build_identity_map();
void nvc_npt_build_hook_mapping(noir_hypervisor_p hvm);
void nvc_npt_cleanup(noir_npt_manager_p nptm);