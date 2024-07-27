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
#include <nvstatus.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include <acpi.h>
#include <ci.h>
#include "vt_iommu.h"

void nvc_vt_iommu_enqueue_context_cache_invalidation(noir_dmar_iommu_bar_descriptor_p bar,u8 granularity,u16 domain_id,u16 source_id,u8 function_mask)
{
	// Get the tail.
	intel_iommu_inval_queue_tail_register tail={.value=noir_mmio_read64((u64)bar->virt+mmio_offset_inval_queue_tail)};
	intel_iommu_queued_inval_legacy_descriptor_p inval_d;
	if(tail.value==page_size)tail.value=0;
	inval_d=(intel_iommu_queued_inval_legacy_descriptor_p)((ulong_ptr)bar->inval_queue.hv_queue.virt+tail.value);
	// Initialize Entries.
	inval_d->context_cache.lo=inval_d->context_cache.hi=0;
	inval_d->context_cache.type_lo=intel_iommu_queued_inval_type_context_cache;
	inval_d->context_cache.granularity=granularity;
	inval_d->context_cache.domain_id=domain_id;
	inval_d->context_cache.source_id=source_id;
	inval_d->context_cache.function_mask=function_mask;
	// Enqueue.
	tail.tail++;
	noir_mmio_write64((u64)bar->virt+mmio_offset_inval_queue_tail,tail.value);
}

void nvc_vt_iommu_enqueue_iotlb_invalidation(noir_dmar_iommu_bar_descriptor_p bar,u8 granularity,bool drain_write,bool drain_read,u16 domain_id,u8 address_mask,bool inval_hint,u64 address)
{
	// Get the tail.
	intel_iommu_inval_queue_tail_register tail={.value=noir_mmio_read64((u64)bar->virt+mmio_offset_inval_queue_tail)};
	intel_iommu_queued_inval_legacy_descriptor_p inval_d;
	if(tail.value==page_size)tail.value=0;
	inval_d=(intel_iommu_queued_inval_legacy_descriptor_p)((ulong_ptr)bar->inval_queue.hv_queue.virt+tail.value);
	// Initialize Entries.
	inval_d->iotlb.lo=inval_d->iotlb.hi=0;
	inval_d->iotlb.type_lo=intel_iommu_queued_inval_type_iotlb;
	inval_d->iotlb.granularity=granularity;
	inval_d->iotlb.drain_write=drain_write;
	inval_d->iotlb.drain_read=drain_read;
	inval_d->iotlb.domain_id=domain_id;
	inval_d->iotlb.address_mask=address_mask;
	inval_d->iotlb.address=page_count(address);
	// Enqueue.
	tail.tail++;
	noir_mmio_write64((u64)bar->virt+mmio_offset_inval_queue_tail,tail.value);
}

void nvc_vt_iommu_enqueue_device_tlb_invalidation(noir_dmar_iommu_bar_descriptor_p bar,u16 pfsid,u8 mip,u16 source_id,bool size,u64 address)
{
	// Get the tail.
	intel_iommu_inval_queue_tail_register tail={.value=noir_mmio_read64((u64)bar->virt+mmio_offset_inval_queue_tail)};
	intel_iommu_queued_inval_legacy_descriptor_p inval_d;
	if(tail.value==page_size)tail.value=0;
	inval_d=(intel_iommu_queued_inval_legacy_descriptor_p)((ulong_ptr)bar->inval_queue.hv_queue.virt+tail.value);
	// Initialize Entries.
	inval_d->device_tlb.lo=inval_d->device_tlb.hi=0;
	inval_d->device_tlb.type_lo=intel_iommu_queued_inval_type_device_tlb;
	inval_d->device_tlb.pfsid_lo=pfsid&0xf;
	inval_d->device_tlb.pfsid_hi=pfsid>>4;
	inval_d->device_tlb.mip=mip;
	inval_d->device_tlb.source_id=source_id;
	inval_d->device_tlb.size=size;
	inval_d->device_tlb.address=page_count(address);
	// Enqueue.
	tail.tail++;
	noir_mmio_write64((u64)bar->virt+mmio_offset_inval_queue_tail,tail.value);
}

void nvc_vt_iommu_enqueue_intentry_cache_invalidation(noir_dmar_iommu_bar_descriptor_p bar,bool granularity,u8 index_mask,u16 interrupt_index)
{
	// Get the tail.
	intel_iommu_inval_queue_tail_register tail={.value=noir_mmio_read64((u64)bar->virt+mmio_offset_inval_queue_tail)};
	intel_iommu_queued_inval_legacy_descriptor_p inval_d;
	if(tail.value==page_size)tail.value=0;
	inval_d=(intel_iommu_queued_inval_legacy_descriptor_p)((ulong_ptr)bar->inval_queue.hv_queue.virt+tail.value);
	// Initialize Entries.
	inval_d->intentry_cache.lo=inval_d->intentry_cache.hi=0;
	inval_d->intentry_cache.type_lo=intel_iommu_queued_inval_type_intentry_cache;
	inval_d->intentry_cache.granularity=granularity;
	inval_d->intentry_cache.index_mask=index_mask;
	inval_d->intentry_cache.interrupt_index=interrupt_index;
	// Enqueue.
	tail.tail++;
	noir_mmio_write64((u64)bar->virt+mmio_offset_inval_queue_tail,tail.value);
}

void nvc_vt_iommu_wait_invalidation_queue(noir_dmar_iommu_bar_descriptor_p bar,bool interrupt,bool write_status,bool fence,bool drain_pagereq,u32 status_data,u64 status_address)
{
	// Get the tail.
	intel_iommu_inval_queue_head_register head;
	intel_iommu_inval_queue_tail_register tail={.value=noir_mmio_read64((u64)bar->virt+mmio_offset_inval_queue_tail)};
	intel_iommu_queued_inval_legacy_descriptor_p inval_d;
	if(tail.value==page_size)tail.value=0;
	inval_d=(intel_iommu_queued_inval_legacy_descriptor_p)((ulong_ptr)bar->inval_queue.hv_queue.virt+tail.value);
	// Initialize Entries.
	inval_d->inval_wait.lo=inval_d->inval_wait.hi=0;
	inval_d->inval_wait.type_lo=intel_iommu_queued_inval_type_inval_wait;
	inval_d->inval_wait.interrupt_flag=interrupt;
	inval_d->inval_wait.status_write=write_status;
	inval_d->inval_wait.fence_flag=fence;
	inval_d->inval_wait.pagereq_drain=drain_pagereq;
	inval_d->inval_wait.status_data=status_data;
	inval_d->inval_wait.status_address=status_address>>2;
	// Enqueue.
	tail.tail++;
	noir_mmio_write64((u64)bar->virt+mmio_offset_inval_queue_tail,tail.value);
	// Wait for the hardware.
	do
	{
		noir_pause();
		head.value=noir_mmio_read64((u64)bar->virt+mmio_offset_inval_queue_head);
	}while(head.head!=tail.tail);
}

