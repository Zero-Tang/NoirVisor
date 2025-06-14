/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the AMD-Vi MMIO driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::*;
use paste::*;

#[derive(Default)]
#[repr(C)] pub struct DeviceTableBaseRegister(pub u64);

impl DeviceTableBaseRegister
{
	build_int_mut_method!(size,0,9,u64);
	build_int_mut_method!(base,12,40,u64);
}

#[derive(Default)]
#[repr(C)] pub struct CommandBufferBaseRegister(pub u64);

impl CommandBufferBaseRegister
{
	build_int_mut_method!(base,12,40,u64);
	build_int_mut_method!(length,56,4,u64);
}

#[derive(Default)]
#[repr(C)] pub struct EventLogBaseRegister(pub u64);

impl EventLogBaseRegister
{
	build_int_mut_method!(base,12,40,u64);
	build_int_mut_method!(length,56,4,u64);
}

#[derive(Default)]
#[repr(C)] pub struct IommuControlRegister(pub u64);

impl IommuControlRegister
{
	build_bit_mut_method!(iommu_enable,0);
	build_bit_mut_method!(httun_enable,1);
	build_bit_mut_method!(event_log_enable,2);
	build_bit_mut_method!(event_int_enable,3);
	build_bit_mut_method!(comwait_int_enable,4);
	build_int_mut_method!(invl_timeout,5,3,u64);
	build_bit_mut_method!(pass_posted_writes,8);
	build_bit_mut_method!(resp_pass_posted_writes,9);
	build_bit_mut_method!(coherent,10);
	build_bit_mut_method!(isochronous,11);
	build_bit_mut_method!(cmd_buff_enable,12);
	build_bit_mut_method!(ppr_log_enable,13);
	build_bit_mut_method!(ppr_int_enable,14);
	build_bit_mut_method!(ppr_proc_enable,15);
	build_bit_mut_method!(guest_trans_enable,16);
	build_bit_mut_method!(guest_vapic_enable,17);
	build_bit_mut_method!(smi_filter_enable,22);
	build_bit_mut_method!(self_wb_enable,23);
	build_bit_mut_method!(smi_flt_log_enable,24);
	build_int_mut_method!(guest_vapic_mode_enabled,25,3,u64);
	build_bit_mut_method!(guest_vapic_log_enabled,28);
	build_bit_mut_method!(guest_vapic_int_enabled,29);
	build_int_mut_method!(dual_ppr_log_enabled,30,2,u64);
	build_int_mut_method!(dual_event_log_enabled,32,2,u64);
	build_int_mut_method!(dev_table_segments,34,3,u64);
	build_int_mut_method!(privilege_abort_enable,37,2,u64);
	build_bit_mut_method!(ppr_auto_resp_enable,39);
	build_bit_mut_method!(marc_enable,40);
	build_bit_mut_method!(block_stopmark_enable,41);
	build_bit_mut_method!(ppr_auto_resp_always_on,42);
	build_bit_mut_method!(enhanced_ppr_handle,45);
	build_int_mut_method!(host_ad_bits_update,46,2,u64);
	build_bit_mut_method!(disable_guest_dirty_bit,48);
	build_bit_mut_method!(x2apic_enable,50);
	build_bit_mut_method!(x2apic_int_gen,51);
	build_bit_mut_method!(vcmd_buffer_processing,52);
	build_bit_mut_method!(viommu_enable,53);
	build_bit_mut_method!(disable_guest_accessed_bit,54);
	build_bit_mut_method!(gapic_phys_apic_int_enable,55);
	build_bit_mut_method!(tiered_mem_page_migration,56);
	build_bit_mut_method!(gcr3_trp_mode,58);
	build_bit_mut_method!(irte_cache_disable,59);
	build_bit_mut_method!(gst_buffer_trp_mode,60);
	build_int_mut_method!(snp_avic_enabled,61,3,u64);
}

#[derive(Default)]
#[repr(C)] pub struct IommuExclusionBaseRegister(pub u64);

impl IommuExclusionBaseRegister
{
	build_bit_mut_method!(exclusion_enable,0);
	build_bit_mut_method!(allow_all_devices,1);
	build_int_mut_method!(base,12,40,u64);
}

#[derive(Default)]
#[repr(C)] pub struct IommuExtendedFeatureRegister(pub u64);

