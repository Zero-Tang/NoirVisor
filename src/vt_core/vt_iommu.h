/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the basic driver of Intel VT-d.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_iommu.c
*/

#include <nvdef.h>
#include "vt_iommudef.h"

typedef struct _noir_dmar_pml4e_descriptor
{
	struct _noir_dmar_pml4e_descriptor* next;
	intel_iommu_stage2_pml4e_p virt;
	u64 phys;
	u64 gpa_start;
}noir_dmar_pml4e_descriptor,*noir_dmar_pml4e_descriptor_p;

typedef struct _noir_dmar_pdpte_descriptor
{
	struct _noir_dmar_pdpte_descriptor* next;
	union
	{
		intel_iommu_stage2_huge_pdpte_p huge;
		intel_iommu_stage2_pdpte_p virt;
	};
	u64 phys;
	u64 gpa_start;
}noir_dmar_pdpte_descriptor,*noir_dmar_pdpte_descriptor_p;

typedef struct _noir_dmar_pde_descriptor
{
	struct _noir_dmar_pde_descriptor* next;
	union
	{
		intel_iommu_stage2_large_pde_p large;
		intel_iommu_stage2_pde_p virt;
	};
	u64 phys;
	u64 gpa_start;
}noir_dmar_pde_descriptor,*noir_dmar_pde_descriptor_p;

typedef struct _noir_dmar_pte_descriptor
{
	struct _noir_dmar_pte_descriptor* next;
	intel_iommu_stage2_pte_p virt;
	u64 phys;
	u64 gpa_start;
}noir_dmar_pte_descriptor,*noir_dmar_pte_descriptor_p;

typedef struct _noir_dmar_iommu_bar_descriptor
{
	void* virt;
	u64 phys;
	u32 pages;
	// IOMMU-aware OS may utilize Invalidation Queue.
	struct
	{
		memory_descriptor hv_queue;
		struct
		{
			memory_descriptor queue;
			u64 size;
			u64 head;
			u64 tail;
		}guest;
		u32 wait_status;
	}inval_queue;
	// Records the state before NoirVisor uses IOMMU.
	union
	{
		struct
		{
			u32 tes:1;
			u32 reserved:31;
		};
	}prev_state;
	u8 fault_vector;
	u64 prev_root_ptr;
	u64 iotlb_reg_offset;
	u64 fault_rec_offset;
}noir_dmar_iommu_bar_descriptor,*noir_dmar_iommu_bar_descriptor_p;

// Note: Main DMAR Manager also affects NoirVisor CVM.
// This is different from EPT Manager.
typedef struct _noir_dmar_manager
{
	struct
	{
		intel_iommu_root_entry_p virt;
		u64 phys;
	}root;
	struct
	{
		intel_iommu_context_entry_p virt;
		u64 phys;
	}context;
	// Following page tables organizations are intended for the Subverted Host.
	// For CVM Guests, organize DMAR page tables in correspondng VM structures.
	struct
	{
		intel_iommu_stage2_pml5e_p virt;
		u64 phys;
	}pml5;
	union
	{
		struct
		{
			noir_dmar_pml4e_descriptor_p head;
			noir_dmar_pml4e_descriptor_p tail;
		};
		struct
		{
			intel_iommu_stage2_pml4e_p virt;
			u64 phys;
		};
	}pml4;
	union
	{
		struct
		{
			noir_dmar_pdpte_descriptor_p head;
			noir_dmar_pdpte_descriptor_p tail;
		};
		struct
		{
			union
			{
				intel_iommu_stage2_pdpte_p virt;
				intel_iommu_stage2_huge_pdpte_p huge;
			};
			u64 phys;
		};
	}pdpte;
	struct
	{
		noir_dmar_pde_descriptor_p head;
		noir_dmar_pde_descriptor_p tail;
	}pde;
	struct
	{
		noir_dmar_pte_descriptor_p head;
		noir_dmar_pte_descriptor_p tail;
	}pte;
	// Lower bits list the features supported by all IOMMU Hardwares.
	// Higher bits list the what NoirVisor has enabled.
	union
	{
		struct
		{
			u64 la39:1;
			u64 la48:1;
			u64 la57:1;
			u64 large_page:1;
			u64 huge_page:1;
			u64 reserved:55;
			u64 using_agaw:3;
			u64 active:1;
		};
		u64 value;
	}minimum_features;
	// IOMMU Register Bases
	u64 iommu_count;
	noir_dmar_iommu_bar_descriptor bars[0];
}noir_dmar_manager,*noir_dmar_manager_p;

void nvc_vt_iommu_enqueue_context_cache_invalidation(noir_dmar_iommu_bar_descriptor_p bar,u8 granularity,u16 domain_id,u16 source_id,u8 function_mask);
void nvc_vt_iommu_enqueue_iotlb_invalidation(noir_dmar_iommu_bar_descriptor_p bar,u8 granularity,bool drain_write,bool drain_read,u16 domain_id,u8 address_mask,bool inval_hint,u64 address);
void nvc_vt_iommu_enqueue_device_tlb_invalidation(noir_dmar_iommu_bar_descriptor_p bar,u16 pfsid,u8 mip,u16 source_id,bool size,u64 address);
void nvc_vt_iommu_enqueue_intentry_cache_invalidation(noir_dmar_iommu_bar_descriptor_p bar,bool granularity,u8 index_mask,u16 interrupt_index);
void nvc_vt_iommu_wait_invalidation_queue(noir_dmar_iommu_bar_descriptor_p bar,bool interrupt,bool write_status,bool fence,bool drain_pagereq,u32 status_data,u64 status_address);
void nvc_vt_iommu_set_global_command(noir_dmar_iommu_bar_descriptor_p bar,u32 bit_position,bool value);
void nvc_vt_iommu_flush_context_cache(noir_dmar_iommu_bar_descriptor_p bar,u16 domain_id,u16 source_id,u8 function_mask,u8 request_granularity);
void nvc_vt_iommu_flush_iotlb(noir_dmar_iommu_bar_descriptor_p bar,u16 domain_id,bool drain_write,bool drain_read,u8 request_granularity,u64 address);

noir_status nvc_vt_iommu_initialize();
void nvc_vt_iommu_activate();
void nvc_vt_iommu_finalize();