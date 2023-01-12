/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the basic driver of Intel EPT.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_npt.h
*/

#include <nvdef.h>

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
		u64 page_base:40;	// Bits	12-51
		u64 reserved2:11;	// Bits 52-62
		u64 no_execute:1;	// Bit	63
	};
	u64 value;
}amd64_npt_pte,*amd64_npt_pte_p;

// Notice that NPT PDPTE Descriptor is describing
// 512 1GiB-Pages in a 512GiB Page.
typedef struct _noir_npt_pdpte_descriptor
{
	struct _noir_npt_pdpte_descriptor* next;
	union
	{
		amd64_npt_pdpte_p virt;
		amd64_npt_huge_pdpte_p huge;
	};
	u64 phys;
	u64 gpa_start;
}noir_npt_pdpte_descriptor,*noir_npt_pdpte_descriptor_p;

// Notice that NPT PDE Descriptor is describing
// 512 2MiB-Pages in a 1GiB Page.
typedef struct _noir_npt_pde_descriptor
{
	struct _noir_npt_pde_descriptor* next;
	union
	{
		amd64_npt_pde_p virt;
		amd64_npt_large_pde_p large;
	};
	u64 phys;
	u64 gpa_start;
}noir_npt_pde_descriptor,*noir_npt_pde_descriptor_p;

// Notice that NPT PTE Descriptor is describing
// 512 4KiB-Pages in a 2MiB Page.
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
		amd64_npt_huge_pdpte_p virt;
		u64 phys;
	}pdpt;
	struct
	{
		noir_npt_pde_descriptor_p head;
		noir_npt_pde_descriptor_p tail;
	}pde;
	struct
	{
		noir_npt_pte_descriptor_p head;
		noir_npt_pte_descriptor_p tail;
	}pte;
#if !defined(_hv_type1)
	noir_pushlock nptm_lock;
	noir_hook_page hook_pages[1];
#endif
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
		u64 reserved1:1;	// Bit	5, albeit reserved, this should be about protection keys once AMD defines it.
		u64 shadow_stk:1;	// Bit	6
		u64 reserved2:24;	// Bits	7-30
		u64 rmp_check:1;	// Bit	31
		u64 npf_addr:1;		// Bit	32
		u64 npf_table:1;	// Bit	33
		u64 encrypted:1;	// Bit	34
		u64 size_mismatch:1;// Bit	35
		u64 vmpl_failed:1;	// Bit	36
		u64 sss:1;			// Bit	37
		u64 reserved4:26;	// Bits 38-63
	};
	u64 value;
}amd64_npt_fault_code,*amd64_npt_fault_code_p;

bool nvc_npt_protect_critical_hypervisor(noir_hypervisor_p hvm);
bool nvc_npt_initialize_ci(noir_npt_manager_p nptm);
noir_npt_manager_p nvc_npt_build_identity_map();
void nvc_npt_build_reverse_map();
bool nvc_npt_update_pde(noir_npt_manager_p nptm,u64 gpa,u64 hpa,bool r,bool w,bool x,bool l,bool alloc);
bool nvc_npt_update_pte(noir_npt_manager_p nptm,u64 hpa,u64 gpa,bool r,bool w,bool x,bool alloc);
#if defined(_hv_type1)
bool nvc_npt_build_apic_interceptions();
#else
void nvc_npt_build_hook_mapping(noir_npt_manager_p pri_nptm,noir_npt_manager_p sec_nptm);
#endif
void nvc_npt_cleanup(noir_npt_manager_p nptm);
u32 nvc_npt_get_allocation_size();