void nvc_vt_iommu_set_global_command(noir_dmar_iommu_bar_descriptor_p bar,u32 bit_position,bool value)
{
	u32v gcmd=noir_mmio_read32((u64)bar->virt+mmio_offset_global_status);
	u32v gst;
	if(value)
		noir_bts(&gcmd,bit_position);
	else
		noir_btr(&gcmd,bit_position);
	noir_mmio_write32((u64)bar->virt+mmio_offset_global_command,gcmd);
	do
	{
		noir_pause();
		gst=noir_mmio_read32((u64)bar->virt+mmio_offset_global_status);
	}while(noir_bt(&gst,bit_position)!=value);
}

void nvc_vt_iommu_flush_context_cache(noir_dmar_iommu_bar_descriptor_p bar,u16 domain_id,u16 source_id,u8 function_mask,u8 request_granularity)
{
	intel_iommu_context_command_register ccmd;
	ccmd.value=0;
	ccmd.domain_id=domain_id;
	ccmd.source_id=source_id;
	ccmd.function_mask=function_mask;
	ccmd.request_granularity=request_granularity;
	ccmd.inval_context_cache=true;
	noir_mmio_write64((u64)bar->virt+mmio_offset_context_command,ccmd.value);
	do
	{
		noir_pause();
		ccmd.value=noir_mmio_read64((u64)bar->virt+mmio_offset_context_command);
	}while(ccmd.inval_context_cache);
}

void nvc_vt_iommu_flush_iotlb(noir_dmar_iommu_bar_descriptor_p bar,u16 domain_id,bool drain_write,bool drain_read,u8 request_granularity,u64 address)
{
	intel_iommu_iotlb_inval_cmd_register icmd;
	icmd.value=0;
	icmd.domain_id=domain_id;
	icmd.drain_write=drain_write;
	icmd.drain_read=drain_read;
	icmd.request_granularity=request_granularity;
	icmd.inval_iotlb=true;
	noir_mmio_write64((u64)bar->virt+bar->iotlb_reg_offset+mmio_offset_iotlb_inval_addr,address);
	noir_mmio_write64((u64)bar->virt+bar->iotlb_reg_offset+mmio_offset_iotlb_inval_cmd,icmd.value);
	do
	{
		noir_pause();
		icmd.value=noir_mmio_read64((u64)bar->virt+bar->iotlb_reg_offset);
	}while(icmd.inval_iotlb);
}

// This handler routine will play the significant role of nested Intel VT-d.
void nvc_vt_iommu_region_rw_handler(bool direction,u64 address,u64 size,u64p value,void* context)
{
	noir_dmar_iommu_bar_descriptor_p bar=context;
	const u64 offset=address-bar->phys;
	const u64 addr=(u64)bar->virt+offset;
	// FIXME: Current implementation is simply pass-through. We should virtualize IOMMU accesses.
	nvd_printf("Intercepted IOMMU Access (%s)! Address=0x%llX (Offset=0x%llX), Access-Size=%llu\n",direction?"Write":"Read",address,offset,size);
	if(direction)
	{
		// Emulate Write to IOMMU...
		switch(size)
		{
		case 1:
			noir_mmio_write8(addr,*(u8p)value);
			break;
		case 2:
			noir_mmio_write16(addr,*(u16p)value);
			break;
		case 4:
			noir_mmio_write32(addr,*(u32p)value);
			break;
		case 8:
			noir_mmio_write64(addr,*(u64p)value);
			break;
		default:
			nvd_printf("Unsupported size (%u) of MMIO to IOMMU is intercepted!",size);
			break;
		}
	}
	else
	{
		// Emulate Read from IOMMU...
		switch(size)
		{
		case 1:
			*(u8p)value=noir_mmio_read8(addr);
			break;
		case 2:
			*(u16p)value=noir_mmio_read16(addr);
			break;
		case 4:
			*(u32p)value=noir_mmio_read32(addr);
			break;
		case 8:
			*(u64p)value=noir_mmio_read64(addr);
			break;
		default:
			nvd_printf("Unsupported size (%u) of MMIO to IOMMU is intercepted!",size);
			break;
		}
	}
}

