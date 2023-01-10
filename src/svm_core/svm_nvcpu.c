/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the infrastructure for nested virtualization of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_nvcpu.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include <ci.h>
#include "svm_vmcb.h"
#include "svm_npt.h"
#include "svm_exit.h"
#include "svm_def.h"

// See "cache_list.md" for specification of cache list.

void static nvc_svm_reference_nested_vcpu_node_in_cache_list(noir_svm_nested_vcpu_p nvcpu,noir_svm_nested_vcpu_node_p node)
{
	// Remove the node from the middle.
	if(node->cache_list.prev)node->cache_list.prev->cache_list.next=node->cache_list.next;
	if(node->cache_list.next)node->cache_list.next->cache_list.prev=node->cache_list.prev;
	// Place the node at the head.
	node->cache_list.prev=null;
	node->cache_list.next=nvcpu->nodes.head;
	// Reset the head.
	nvcpu->nodes.head->cache_list.prev=node;
	nvcpu->nodes.head=node;
}

void static nvc_svm_remove_nested_vcpu_node_in_cache_list(noir_svm_nested_vcpu_p nvcpu,noir_svm_nested_vcpu_node_p node)
{
	// Remove the node from the middle.
	if(node->cache_list.prev)node->cache_list.prev->cache_list.next=node->cache_list.next;
	if(node->cache_list.next)node->cache_list.next->cache_list.prev=node->cache_list.prev;
	// Place the node at the tail.
	node->cache_list.next=null;
	node->cache_list.prev=nvcpu->nodes.tail;
	// Reset the tail.
	nvcpu->nodes.tail->cache_list.prev=node;
	nvcpu->nodes.tail=node;
}

void static nvc_svm_insert_nested_vcpu_node_to_cache_list(noir_svm_nested_vcpu_p nvcpu,u64 vmcb)
{
	noir_svm_nested_vcpu_node_p node=nvcpu->nodes.tail,tail=nvcpu->nodes.tail->cache_list.prev;
	// Reset the tail.
	tail->cache_list.next=null;
	nvcpu->nodes.tail=tail;
	// Set the node up.
	node->l2_vmcb=vmcb;
	node->cache_list.prev=null;
	node->cache_list.next=nvcpu->nodes.head;
	// Reset the head.
	nvcpu->nodes.head->cache_list.prev=node;
	nvcpu->nodes.head=node;
}

void nvc_svm_initialize_nested_vcpu_node_pool(noir_svm_nested_vcpu_p nvcpu)
{
	// Initialize the pointers.
	nvcpu->nodes.head=nvcpu->nodes.pool;
	nvcpu->nodes.tail=&nvcpu->nodes.pool[noir_svm_cached_nested_vmcb-1];
	// Initialize the list head.
	nvcpu->nodes.head->cache_list.prev=null;
	nvcpu->nodes.head->cache_list.next=&nvcpu->nodes.pool[1];
	nvcpu->nodes.head->l2_vmcb=maxu64;
	// Initialize the nodes in the middle.
	for(u32 i=1;i<noir_svm_cached_nested_vmcb-1;i++)
	{
		nvcpu->nodes.pool[i].cache_list.prev=&nvcpu->nodes.pool[i-1];
		nvcpu->nodes.pool[i].cache_list.next=&nvcpu->nodes.pool[i+1];
		nvcpu->nodes.pool[i].l2_vmcb=maxu64;
	}
	// Initialize the list tail.
	nvcpu->nodes.tail->cache_list.prev=&nvcpu->nodes.pool[noir_svm_cached_nested_vmcb-1];
	nvcpu->nodes.tail->cache_list.next=null;
	nvcpu->nodes.tail->l2_vmcb=maxu64;
}

noir_svm_nested_vcpu_node_p nvc_svm_insert_nested_vcpu_node(noir_svm_nested_vcpu_p nvcpu,u64 vmcb)
{
	nvc_svm_insert_nested_vcpu_node_to_cache_list(nvcpu,vmcb);
	return nvcpu->nodes.head;
}