impl IommuExtendedFeatureRegister
{
	build_bit_mut_method!(pref_sup,0);
	build_bit_mut_method!(ppr_sup,1);
	build_bit_mut_method!(xt_sup,2);
	build_bit_mut_method!(nx_sup,3);
	build_bit_mut_method!(gt_sup,4);
	build_bit_mut_method!(gappi_sup,5);
	build_bit_mut_method!(ia_sup,6);
	build_bit_mut_method!(ga_sup,7);
	build_bit_mut_method!(he_sup,8);
	build_bit_mut_method!(pc_sup,9);
	build_int_mut_method!(hats,10,2,u64);
	build_int_mut_method!(gats,12,2,u64);
	build_int_mut_method!(glx_sup,14,2,u64);
	build_int_mut_method!(smif_sup,16,2,u64);
	build_int_mut_method!(smif_rc,18,3,u64);
	build_int_mut_method!(gam_sup,21,3,u64);
	build_int_mut_method!(dual_ppr_log_sup,24,2,u64);
	build_int_mut_method!(dual_evt_log_sup,28,2,u64);
	build_bit_mut_method!(sats_sup,31);
	build_int_mut_method!(pas_max,32,5,u64);
	build_bit_mut_method!(us_sup,37);
	build_int_mut_method!(dev_tbl_seg_sup,38,2,u64);
	build_bit_mut_method!(ppr_ovrflw_early_sup,40);
	build_bit_mut_method!(ppr_auto_rsp_sup,41);
	build_int_mut_method!(marc_sup,42,2,u64);
	build_bit_mut_method!(blk_stop_mrk_sup,44);
	build_bit_mut_method!(perf_opt_sup,45);
	build_bit_mut_method!(msi_cap_mmio_sup,46);
	build_bit_mut_method!(gio_sup,48);
	build_bit_mut_method!(ha_sup,49);
	build_bit_mut_method!(eph_sup,50);
	build_bit_mut_method!(attr_fw_sup,51);
	build_bit_mut_method!(hd_sup,52);
	build_bit_mut_method!(inv_iotlb_type_sup,54);
	build_bit_mut_method!(viommu_sup,55);
	build_bit_mut_method!(ga_update_dis_sup,61);
	build_bit_mut_method!(force_phy_dest_sup,62);
	build_bit_mut_method!(snp_sup,63);
}

#[derive(Default)]
#[repr(C)] pub struct PprLogBaseRegister(pub u64);

impl PprLogBaseRegister
{
	build_int_mut_method!(base,12,40,u64);
	build_int_mut_method!(length,56,4,u64);
}

#[derive(Debug)]
pub enum SvmIommuCommand
{
	CompletionWait
	{
		store_address:u64,
		store_data:u64,
		completion_store:bool,
		incompletion_interrupt:bool,
		flush_queue:bool
	},
	InvalidateDeviceTableEntry
	{
		device_id:u16
	},
	InvalidateIommuPages
	{
		address:u64,
		pasid:u32,
		domain_id:u16,
		is_page_size:bool,
		is_pde:bool,
		is_guest_or_nested:bool
	},
	InvalidateIotlbPages
	{
		address:u64,
		pasid:u32,
		device_id:u16,
		queue_id:u16,
		maxpend:u8,
		inval_type:u8,
		is_page_size:bool,
		is_guest_or_nested:bool,
	},
	InvalidateInterruptTable
	{
		device_id:u16
	},
	PrefetchIommuPages
	{
		address:u64,
		pasid:u32,
		device_id:u16,
		prefetch_count:u8,
		is_page_size:bool,
		is_guest_or_nested:bool,
		inval_then_prefetch:bool
	},
	CompletePprRequest
	{
		pasid:u32,
		completion_tag:u16,
		device_id:u16
	},
	InvalidateIommuAll,
	InsertGuestEvent
	{
		guest_id:u16
	},
	ResetVmmio
	{
		guest_id:u16,
		reset_all:bool,
		vcmd:bool
	}
}

impl SvmIommuCommand
{
	pub fn into_raw(self)->[u32;4]
	{
		use SvmIommuCommand::*;
		match self
		{
			CompletionWait{store_address,store_data,completion_store,incompletion_interrupt,flush_queue}=>
			{
				let mut raw:[u32;4]=[(store_address&0xffffffff) as u32,(store_address>>32) as u32,(store_data&0xffffffff) as u32,(store_data>>32) as u32];
				// Fill in the boolean values.
				if completion_store {raw[0]|=0x1;}
				if incompletion_interrupt {raw[0]|=0x2;}
				if flush_queue {raw[0]|=0x4;}
				// Mark as COMPLETION_WAIT command.
				raw[1]|=0x10000000;
				raw
			}
			InvalidateIommuAll=>[0,0x80000000,0,0],
			_=>panic!("Raw conversion for {self:?} is not implemented!")
		}
	}
}

pub const MMIO_BASE_DEVICE_TABLE_BASE:usize=0x0000;
pub const MMIO_BASE_COMMAND_BUFFER_BASE:usize=0x0008;
pub const MMIO_BASE_EVENT_LOG_BUFFER_BASE:usize=0x0010;
pub const MMIO_BASE_IOMMU_CONTROL_REGISTER:usize=0x0018;
pub const MMIO_BASE_COMMAND_BUFFER_HEAD:usize=0x2000;
pub const MMIO_BASE_COMMAND_BUFFER_TAIL:usize=0x2008;
pub const MMIO_BASE_EVENT_LOG_BUFFER_HEAD:usize=0x2010;
pub const MMIO_BASE_EVENT_LOG_BUFFER_TAIL:usize=0x2018;