void nvc_vt_iommu_cleanup()
{
	noir_dmar_manager_p dmarm=hvm_p->relative_hvm->dmar_manager;
	if(dmarm)
	{
		noir_dmar_pde_descriptor_p pde_p=dmarm->pde.head;
		noir_dmar_pte_descriptor_p pte_p=dmarm->pte.head;
		for(u64 i=0;i<dmarm->iommu_count;i++)
		{
			// Unmap IOMMU Register Base.
			if(dmarm->bars[i].virt)
				noir_unmap_physical_memory(dmarm->bars[i].virt,page_mult(dmarm->bars[i].pages));
#if !defined(_hv_type1)
			// Unmap Guest's Invalidation Queue.
			if(dmarm->bars[i].inval_queue.guest.queue.virt)
				noir_unmap_physical_memory(dmarm->bars[i].inval_queue.guest.queue.virt,dmarm->bars[i].inval_queue.guest.size);
#endif
			// Free Invalidation Queue.
			if(dmarm->bars[i].inval_queue.hv_queue.virt)
				noir_free_contd_memory(dmarm->bars[i].inval_queue.hv_queue.virt,page_size);
		}
		// Free Root-Table and Context-Table.
		if(dmarm->root.virt)
			noir_free_contd_memory(dmarm->root.virt,page_size);
		if(dmarm->context.virt)
			noir_free_contd_memory(dmarm->context.virt,256*page_size);
		// Level 5 (PML5E), if supported, is always top-level.
		if(dmarm->minimum_features.using_agaw<=intel_iommu_context_agaw_57_bit)
			if(dmarm->pml5.virt)
				noir_free_contd_memory(dmarm->pml5.virt,page_size);
		// Level 4 (PML4E) can be either top-level or sub-level.
		if(dmarm->minimum_features.using_agaw<=intel_iommu_context_agaw_48_bit)
		{
			if(dmarm->pml4.virt)
				noir_free_contd_memory(dmarm->pml4.virt,page_size);
		}
		else
		{
			// This is a linked-list.
			noir_dmar_pml4e_descriptor_p cur=dmarm->pml4.head;
			while(cur)
			{
				noir_dmar_pml4e_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		// Level 3 (PDPTE) can be either top-level or sub-level.
		if(dmarm->minimum_features.using_agaw<=intel_iommu_context_agaw_39_bit)
			noir_free_contd_memory(dmarm->pdpte.virt,page_size);
		else
		{
			// This is a linked-list.
			noir_dmar_pdpte_descriptor_p cur=dmarm->pdpte.head;
			while(cur)
			{
				noir_dmar_pdpte_descriptor_p next=cur->next;
				if(cur->virt)noir_free_contd_memory(cur->virt,page_size);
				noir_free_nonpg_memory(cur);
				cur=next;
			}
		}
		// Level 2 (PDE) is always sub-level.
		while(pde_p)
		{
			noir_dmar_pde_descriptor_p next=pde_p->next;
			if(pde_p->virt)noir_free_contd_memory(pde_p->virt,page_size);
			noir_free_nonpg_memory(pde_p);
			pde_p=next;
		}
		// Level 1 (PTE) is always sub-level.
		while(pte_p)
		{
			noir_dmar_pte_descriptor_p next=pte_p->next;
			if(pte_p->virt)noir_free_contd_memory(pte_p->virt,page_size);
			noir_free_nonpg_memory(pte_p);
			pte_p=next;
		}
		// Free the entire DMAR Manager.
		noir_free_nonpg_memory(dmarm);
	}
}

noir_dmar_pml4e_descriptor_p nvc_vt_iommu_search_pml4e_descriptor(noir_dmar_manager_p dmar_manager,u64 gpa)
{
	if(dmar_manager->minimum_features.using_agaw<=intel_iommu_context_agaw_48_bit)
		return null;
	for(noir_dmar_pml4e_descriptor_p cur=dmar_manager->pml4.head;cur;cur=cur->next)
		if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_256tb_size)
			return cur;
	return null;
}

noir_dmar_pdpte_descriptor_p nvc_vt_iommu_search_pdpte_descriptor(noir_dmar_manager_p dmar_manager,u64 gpa)
{
	if(dmar_manager->minimum_features.using_agaw<=intel_iommu_context_agaw_39_bit)
		return null;
	for(noir_dmar_pdpte_descriptor_p cur=dmar_manager->pdpte.head;cur;cur=cur->next)
		if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_512gb_size)
			return cur;
	return null;
}

noir_dmar_pde_descriptor_p nvc_vt_iommu_search_pde_descriptor(noir_dmar_manager_p dmar_manager,u64 gpa)
{
	for(noir_dmar_pde_descriptor_p cur=dmar_manager->pde.head;cur;cur=cur->next)
		if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_1gb_size)
			return cur;
	return null;
}

noir_dmar_pte_descriptor_p nvc_vt_iommu_search_pte_descriptor(noir_dmar_manager_p dmar_manager,u64 gpa)
{
	for(noir_dmar_pte_descriptor_p cur=dmar_manager->pte.head;cur;cur=cur->next)
		if(gpa>=cur->gpa_start && gpa<cur->gpa_start+page_2mb_size)
			return cur;
	return null;
}

// This will create a PML4E table that covers 256TiB range.
// This also splits upper PML5E structures, if applicable.
noir_status nvc_vt_iommu_create_512gb_page_map(noir_dmar_manager_p dmar_manager,u64 gpa,noir_dmar_pml4e_descriptor_p *descriptor)
{
	noir_status st=noir_insufficient_resources;
	// Note that PML4E can be top-level of paging structure.
	if(dmar_manager->minimum_features.using_agaw==intel_iommu_context_agaw_48_bit)
	{
		*descriptor=null;
		st=noir_success;
	}
	else
	{
		noir_dmar_pml4e_descriptor_p pml4e_p=nvc_vt_iommu_search_pml4e_descriptor(dmar_manager,gpa);
		if(pml4e_p)
		{
			*descriptor=pml4e_p;
			return noir_success;
		}
		pml4e_p=noir_alloc_nonpg_memory(sizeof(noir_dmar_pml4e_descriptor));
		if(pml4e_p)
		{
			pml4e_p->virt=noir_alloc_contd_memory(sizeof(page_size));
			if(pml4e_p->virt==null)
				noir_free_nonpg_memory(pml4e_p);
			else
			{
				// Setup PML4E descriptor.
				pml4e_p->phys=noir_get_physical_address(pml4e_p->virt);
				pml4e_p->gpa_start=page_256tb_base(gpa);
				// Append to linked-list.
				if(dmar_manager->pml4.head)
					dmar_manager->pml4.tail->next=pml4e_p;
				else
					dmar_manager->pml4.head=pml4e_p;
				dmar_manager->pml4.tail=pml4e_p;
				// Setup upper-level mapping.
				if(dmar_manager->minimum_features.using_agaw==intel_iommu_context_agaw_57_bit)
				{
					// PML5E is the top-level. No descriptors can be found.
					intel_iommu_stage2_pml5e_p pml5e_p=dmar_manager->pml5.virt;
					ia32_addr_translator gat;
					gat.value=gpa;
					pml5e_p[gat.pml5e_offset].read=pml5e_p[gat.pml5e_offset].write=pml5e_p[gat.pml5e_offset].execute=true;
					pml5e_p[gat.pml5e_offset].pml4_ptr=page_count(pml4e_p->phys);
					*descriptor=pml4e_p;
					st=noir_success;
				}
				else
				{
					// NoirVisor doesn't know 6 or more levels of paging yet.
					nv_dprintf("Unexpected AGAW (%u bits) while setting up 512GiB page map!\n",30+9*dmar_manager->minimum_features.using_agaw);
					noir_int3();
					st=noir_not_implemented;
				}
			}
		}
	}
	return st;
}