noir_svm_nested_vcpu_node_p nvc_svm_search_nested_vcpu_node(noir_svm_nested_vcpu_p nvcpu,u64 vmcb)
{
	// FIXME: Use AVL balanced tree to reduce running time.
	for(noir_svm_nested_vcpu_node_p node=nvcpu->nodes.head;node;node=node->cache_list.next)
		if(vmcb>=node->l2_vmcb && vmcb<node->l2_vmcb+page_size)
			return node;
	return null;
}

void nvc_svmn_virtualize_entry_vmcb(noir_svm_vcpu_p vcpu,void* l2_vmcb,void* l1_vmcb)
{
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_interception)==false)
	{
		// Cached states for Interception Vectors are invalid.
		noir_svm_vmwrite32(l1_vmcb,intercept_access_cr,noir_svm_vmread32(l2_vmcb,intercept_access_cr));
		noir_svm_vmwrite32(l1_vmcb,intercept_access_dr,noir_svm_vmread32(l2_vmcb,intercept_access_dr));
		noir_svm_vmwrite32(l1_vmcb,intercept_exceptions,noir_svm_vmread32(l2_vmcb,intercept_exceptions));
		noir_svm_vmwrite32(l1_vmcb,intercept_instruction1,noir_svm_vmread32(l2_vmcb,intercept_instruction1));
		noir_svm_vmwrite16(l1_vmcb,intercept_instruction2,noir_svm_vmread16(l2_vmcb,intercept_instruction2));
		noir_svm_vmwrite16(l1_vmcb,intercept_write_cr_post,noir_svm_vmread16(l2_vmcb,intercept_write_cr_post));
		noir_svm_vmwrite32(l1_vmcb,intercept_instruction3,noir_svm_vmread32(l2_vmcb,intercept_instruction3));
		noir_svm_vmwrite16(l1_vmcb,pause_filter_threshold,noir_svm_vmread16(l2_vmcb,pause_filter_threshold));
		noir_svm_vmwrite16(l1_vmcb,pause_filter_count,noir_svm_vmread16(l2_vmcb,pause_filter_count));
		noir_svm_vmwrite64(l1_vmcb,tsc_offset,noir_svm_vmread64(l2_vmcb,tsc_offset));
		// Some interceptions must be taken, regardless of the settings from nested hypervisor.
		noir_svm_vmcb_bts32(l1_vmcb,intercept_instruction1,nvc_svm_intercept_vector1_shutdown);
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_iomsrpm)==false)
	{
		// Cached states for I/O and MSR Permission Map are invalid.
		noir_svm_vmwrite64(l1_vmcb,iopm_physical_address,noir_svm_vmread64(l2_vmcb,iopm_physical_address));
		noir_svm_vmwrite64(l1_vmcb,msrpm_physical_address,noir_svm_vmread64(l2_vmcb,msrpm_physical_address));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_asid)==false)
	{
		// ASID must be virtualized.
		u32 asid=noir_svm_vmread32(l2_vmcb,guest_asid);
		// Cached ASID is invalid.
		noir_svm_vmwrite32(l1_vmcb,guest_asid,asid!=0?asid+hvm_p->tlb_tagging.start:0);
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_tpr)==false)
	{
		// Virtualized Local APIC is invalid.
		noir_svm_vmwrite64(l1_vmcb,avic_control,noir_svm_vmread64(l2_vmcb,avic_control));
		if(noir_svm_vmcb_bt64(l2_vmcb,avic_control,nvc_svm_avic_control_vintr_mask))
		{
			// If V_INTR_MASKING is enabled, Guest RFLAGS.IF must be copied to the host.
			if(noir_svm_vmcb_bt32(vcpu->vmcb.virt,guest_rflags,amd64_rflags_if))
				noir_sti();
			else
				noir_cli();
		}
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_npt)==false)
	{
		// Nested Paging is invalid.
		noir_svm_vmwrite64(l1_vmcb,npt_control,noir_svm_vmread64(l2_vmcb,npt_control));
		noir_svm_vmwrite64(l1_vmcb,npt_cr3,noir_svm_vmread64(l2_vmcb,npt_cr3));
		noir_svm_vmwrite64(l1_vmcb,guest_pat,noir_svm_vmread64(l2_vmcb,guest_pat));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_control_reg)==false)
	{
		// Control Registers, excluding CR2, are invalid.
		noir_svm_vmwrite64(l1_vmcb,guest_cr0,noir_svm_vmread64(l2_vmcb,guest_cr0));
		noir_svm_vmwrite64(l1_vmcb,guest_cr3,noir_svm_vmread64(l2_vmcb,guest_cr3));
		noir_svm_vmwrite64(l1_vmcb,guest_cr4,noir_svm_vmread64(l2_vmcb,guest_cr4));
		noir_svm_vmwrite64(l1_vmcb,guest_efer,noir_svm_vmread64(l2_vmcb,guest_efer));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_debug_reg)==false)
	{
		// Debug Registers, including dr6 and dr7, are invalid.
		noir_svm_vmwrite64(l1_vmcb,guest_dr6,noir_svm_vmread64(l2_vmcb,guest_dr6));
		noir_svm_vmwrite64(l1_vmcb,guest_dr7,noir_svm_vmread64(l2_vmcb,guest_dr7));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_idt_gdt)==false)
	{
		// IDTR and GDTR are invalid.
		noir_svm_vmwrite32(l1_vmcb,guest_gdtr_limit,noir_svm_vmread32(l2_vmcb,guest_gdtr_limit));
		noir_svm_vmwrite64(l1_vmcb,guest_gdtr_base,noir_svm_vmread64(l2_vmcb,guest_gdtr_base));
		noir_svm_vmwrite32(l1_vmcb,guest_idtr_limit,noir_svm_vmread32(l2_vmcb,guest_idtr_limit));
		noir_svm_vmwrite64(l1_vmcb,guest_idtr_base,noir_svm_vmread64(l2_vmcb,guest_idtr_base));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_segment_reg)==false)
	{
		// Segment Registers, including cs, ds, es and ss, are invalid.
		noir_svm_vmwrite16(l1_vmcb,guest_cs_selector,noir_svm_vmread16(l2_vmcb,guest_cs_selector));
		noir_svm_vmwrite16(l1_vmcb,guest_cs_attrib,noir_svm_vmread16(l2_vmcb,guest_cs_attrib));
		noir_svm_vmwrite32(l1_vmcb,guest_cs_limit,noir_svm_vmread32(l2_vmcb,guest_cs_limit));
		noir_svm_vmwrite64(l1_vmcb,guest_cs_base,noir_svm_vmread64(l2_vmcb,guest_cs_base));
		noir_svm_vmwrite16(l1_vmcb,guest_ds_selector,noir_svm_vmread16(l2_vmcb,guest_ds_selector));
		noir_svm_vmwrite16(l1_vmcb,guest_ds_attrib,noir_svm_vmread16(l2_vmcb,guest_ds_attrib));
		noir_svm_vmwrite32(l1_vmcb,guest_ds_limit,noir_svm_vmread32(l2_vmcb,guest_ds_limit));
		noir_svm_vmwrite64(l1_vmcb,guest_ds_base,noir_svm_vmread64(l2_vmcb,guest_ds_base));
		noir_svm_vmwrite16(l1_vmcb,guest_es_selector,noir_svm_vmread16(l2_vmcb,guest_es_selector));
		noir_svm_vmwrite16(l1_vmcb,guest_es_attrib,noir_svm_vmread16(l2_vmcb,guest_es_attrib));
		noir_svm_vmwrite32(l1_vmcb,guest_es_limit,noir_svm_vmread32(l2_vmcb,guest_es_limit));
		noir_svm_vmwrite64(l1_vmcb,guest_es_base,noir_svm_vmread64(l2_vmcb,guest_es_base));
		noir_svm_vmwrite16(l1_vmcb,guest_ss_selector,noir_svm_vmread16(l2_vmcb,guest_ss_selector));
		noir_svm_vmwrite16(l1_vmcb,guest_ss_attrib,noir_svm_vmread16(l2_vmcb,guest_ss_attrib));
		noir_svm_vmwrite32(l1_vmcb,guest_ss_limit,noir_svm_vmread32(l2_vmcb,guest_ss_limit));
		noir_svm_vmwrite64(l1_vmcb,guest_ss_base,noir_svm_vmread64(l2_vmcb,guest_ss_base));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_cr2)==false)
		noir_svm_vmwrite64(l1_vmcb,guest_cr2,noir_svm_vmread64(l2_vmcb,guest_cr2));
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_lbr)==false)
	{
		noir_svm_vmwrite64(l1_vmcb,guest_debug_ctrl,noir_svm_vmread64(l2_vmcb,guest_debug_ctrl));
		noir_svm_vmwrite64(l1_vmcb,guest_last_branch_from,noir_svm_vmread64(l2_vmcb,guest_last_branch_from));
		noir_svm_vmwrite64(l1_vmcb,guest_last_branch_to,noir_svm_vmread64(l2_vmcb,guest_last_branch_to));
		noir_svm_vmwrite64(l1_vmcb,guest_last_exception_from,noir_svm_vmread64(l2_vmcb,guest_last_exception_from));
		noir_svm_vmwrite64(l1_vmcb,guest_last_exception_to,noir_svm_vmread64(l2_vmcb,guest_last_exception_to));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_avic)==false)
	{
		noir_svm_vmwrite64(l1_vmcb,avic_apic_bar,noir_svm_vmread64(l2_vmcb,avic_apic_bar));
		noir_svm_vmwrite64(l1_vmcb,avic_backing_page_pointer,noir_svm_vmread64(l2_vmcb,avic_backing_page_pointer));
		noir_svm_vmwrite64(l1_vmcb,avic_logical_table_pointer,noir_svm_vmread64(l2_vmcb,avic_logical_table_pointer));
		noir_svm_vmwrite64(l1_vmcb,avic_physical_table_pointer,noir_svm_vmread64(l2_vmcb,avic_physical_table_pointer));
	}
	if(noir_svm_vmcb_bt32(l2_vmcb,vmcb_clean_bits,noir_svm_clean_cet)==false)
	{
		noir_svm_vmwrite64(l1_vmcb,guest_s_cet,noir_svm_vmread64(l2_vmcb,guest_s_cet));
		noir_svm_vmwrite64(l1_vmcb,guest_ssp,noir_svm_vmread64(l2_vmcb,guest_ssp));
		noir_svm_vmwrite64(l1_vmcb,guest_isst,noir_svm_vmread64(l2_vmcb,guest_isst));
	}
	// Copy states that are never to be cached.
	noir_svm_vmwrite32(l1_vmcb,vmcb_clean_bits,noir_svm_vmread32(l2_vmcb,vmcb_clean_bits));
	noir_svm_vmwrite8(l1_vmcb,tlb_control,noir_svm_vmread8(l2_vmcb,tlb_control));
	noir_svm_vmwrite64(l1_vmcb,guest_interrupt,noir_svm_vmread64(l2_vmcb,guest_interrupt));
	noir_svm_vmwrite64(l1_vmcb,event_injection,noir_svm_vmread64(l2_vmcb,event_injection));
	noir_svm_vmwrite64(l1_vmcb,guest_rflags,noir_svm_vmread64(l2_vmcb,guest_rflags));
	noir_svm_vmwrite64(l1_vmcb,guest_rip,noir_svm_vmread64(l2_vmcb,guest_rip));
	noir_svm_vmwrite64(l1_vmcb,guest_rsp,noir_svm_vmread64(l2_vmcb,guest_rsp));
	noir_svm_vmwrite64(l1_vmcb,guest_rax,noir_svm_vmread64(l2_vmcb,guest_rax));
	noir_svm_vmwrite8(l1_vmcb,guest_cpl,noir_svm_vmread8(l2_vmcb,guest_cpl));
	// Transfer the state from vmload. Note that the state to be copied is not from L2 VMCB.
	noir_svm_vmwrite64(l1_vmcb,guest_fs_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_fs_selector));
	noir_svm_vmwrite64(l1_vmcb,guest_fs_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_fs_attrib));
	noir_svm_vmwrite64(l1_vmcb,guest_fs_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_fs_limit));
	noir_svm_vmwrite64(l1_vmcb,guest_fs_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_fs_base));
	noir_svm_vmwrite64(l1_vmcb,guest_gs_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_gs_selector));
	noir_svm_vmwrite64(l1_vmcb,guest_gs_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_gs_attrib));
	noir_svm_vmwrite64(l1_vmcb,guest_gs_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_gs_limit));
	noir_svm_vmwrite64(l1_vmcb,guest_gs_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_gs_base));
	noir_svm_vmwrite64(l1_vmcb,guest_tr_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_tr_selector));
	noir_svm_vmwrite64(l1_vmcb,guest_tr_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_tr_attrib));
	noir_svm_vmwrite64(l1_vmcb,guest_tr_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_tr_limit));
	noir_svm_vmwrite64(l1_vmcb,guest_tr_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_tr_base));
	noir_svm_vmwrite64(l1_vmcb,guest_ldtr_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_ldtr_selector));
	noir_svm_vmwrite64(l1_vmcb,guest_ldtr_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_ldtr_attrib));
	noir_svm_vmwrite64(l1_vmcb,guest_ldtr_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_ldtr_limit));
	noir_svm_vmwrite64(l1_vmcb,guest_ldtr_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_ldtr_base));
	noir_svm_vmwrite64(l1_vmcb,guest_star,noir_svm_vmread64(vcpu->vmcb.virt,guest_star));
	noir_svm_vmwrite64(l1_vmcb,guest_lstar,noir_svm_vmread64(vcpu->vmcb.virt,guest_lstar));
	noir_svm_vmwrite64(l1_vmcb,guest_cstar,noir_svm_vmread64(vcpu->vmcb.virt,guest_cstar));
	noir_svm_vmwrite64(l1_vmcb,guest_sfmask,noir_svm_vmread64(vcpu->vmcb.virt,guest_sfmask));
	noir_svm_vmwrite64(l1_vmcb,guest_kernel_gs_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_kernel_gs_base));
	// FIXME: Use separated vmload/vmrun to optimize the synchronization.
}