// This will create a PDPTE table that covers 512GiB range.
// This also splits upper PML4E structures, if applicable.
noir_status nvc_vt_iommu_create_1gb_page_map(noir_dmar_manager_p dmar_manager,u64 gpa,bool huge,noir_dmar_pdpte_descriptor_p *descriptor)
{
	noir_status st=noir_insufficient_resources;
	// Note that PDPTE can be top-level of paging structure.
	if(dmar_manager->minimum_features.using_agaw==intel_iommu_context_agaw_39_bit)
	{
		if(huge)
		{
			intel_iommu_stage2_huge_pdpte_p pdpte_p=dmar_manager->pdpte.huge;
			// Set-up Identity-Mapping if this PDPTE is created to map huge pages.
			for(u32 i=0;i<page_table_entries64;i++)
			{
				pdpte_p[i].read=pdpte_p[i].write=pdpte_p[i].execute=pdpte_p[i].huge_page=true;
				pdpte_p[i].page_ptr=page_1gb_count(gpa)+i;
			}
		}
		*descriptor=null;
		st=noir_success;
	}
	else
	{
		noir_dmar_pdpte_descriptor_p pdpte_p=nvc_vt_iommu_search_pdpte_descriptor(dmar_manager,gpa);
		if(pdpte_p)
		{
			*descriptor=pdpte_p;
			return noir_success;
		}
		pdpte_p=noir_alloc_nonpg_memory(sizeof(noir_dmar_pdpte_descriptor));
		if(pdpte_p)
		{
			pdpte_p->virt=noir_alloc_contd_memory(page_size);
			if(pdpte_p->virt==null)
				noir_free_nonpg_memory(pdpte_p);
			else
			{
				// Locate the PML4E descriptor to update PDPTE.
				noir_dmar_pml4e_descriptor_p pml4e_p;
				st=nvc_vt_iommu_create_512gb_page_map(dmar_manager,gpa,&pml4e_p);
				if(st==noir_success)
				{
					// Setup PDPTE descriptor.
					pdpte_p->phys=noir_get_physical_address(pdpte_p->virt);
					pdpte_p->gpa_start=page_512gb_base(gpa);
					// Append to linked-list.
					if(dmar_manager->pdpte.head)
						dmar_manager->pdpte.tail->next=pdpte_p;
					else
						dmar_manager->pdpte.head=pdpte_p;
					dmar_manager->pdpte.tail=pdpte_p;
					if(huge)
					{
						// Set-up Identity-Mapping if this PDPTE is created to map huge pages.
						for(u32 i=0;i<page_table_entries64;i++)
						{
							pdpte_p->huge[i].read=pdpte_p->huge[i].write=pdpte_p->huge[i].execute=pdpte_p->huge[i].huge_page=true;
							pdpte_p->huge[i].page_ptr=page_1gb_count(pdpte_p->gpa_start)+i;
						}
					}
					// Set-up upper-level mapping.
					if(dmar_manager->minimum_features.using_agaw==intel_iommu_context_agaw_48_bit)
					{
						// PML4E is the top-level. No descriptors can be found.
						intel_iommu_stage2_pml4e_p pml4e_p=dmar_manager->pml4.virt;	// Note that this is a shadowed variable.
						ia32_addr_translator gat;
						gat.value=gpa;
						pml4e_p[gat.pml4e_offset].read=pml4e_p[gat.pml4e_offset].write=pml4e_p[gat.pml4e_offset].execute=true;
						pml4e_p[gat.pml4e_offset].pdpt_ptr=page_count(pdpte_p->phys);
						*descriptor=pdpte_p;
						st=noir_success;
					}
					else
					{
						ia32_addr_translator gat;
						gat.value=gpa;
						pml4e_p->virt[gat.pml4e_offset].read=pml4e_p->virt[gat.pml4e_offset].write=pml4e_p->virt[gat.pml4e_offset].execute=true;
						pml4e_p->virt[gat.pml4e_offset].pdpt_ptr=page_count(pdpte_p->phys);
						*descriptor=pdpte_p;
						st=noir_success;
					}
				}
				else
				{
					noir_free_contd_memory(pdpte_p->virt,page_size);
					noir_free_nonpg_memory(pdpte_p);
				}
			}
		}
	}
	return st;
}

// This will create a PDE table that covers 1GiB range.
// This also splits upper PDPTE structures.
noir_status nvc_vt_iommu_create_2mb_page_map(noir_dmar_manager_p dmar_manager,u64 gpa,bool large,noir_dmar_pde_descriptor_p *descriptor)
{
	noir_status st=noir_insufficient_resources;
	noir_dmar_pde_descriptor_p pde_p=nvc_vt_iommu_search_pde_descriptor(dmar_manager,gpa);
	if(pde_p)
	{
		*descriptor=pde_p;
		return noir_success;
	}
	pde_p=noir_alloc_nonpg_memory(sizeof(noir_dmar_pde_descriptor));
	if(pde_p)
	{
		pde_p->virt=noir_alloc_contd_memory(page_size);
		if(pde_p->virt==null)
			noir_free_nonpg_memory(pde_p);
		else
		{
			// Locate the PDPTE descriptor to update PDPTE.
			noir_dmar_pdpte_descriptor_p pdpte_p;
			st=nvc_vt_iommu_create_1gb_page_map(dmar_manager,gpa,true,&pdpte_p);
			if(st==noir_success)
			{
				// Setup PDE descriptor.
				pde_p->phys=noir_get_physical_address(pde_p->virt);
				pde_p->gpa_start=page_1gb_base(gpa);
				// Append to linked-list.
				if(dmar_manager->pde.head)
					dmar_manager->pde.tail->next=pde_p;
				else
					dmar_manager->pde.head=pde_p;
				dmar_manager->pde.tail=pde_p;
				if(large)
				{
					// Set-up Identity-Mapping if this PDE is created to map large pages.
					for(u32 i=0;i<page_table_entries64;i++)
					{
						pde_p->large[i].read=pde_p->large[i].write=pde_p->large[i].execute=pde_p->large[i].large_page=true;
						pde_p->large[i].page_ptr=page_2mb_count(pde_p->gpa_start)+i;
					}
				}
				// Set-up upper-level mapping.
				if(dmar_manager->minimum_features.using_agaw==intel_iommu_context_agaw_39_bit)
				{
					// PDPTE is the top-level. No descriptors can be found.
					intel_iommu_stage2_pdpte_p pdpte_p=dmar_manager->pdpte.virt;	// Note that this is a shadowed variable.
					ia32_addr_translator gat;
					gat.value=gpa;
					pdpte_p[gat.pdpte_offset].read=pdpte_p[gat.pdpte_offset].write=pdpte_p[gat.pdpte_offset].execute=true;
					pdpte_p[gat.pdpte_offset].huge_page=false;
					pdpte_p[gat.pdpte_offset].pde_ptr=page_count(pde_p->phys);
					*descriptor=pde_p;
					st=noir_success;
				}
				else
				{
					// PDPTE is not the top-level. Find the descriptor.
					ia32_addr_translator gat;
					gat.value=gpa;
					pdpte_p->virt[gat.pdpte_offset].read=pdpte_p->virt[gat.pdpte_offset].write=pdpte_p->virt[gat.pdpte_offset].execute=true;
					pdpte_p->virt[gat.pdpte_offset].huge_page=false;
					pdpte_p->virt[gat.pdpte_offset].pde_ptr=page_count(pde_p->phys);
					*descriptor=pde_p;
					st=noir_success;
				}
			}
			else
			{
				noir_free_contd_memory(pde_p->virt,page_size);
				noir_free_nonpg_memory(pde_p);
			}
		}
	}
	return st;
}

// This will create a PTE table that covers 2MiB range.
// This also splits upper PDE structures.
noir_status nvc_vt_iommu_create_4kb_page_map(noir_dmar_manager_p dmar_manager,u64 gpa,noir_dmar_pte_descriptor_p *descriptor)
{
	noir_status st=noir_insufficient_resources;
	noir_dmar_pte_descriptor_p pte_p=nvc_vt_iommu_search_pte_descriptor(dmar_manager,gpa);
	if(pte_p)
	{
		*descriptor=pte_p;
		return noir_success;
	}
	pte_p=noir_alloc_nonpg_memory(sizeof(noir_dmar_pte_descriptor));
	if(pte_p)
	{
		pte_p->virt=noir_alloc_contd_memory(page_size);
		if(pte_p->virt==null)
			noir_free_nonpg_memory(pte_p);
		else
		{
			// Locate the PDE descriptor to update PDE.
			noir_dmar_pde_descriptor_p pde_p;
			st=nvc_vt_iommu_create_2mb_page_map(dmar_manager,gpa,true,&pde_p);
			if(st==noir_success)
			{
				// Setup PTE descriptor.
				pte_p->phys=noir_get_physical_address(pte_p->virt);
				pte_p->gpa_start=page_2mb_base(gpa);
				// Append to linked-list.
				if(dmar_manager->pte.head)
					dmar_manager->pte.tail->next=pte_p;
				else
					dmar_manager->pte.head=pte_p;
				dmar_manager->pte.tail=pte_p;
				// Set-up Identity-Mapping.
				for(u32 i=0;i<page_table_entries64;i++)
				{
					pte_p->virt[i].read=pte_p->virt[i].write=pte_p->virt[i].execute=true;
					pte_p->virt[i].page_ptr=page_4kb_count(pte_p->gpa_start)+i;
				}
				// Set-up upper-level mapping.
				ia32_addr_translator gat;
				gat.value=gpa;
				pde_p->virt[gat.pde_offset].read=pde_p->virt[gat.pde_offset].write=pde_p->virt[gat.pde_offset].execute=true;
				pde_p->virt[gat.pde_offset].large_page=false;
				pde_p->virt[gat.pde_offset].pte_ptr=page_count(pte_p->phys);
				*descriptor=pte_p;
			}
			else
			{
				noir_free_contd_memory(pte_p->virt,page_size);
				noir_free_nonpg_memory(pte_p);
			}
		}
	}
	return st;
}

noir_status nvc_vt_iommu_update_pte(noir_dmar_manager_p dmar_manager,u64 gpa,u64 hpa,bool r,bool w,bool x)
{
	noir_dmar_pte_descriptor_p pte_p=null;
	noir_status st=nvc_vt_iommu_create_4kb_page_map(dmar_manager,gpa,&pte_p);
	if(st==noir_success)
	{
		ia32_addr_translator gat={.value=gpa};
		pte_p->virt[gat.pte_offset].read=r;
		pte_p->virt[gat.pte_offset].write=w;
		pte_p->virt[gat.pte_offset].execute=x;
		pte_p->virt[gat.pte_offset].page_ptr=page_count(hpa);
	}
	return st;
}

noir_status nvc_vt_iommu_initialize_ci(noir_dmar_manager_p dmar_manager)
{
	noir_status st=noir_success;
	for(u32 i=0;i<noir_ci->pages;i++)
	{
		u64 phys=noir_ci->page_ci[i].phys;
		st=nvc_vt_iommu_update_pte(dmar_manager,phys,phys,false,false,false);
		if(st!=noir_success)break;
	}
	return st;
}

// This routine will register MMIO region so that NoirVisor can intercept any accesses to Intel VT-d.
// In other words, Nested Virtualization of Intel VT-d may be viable.
noir_status nvc_vt_iommu_register_mmio_region(noir_dmar_manager_p dmar_manager)
{
	noir_status st=noir_success;
	for(u64 i=0;i<dmar_manager->iommu_count;i++)
	{
		// Initialize MMIO Region.
		noir_mmio_region mr;
		mr.handler=nvc_vt_iommu_region_rw_handler;
		mr.phys=dmar_manager->bars[i].phys;
		mr.size=page_mult(dmar_manager->bars[i].pages);
		mr.context=&dmar_manager->bars[i];
		// Register this Region.
		st=nvc_register_mmio_region(&mr);
		if(st!=noir_success)
		{
			nv_dprintf("Failed to register MMIO Region for IOMMU BAR 0x%p! Status=0x%X\n",mr.phys,st);
			break;
		}
	}
	return st;
}