void nvc_svmn_virtualize_exit_vmcb(noir_svm_vcpu_p vcpu,void* l1_vmcb,void* l2_vmcb)
{
	// Copy the nested guest state to L2 VMCB.
	// Segment Registers...
	noir_svm_vmwrite16(l2_vmcb,guest_cs_selector,noir_svm_vmread16(l1_vmcb,guest_cs_selector));
	noir_svm_vmwrite16(l2_vmcb,guest_cs_attrib,noir_svm_vmread16(l1_vmcb,guest_cs_attrib));
	noir_svm_vmwrite32(l2_vmcb,guest_cs_limit,noir_svm_vmread32(l1_vmcb,guest_cs_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_cs_base,noir_svm_vmread64(l1_vmcb,guest_cs_base));
	noir_svm_vmwrite16(l2_vmcb,guest_ds_selector,noir_svm_vmread16(l1_vmcb,guest_ds_selector));
	noir_svm_vmwrite16(l2_vmcb,guest_ds_attrib,noir_svm_vmread16(l1_vmcb,guest_ds_attrib));
	noir_svm_vmwrite32(l2_vmcb,guest_ds_limit,noir_svm_vmread32(l1_vmcb,guest_ds_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_ds_base,noir_svm_vmread64(l1_vmcb,guest_ds_base));
	noir_svm_vmwrite16(l2_vmcb,guest_es_selector,noir_svm_vmread16(l1_vmcb,guest_es_selector));
	noir_svm_vmwrite16(l2_vmcb,guest_es_attrib,noir_svm_vmread16(l1_vmcb,guest_es_attrib));
	noir_svm_vmwrite32(l2_vmcb,guest_es_limit,noir_svm_vmread32(l1_vmcb,guest_es_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_es_base,noir_svm_vmread64(l1_vmcb,guest_es_base));
	noir_svm_vmwrite16(l2_vmcb,guest_ss_selector,noir_svm_vmread16(l1_vmcb,guest_ss_selector));
	noir_svm_vmwrite16(l2_vmcb,guest_ss_attrib,noir_svm_vmread16(l1_vmcb,guest_ss_attrib));
	noir_svm_vmwrite32(l2_vmcb,guest_ss_limit,noir_svm_vmread32(l1_vmcb,guest_ss_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_ss_base,noir_svm_vmread64(l1_vmcb,guest_ss_base));
	// Descriptor Tables..
	noir_svm_vmwrite32(l2_vmcb,guest_gdtr_limit,noir_svm_vmread32(l1_vmcb,guest_gdtr_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_gdtr_base,noir_svm_vmread64(l1_vmcb,guest_gdtr_base));
	noir_svm_vmwrite32(l2_vmcb,guest_idtr_limit,noir_svm_vmread32(l1_vmcb,guest_idtr_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_idtr_base,noir_svm_vmread64(l1_vmcb,guest_idtr_base));
	// Control Registers...
	noir_svm_vmwrite64(l2_vmcb,guest_cr0,noir_svm_vmread64(l1_vmcb,guest_cr0));
	noir_svm_vmwrite64(l2_vmcb,guest_cr2,noir_svm_vmread64(l1_vmcb,guest_cr2));
	noir_svm_vmwrite64(l2_vmcb,guest_cr3,noir_svm_vmread64(l1_vmcb,guest_cr3));
	noir_svm_vmwrite64(l2_vmcb,guest_cr4,noir_svm_vmread64(l1_vmcb,guest_cr4));
	noir_svm_vmwrite64(l2_vmcb,guest_efer,noir_svm_vmread64(l1_vmcb,guest_efer));
	// Debug Registers
	noir_svm_vmwrite64(l2_vmcb,guest_dr6,noir_svm_vmread64(l1_vmcb,guest_dr6));
	noir_svm_vmwrite64(l2_vmcb,guest_dr7,noir_svm_vmread64(l1_vmcb,guest_dr7));
	// Last Branch Record Registers
	noir_svm_vmwrite64(l2_vmcb,guest_debug_ctrl,noir_svm_vmread64(l1_vmcb,guest_debug_ctrl));
	noir_svm_vmwrite64(l2_vmcb,guest_last_branch_from,noir_svm_vmread64(l1_vmcb,guest_last_branch_from));
	noir_svm_vmwrite64(l2_vmcb,guest_last_branch_to,noir_svm_vmread64(l1_vmcb,guest_last_branch_to));
	noir_svm_vmwrite64(l2_vmcb,guest_last_exception_from,noir_svm_vmread64(l1_vmcb,guest_last_exception_from));
	noir_svm_vmwrite64(l2_vmcb,guest_last_exception_to,noir_svm_vmread64(l1_vmcb,guest_last_exception_to));
	// Shadow Stack Registers
	noir_svm_vmwrite64(l2_vmcb,guest_s_cet,noir_svm_vmread64(l1_vmcb,guest_s_cet));
	noir_svm_vmwrite64(l2_vmcb,guest_ssp,noir_svm_vmread64(l1_vmcb,guest_ssp));
	noir_svm_vmwrite64(l2_vmcb,guest_isst,noir_svm_vmread64(l1_vmcb,guest_isst));
	// Page Attributes Register
	noir_svm_vmwrite64(l2_vmcb,guest_pat,noir_svm_vmread64(l1_vmcb,guest_pat));
	// APIC Base Address Register
	noir_svm_vmwrite64(l2_vmcb,avic_apic_bar,noir_svm_vmread64(l1_vmcb,avic_apic_bar));
	// General-Purpose Registers
	noir_svm_vmwrite64(l2_vmcb,guest_interrupt,noir_svm_vmread64(l1_vmcb,guest_interrupt));
	noir_svm_vmwrite64(l2_vmcb,guest_rflags,noir_svm_vmread64(l1_vmcb,guest_rflags));
	noir_svm_vmwrite64(l2_vmcb,guest_rip,noir_svm_vmread64(l1_vmcb,guest_rip));
	noir_svm_vmwrite64(l2_vmcb,guest_rsp,noir_svm_vmread64(l1_vmcb,guest_rsp));
	noir_svm_vmwrite64(l2_vmcb,guest_rax,noir_svm_vmread64(l1_vmcb,guest_rax));
	noir_svm_vmwrite8(l2_vmcb,guest_cpl,noir_svm_vmread8(l1_vmcb,guest_cpl));
	// Copy the VM-Exit information.
	noir_svm_vmwrite64(l2_vmcb,exit_code,noir_svm_vmread64(l1_vmcb,exit_code));
	noir_svm_vmwrite64(l2_vmcb,exit_info1,noir_svm_vmread64(l1_vmcb,exit_info1));
	noir_svm_vmwrite64(l2_vmcb,exit_info2,noir_svm_vmread64(l1_vmcb,exit_info2));
	noir_svm_vmwrite64(l2_vmcb,exit_interrupt_info,noir_svm_vmread64(l1_vmcb,exit_interrupt_info));
	noir_svm_vmwrite64(l2_vmcb,next_rip,noir_svm_vmread64(l1_vmcb,next_rip));
	noir_svm_vmwrite64(l2_vmcb,avic_control,noir_svm_vmread64(l1_vmcb,avic_control));
	noir_svm_vmwrite64(l2_vmcb,event_injection,noir_svm_vmread64(l1_vmcb,event_injection));
	// Transfer the state from vmsave. Note that the state to be copied is not from L2 VMCB.
	noir_svm_vmwrite64(l2_vmcb,guest_fs_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_fs_selector));
	noir_svm_vmwrite64(l2_vmcb,guest_fs_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_fs_attrib));
	noir_svm_vmwrite64(l2_vmcb,guest_fs_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_fs_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_fs_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_fs_base));
	noir_svm_vmwrite64(l2_vmcb,guest_gs_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_gs_selector));
	noir_svm_vmwrite64(l2_vmcb,guest_gs_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_gs_attrib));
	noir_svm_vmwrite64(l2_vmcb,guest_gs_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_gs_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_gs_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_gs_base));
	noir_svm_vmwrite64(l2_vmcb,guest_tr_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_tr_selector));
	noir_svm_vmwrite64(l2_vmcb,guest_tr_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_tr_attrib));
	noir_svm_vmwrite64(l2_vmcb,guest_tr_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_tr_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_tr_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_tr_base));
	noir_svm_vmwrite64(l2_vmcb,guest_ldtr_selector,noir_svm_vmread16(vcpu->vmcb.virt,guest_ldtr_selector));
	noir_svm_vmwrite64(l2_vmcb,guest_ldtr_attrib,noir_svm_vmread16(vcpu->vmcb.virt,guest_ldtr_attrib));
	noir_svm_vmwrite64(l2_vmcb,guest_ldtr_limit,noir_svm_vmread32(vcpu->vmcb.virt,guest_ldtr_limit));
	noir_svm_vmwrite64(l2_vmcb,guest_ldtr_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_ldtr_base));
	noir_svm_vmwrite64(l2_vmcb,guest_star,noir_svm_vmread64(vcpu->vmcb.virt,guest_star));
	noir_svm_vmwrite64(l2_vmcb,guest_lstar,noir_svm_vmread64(vcpu->vmcb.virt,guest_lstar));
	noir_svm_vmwrite64(l2_vmcb,guest_cstar,noir_svm_vmread64(vcpu->vmcb.virt,guest_cstar));
	noir_svm_vmwrite64(l2_vmcb,guest_sfmask,noir_svm_vmread64(vcpu->vmcb.virt,guest_sfmask));
	noir_svm_vmwrite64(l2_vmcb,guest_kernel_gs_base,noir_svm_vmread64(vcpu->vmcb.virt,guest_kernel_gs_base));
	// FIXME: Use separated vmsave/vmrun to optimize the synchronization.
}