/*
  Introduction to Identity map:

  Intel VT-d introduces a built-in GPA to HPA technology for peripheral devices.
  For NoirVisor, we translate GPA to HPA with GPA=HPA term.
  Essentials: {GPA=HPA} (Important).
  This is key purpose of identity map.

  However, due to different IOMMU hardware implementations, NoirVisor will
  not build the identity map in the same way as Intel EPT/AMD-V NPT. For
  example, VMware's IOMMU does not support 2MiB pages. The memory consumption
  could be ridiculously high if we try to cover the entire memory space.
  Hence, we just need to cover the physical RAM instead.
*/
void nvc_vt_iommu_add_ram_region(u64 start,u64 length,void* context)
{
	noir_dmar_manager_p dmarm=(noir_dmar_manager_p)context;
	// Determine the increment based on largest supported page size.
	const u64 increment=dmarm->minimum_features.huge_page?page_1gb_size:dmarm->minimum_features.large_page?page_2mb_size:page_4kb_size;
	for(u64 i=0;i<length;i+=increment)
	{
		noir_status st;
		switch(increment)
		{
			case page_4kb_size:
			{
				noir_dmar_pte_descriptor_p pte_p;
				st=nvc_vt_iommu_create_4kb_page_map(dmarm,start+i,&pte_p);
				break;
			}
			case page_2mb_size:
			{
				noir_dmar_pde_descriptor_p pde_p;
				st=nvc_vt_iommu_create_2mb_page_map(dmarm,start+i,true,&pde_p);
				break;
			}
			case page_1gb_size:
			{
				noir_dmar_pdpte_descriptor_p pdpte_p;
				st=nvc_vt_iommu_create_1gb_page_map(dmarm,start+i,true,&pdpte_p);
				break;
			}
		}
	}
}

// IOMMU Initialization follows a three-pass procedure.
// First pass will enumerate all IOMMU hardware and check their features.
// Second pass will setup optimized page tables.
// Third pass will activate DMA Remapping on all IOMMU Hardware.
noir_status nvc_vt_iommu_initialize()
{
	// Pass I: Enumerate all DMAR Hardware and check if all features are supported.
	bool la39=false,la48=false,la57=false;
	bool large_page=false,huge_page=false;
	u64 iommu_bar_count=0;
	acpi_dma_remapping_reporting_structure_p dmar_table=null;
	noir_status st=nvc_acpi_search_table('RAMD',null,(acpi_common_description_header_p*)&dmar_table);
	// Pre-set all features to true or otherwise we can't use logical-AND to determine minimum features.
	if(st==noir_success)la39=la48=la57=large_page=huge_page=true;
	while(st==noir_success)
	{
		// Check all DMAR hardware.
		intel_iommu_acpi_unit_common_header_p cur_head=(intel_iommu_acpi_unit_common_header_p)dmar_table->remapping_structures;
		const u32 len_limit=dmar_table->header.length-field_offset(acpi_dma_remapping_reporting_structure,remapping_structures);
		// Traverse all remapping hardware...
		for(u32 acc_len=0;acc_len<len_limit;acc_len+=cur_head->length)
		{
			cur_head=(intel_iommu_acpi_unit_common_header_p)&dmar_table->remapping_structures[acc_len];
			switch(cur_head->type)
			{
				case intel_iommu_acpi_type_drhd:
				{
					intel_iommu_acpi_drhd_p drhd=(intel_iommu_acpi_drhd_p)cur_head;
					intel_iommu_capability_register cap;
					void* iommu_virt=noir_map_uncached_memory(drhd->base_address,page_mult(1<<drhd->size.exponential));
					// Check Features.
					nv_dprintf("Found IOMMU BAR: 0x%016llX\tSize: %u 4KiB Pages\n",drhd->base_address,1<<drhd->size.exponential);
					cap.value=noir_mmio_read64((u64)iommu_virt+mmio_offset_capability);
					la39&=cap.support_39bit_agaw;
					la48&=cap.support_48bit_agaw;
					la57&=cap.support_57bit_agaw;
					large_page&=cap.support_ss_large_page;
					huge_page&=cap.support_ss_huge_page;
					noir_unmap_physical_memory(iommu_virt,page_mult(1<<drhd->size.exponential));
					// Increment counter.
					iommu_bar_count++;
					break;
				}
				default:
				{
					nv_dprintf("Ignoring unknown DMAR structure type %u...\n",cur_head->type);
					break;
				}
			}
		}
		// Make sure there is no other DMAR tables anymore.
		st=nvc_acpi_search_table('RAMD',(acpi_common_description_header_p)dmar_table,(acpi_common_description_header_p*)&dmar_table);
	}
	// Pass II: Setup the optimized page tables.
	if(la39 || la48 || la57)
	{
		noir_dmar_manager_p dmarm=noir_alloc_nonpg_memory(sizeof(noir_dmar_manager)+iommu_bar_count*sizeof(noir_dmar_iommu_bar_descriptor));
		if(dmarm==null)goto alloc_failure;
		dmarm->iommu_count=iommu_bar_count;
		nv_dprintf("This system %s 2MiB Large-Page and %s 1GiB Huge-Page\n",large_page?"supports":"doesn't support",huge_page?"supports":"doesn't support");
		hvm_p->relative_hvm->dmar_manager=dmarm;
		dmarm->minimum_features.la39=la39;
		dmarm->minimum_features.la48=la48;
		dmarm->minimum_features.la57=la57;
		dmarm->minimum_features.large_page=large_page;
		dmarm->minimum_features.huge_page=huge_page;
		// Allocate Root-Table and Context-Table.
		dmarm->root.virt=noir_alloc_contd_memory(page_size);
		if(dmarm->root.virt)
		{
			// We may need to assign some devices to NoirVisor CVM guests, so we need 256 pages to cover all devices individually.
			// Otherwise, just use one page and repeat in all root entries.
			dmarm->context.virt=noir_alloc_contd_memory(256*page_size);
			if(dmarm->context.virt)
			{
				dmarm->context.phys=noir_get_physical_address(dmarm->context.virt);
				for(u32 i=0;i<256;i++)
				{
					dmarm->root.virt[i].lo.present=true;
					// Remove the "+i" if it's just for mere DMA protection (i.e.: no device virtualization).
					dmarm->root.virt[i].lo.context_table_ptr=page_count(dmarm->context.phys)+i;
					for(u32 j=0;j<256;j++)
					{
						const u32 index=(i<<8)+j;
						dmarm->context.virt[index].lo.present=true;
						dmarm->context.virt[index].hi.domain_id=1;
					}
				}
			}
			else
				goto alloc_failure;
			dmarm->root.phys=noir_get_physical_address(dmarm->root.virt);
		}
		else
			goto alloc_failure;
		// Choose the biggest address-width to cover most addresses.
		// Just build the top-level page-level for now.
		if(la57)
		{
			nv_dprintf("Using 5-Level Paging to remap DMA in Intel VT-d...\n");
			dmarm->pml5.virt=noir_alloc_contd_memory(page_size);
			if(dmarm->pml5.virt)
			{
				dmarm->pml5.phys=noir_get_physical_address(dmarm->pml5.virt);
				for(u32 i=0;i<256*256;i++)
				{
					dmarm->context.virt[i].lo.sspt_ptr=page_count(dmarm->pml5.phys);
					dmarm->context.virt[i].hi.address_width=intel_iommu_context_agaw_57_bit;
				}
			}
			else
				goto alloc_failure;
			dmarm->minimum_features.using_agaw=intel_iommu_context_agaw_57_bit;
		}
		else if(la48)
		{
			nv_dprintf("Using 4-Level Paging to remap DMA in Intel VT-d...\n");
			dmarm->pml4.virt=noir_alloc_contd_memory(page_size);
			if(dmarm->pml4.virt)
			{
				dmarm->pml4.phys=noir_get_physical_address(dmarm->pml4.virt);
				for(u32 i=0;i<256*256;i++)
				{
					dmarm->context.virt[i].lo.sspt_ptr=page_count(dmarm->pml4.phys);
					dmarm->context.virt[i].hi.address_width=intel_iommu_context_agaw_48_bit;
				}
			}
			else
				goto alloc_failure;
			dmarm->minimum_features.using_agaw=intel_iommu_context_agaw_48_bit;
		}
		else if(la39)
		{
			nv_dprintf("Using 3-Level Paging to remap DMA in Intel VT-d...\n");
			dmarm->pdpte.virt=noir_alloc_contd_memory(page_size);
			if(dmarm->pdpte.virt)
			{
				dmarm->pdpte.phys=noir_get_physical_address(dmarm->pdpte.virt);
				for(u32 i=0;i<256*256;i++)
				{
					dmarm->context.virt[i].lo.sspt_ptr=page_count(dmarm->pdpte.phys);
					dmarm->context.virt[i].hi.address_width=intel_iommu_context_agaw_39_bit;
				}
			}
			else
				goto alloc_failure;
			dmarm->minimum_features.using_agaw=intel_iommu_context_agaw_39_bit;
		}
		// Then, enumerate the memory ranges in order to build the page table.
		noir_enum_physical_memory_ranges(nvc_vt_iommu_add_ram_region,dmarm);
		// Code-Integrity.
		nvc_vt_iommu_initialize_ci(dmarm);
		st=noir_success;
	}
	else
	{
		// All Intel VT-d hardware must support remapping DMA.
		nv_dprintf("There is at least one of the Intel VT-d hardware does not support remapping DMA!\n");
		st=noir_dmar_not_supported;
	}
	// Phase III: Map IOMMU BARs.
	if(st==noir_success)
	{
		noir_dmar_manager_p dmarm=hvm_p->relative_hvm->dmar_manager;
		u64 i=0;
		// Retrieve all IOMMU BARs.
		st=nvc_acpi_search_table('RAMD',null,(acpi_common_description_header_p*)&dmar_table);
		while(st==noir_success)
		{
			// Check all DMAR hardware.
			intel_iommu_acpi_unit_common_header_p cur_head=(intel_iommu_acpi_unit_common_header_p)dmar_table->remapping_structures;
			const u32 len_limit=dmar_table->header.length-field_offset(acpi_dma_remapping_reporting_structure,remapping_structures);
			// Traverse all remapping hardware...
			for(u32 acc_len=0;acc_len<len_limit;acc_len+=cur_head->length)
			{
				cur_head=(intel_iommu_acpi_unit_common_header_p)&dmar_table->remapping_structures[acc_len];
				switch(cur_head->type)
				{
					case intel_iommu_acpi_type_drhd:
					{
						intel_iommu_acpi_drhd_p drhd=(intel_iommu_acpi_drhd_p)cur_head;
						intel_iommu_capability_register cap;
						intel_iommu_extcap_register ext_cap;
						// Map the IOMMU BAR to virtual address space.
						dmarm->bars[i].phys=drhd->base_address;
						dmarm->bars[i].pages=1<<drhd->size.exponential;
						dmarm->bars[i].virt=noir_map_uncached_memory(dmarm->bars[i].phys,page_mult(dmarm->bars[i].pages));
						if(dmarm->bars[i].virt==null)goto alloc_failure;
						nv_dprintf("IOMMU is mapped to 0x%p!\n",dmarm->bars[i].virt);
						// Initialize Invalidation Queue.
						dmarm->bars[i].inval_queue.hv_queue.virt=noir_alloc_contd_memory(page_size);
						if(dmarm->bars[i].inval_queue.hv_queue.virt)
							dmarm->bars[i].inval_queue.hv_queue.phys=noir_get_physical_address(dmarm->bars[i].inval_queue.hv_queue.virt);
						else
							goto alloc_failure;
						cap.value=noir_mmio_read64((u64)dmarm->bars[i].virt+mmio_offset_capability);
						ext_cap.value=noir_mmio_read64((u64)dmarm->bars[i].virt+mmio_offset_extended_capability);
						dmarm->bars[i].iotlb_reg_offset=ext_cap.iotlb_offset<<4;
						dmarm->bars[i].fault_rec_offset=cap.fault_record_offset<<4;
						nv_dprintf("IOMMU Fault-Record Register: 0x%llX\n",dmarm->bars[i].phys+dmarm->bars[i].fault_rec_offset);
						i++;
						break;
					}
				}
			}
			st=nvc_acpi_search_table('RAMD',(acpi_common_description_header_p)dmar_table,(acpi_common_description_header_p*)&dmar_table);
		}
		nvc_vt_iommu_activate();
		// Eventually, register the IOMMU MMIO Regions.
		st=nvc_vt_iommu_register_mmio_region(dmarm);
		nv_dprintf("Intel VT-d MMIO-Region Registration Status=0x%X\n",st);
	}
	return st;
alloc_failure:
	nvc_vt_iommu_cleanup();
	nv_dprintf("Failed to allocate paging structures for Intel VT-d!\n");
	return noir_insufficient_resources;
}