void nvc_svmn_set_gif(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,void* target_vmcb)
{
	void* vmcb=vcpu->vmcb.virt;
	// De-intercept Non-Maskable Interrupts.
	noir_svm_vmcb_btr32(vmcb,intercept_instruction1,nvc_svm_intercept_vector1_nmi);
	// De-intercept #MC and #DB Exceptions.
	noir_svm_vmcb_btr32(vmcb,intercept_exceptions,amd64_debug_exception);
	noir_svm_vmcb_btr32(vmcb,intercept_exceptions,amd64_machine_check);
	// Reset global physical interrupt masking in Local APIC Control.
	noir_svm_vmcb_btr32(vmcb,avic_control,nvc_svm_avic_control_vintr_mask);
	// Mark the relevant fields in VMCB as dirty.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_interception);
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_tpr);
	// Release the interrupts, according to the priority defined by AMD64 architecture.
	if(vcpu->nested_hvm.pending_mc)					// Priority 0
	{
		vcpu->nested_hvm.pending_mc=0;
		noir_svm_inject_event(target_vmcb,amd64_machine_check,amd64_fault_trap_exception,false,true,0);
	}
	else
	{
		if(vcpu->nested_hvm.pending_init)			// Priority 1
		{
			vcpu->nested_hvm.pending_init=0;
			if(vcpu->nested_hvm.r_init)
				noir_svm_inject_event(target_vmcb,amd64_security_exception,amd64_fault_trap_exception,true,true,amd64_sx_init_redirection);
			else
				nvc_svm_emulate_init_signal(gpr_state,target_vmcb,vcpu->cpuid_fms);
		}
		else
		{
			if(vcpu->nested_hvm.pending_db)			// Priority 2
			{
				vcpu->nested_hvm.pending_db=0;
				noir_svm_inject_event(target_vmcb,amd64_debug_exception,amd64_fault_trap_exception,false,true,0);
			}
			else
			{
				if(vcpu->nested_hvm.pending_nmi)	// Priority 3
				{
					vcpu->nested_hvm.pending_nmi=0;
					noir_svm_inject_event(target_vmcb,amd64_nmi_interrupt,amd64_non_maskable_interrupt,false,true,0);
				}
			}
		}
	}
}

void nvc_svmn_clear_gif(noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// Intercept Non-Maskable Interrupts.
	noir_svm_vmcb_bts32(vmcb,intercept_instruction1,nvc_svm_intercept_vector1_nmi);
	// Intercept #MC and #DB Exceptions.
	noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_debug_exception);
	noir_svm_vmcb_bts32(vmcb,intercept_exceptions,amd64_machine_check);
	// Set global physical interrupt masking in Local APIC Control.
	noir_svm_vmcb_bts32(vmcb,avic_control,nvc_svm_avic_control_vintr_mask);
	// Enable Masking of Physical Interrupts.
	noir_cli();
	// When V_INTR_MASKING is set and host EFLAGS.IF is cleared, physical
	// interrupts will be permanently blocked by the processor.
	// Mark the relevant fields in VMCB as dirty.
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_interception);
	noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_tpr);
}

/*
  Handling VM-Exits from Nested Guest...
*/

// Default handler of VM-Exit.
void static fastcall nvc_svm_nvexit_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu)
{
	;
}

// Expected Intercept Code: 0x7F
void static fastcall nvc_svm_nvexit_shutdown_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu)
{
	noir_int3();
}