void nvc_vt_iommu_activate()
{
	noir_dmar_manager_p dmarm=hvm_p->relative_hvm->dmar_manager;
	if(dmarm)
	{
		intel_iommu_root_table_address_register root_ptr;
		root_ptr.reserved=0;
		root_ptr.translation_table_mode=intel_iommu_translation_table_mode_legacy;
		root_ptr.root_table_ptr=page_count(dmarm->root.phys);
		// Setup all IOMMUs.
		for(u64 i=0;i<dmarm->iommu_count;i++)
		{
			intel_iommu_msi_ctrl_register fault_msi_ctrl;
			nv_dprintf("Working with IOMMU at 0x%llX...\n",dmarm->bars[i].phys);
			// Do not inadvertently disable anything that was enabled by the platform (OS, firmware, etc.).
#if !defined(_hv_type1)
			fault_msi_ctrl.value=noir_mmio_read32((u64)dmarm->bars[i].virt+mmio_offset_fault_control);
			if(fault_msi_ctrl.int_mask==false)
			{
				// This OS is IOMMU-aware. Save the vector.
				intel_iommu_msi_data_register fault_msi_data;
				fault_msi_data.value=noir_mmio_read32((u64)dmarm->bars[i].virt+mmio_offset_fault_data);
				dmarm->bars[i].fault_vector=fault_msi_data.vector;
				nv_dprintf("The host has already set up IOMMU Fault-Notification! Interrupt Vector=0x%02X\n",fault_msi_data.vector);
			}
			// Disable the Invalidation Queue.
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_queue_invalidation_enable,false);
			// Save the Root-Table Pointer set by the platform (OS, firmware, etc.)
			// Windows is IOMMU-aware, and sets all contexts to "Pass-Through" mode, even if you disabled Kernel DMA Protection.
			dmarm->bars[i].prev_root_ptr=noir_mmio_read64((u64)dmarm->bars[i].virt+mmio_offset_root_table_ptr);
#endif
			// Write the Root-Table Pointer...
			nv_dprintf("Setting Root-Table Pointer...\n");
			noir_mmio_write64((u64)dmarm->bars[i].virt+mmio_offset_root_table_ptr,root_ptr.value);
			// Inform the IOMMU unit that Root-Table Pointer is updated...
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_set_root_ptr,true);
			noir_int3();
			// Invalidate Context-Cache.
			nv_dprintf("Invalidating Context-Cache...\n");
			nvc_vt_iommu_flush_context_cache(&dmarm->bars[i],0,0,0,intel_iommu_context_inval_global);
			// Invalidate I/O TLB. Drain all R/W Requests by the way.
			nv_dprintf("Invalidating I/O TLB...\n");
			nvc_vt_iommu_flush_iotlb(&dmarm->bars[i],0,true,true,intel_iommu_iotlb_inval_global,0);
			// (Re)Enable the Invalidation Queue.
			noir_mmio_write64((u64)dmarm->bars[i].virt+mmio_offset_inval_queue_tail,0);
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_queue_invalidation_enable,true);
			// Enable DMA-Remapping.
			nv_dprintf("Enabling Translation...\n");
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_translation_enable,true);
			// Completed!
			dmarm->minimum_features.active=true;
			nv_dprintf("Completed Configuration for IOMMU at 0x%p!\n",dmarm->bars[i].phys);
		}
	}
}

void nvc_vt_iommu_finalize()
{
	noir_dmar_manager_p dmarm=hvm_p->relative_hvm->dmar_manager;
	if(dmarm)
	{
		// Reset all IOMMU Hardware...
		for(u64 i=0;i<dmarm->iommu_count;i++)
		{
			// Reset the IOMMU Root-Table Pointer.
			nv_dprintf("Resetting Root-Table Pointer...\n");
			noir_mmio_write64((u64)dmarm->bars[i].virt+mmio_offset_root_table_ptr,dmarm->bars[i].prev_root_ptr);
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_set_root_ptr,true);
			// Reset the Invalidation Queue...
			nv_dprintf("Reenabling Invalidation Queue...\n");
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_queue_invalidation_enable,false);
			// Invalidate Context-Cache.
			nv_dprintf("Invalidating Context-Cache...\n");
			nvc_vt_iommu_flush_context_cache(&dmarm->bars[i],0,0,0,intel_iommu_context_inval_global);
			// Invalidate I/O TLB. Drain all R/W Requests by the way.
			nv_dprintf("Invalidating I/O TLB...\n");
			nvc_vt_iommu_flush_iotlb(&dmarm->bars[i],0,true,true,intel_iommu_iotlb_inval_global,0);
			// (Re)Enable the Invalidation Queue.
			noir_mmio_write64((u64)dmarm->bars[i].virt+mmio_offset_inval_queue_tail,0);
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_queue_invalidation_enable,true);
			// Reset DMA-Remapping.
			nv_dprintf("Resetting Translation...\n");
			nvc_vt_iommu_set_global_command(&dmarm->bars[i],intel_iommu_global_translation_enable,true);
			// Completed!
			nv_dprintf("Completed Reconfiguration for IOMMU at 0x%p!\n",dmarm->bars[i].phys);
		}
		// Eventually, free all paging structures.
		nvc_vt_iommu_cleanup();
	}
}