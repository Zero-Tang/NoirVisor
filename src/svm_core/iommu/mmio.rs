/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the AMD-Vi MMIO driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use bitfield_struct::bitfield;

#[bitfield(u64)] pub struct DeviceTableBaseRegister
{
	#[bits(9)] pub size:u64,
	#[bits(3)] rsvd0:u64,
	#[bits(40)] pub base:u64,
	#[bits(12)] rsvd1:u64
}

#[bitfield(u64)] pub struct CommandBufferBaseRegister
{
	#[bits(12)] rsvd0:u64,
	#[bits(40)] pub base:u64,
	#[bits(4)] rsvd1:u64,
	#[bits(4)] pub length:u64,
	#[bits(4)] rsvd2:u64
}

#[bitfield(u64)] pub struct EventLogBaseRegister
{
	#[bits(12)] rsvd0:u64,
	#[bits(40)] pub base:u64,
	#[bits(4)] rsvd1:u64,
	#[bits(4)] pub length:u64,
	#[bits(4)] rsvd2:u64
}

#[bitfield(u64)] pub struct IommuControlRegister
{
	pub iommu_enable:bool,
	pub httun_enable:bool,
	pub event_log_enable:bool,
	pub event_int_enable:bool,
	pub comwait_int_enable:bool,
	#[bits(3)] pub invl_timeout:u64,
	pub pass_posted_writes:bool,
	pub resp_pass_posted_writes:bool,
	pub coherent:bool,
	pub isochronous:bool,
	pub cmd_buff_enable:bool,
	pub ppr_log_enable:bool,
	pub ppr_int_enable:bool,
	pub ppr_proc_enable:bool,
	pub guest_trans_enable:bool,
	pub guest_vapic_enable:bool,
	#[bits(4)] pub crw:u64,
	pub smi_filter_enable:bool,
	pub self_wb_enable:bool,
	pub smi_flt_log_enable:bool,
	#[bits(3)] pub guest_vapic_mode_enabled:u64,
	pub guest_vapic_log_enabled:bool,
	pub guest_vapic_int_enabled:bool,
	#[bits(2)] pub dual_ppr_log_enabled:u64,
	#[bits(2)] pub dual_event_log_enabled:u64,
	#[bits(3)] pub dev_table_segments:u64,
	#[bits(2)] pub privilege_abort_enable:u64,
	pub ppr_auto_resp_enable:bool,
	pub marc_enable:bool,
	pub block_stopmark_enable:bool,
	pub ppr_auto_resp_always_on:bool,
	#[bits(2)] pub num_int_remap_mode:u64,
	pub enhanced_ppr_handling:bool,
	#[bits(2)] pub host_ad_bits_update:u64,
	pub disable_guest_ad_bit:bool,
	rsvd0:bool,
	pub x2apic_enable:bool,
	pub x2apic_int_gen:bool,
	pub vcmd_buffer_processing:bool,
	pub viommu_enable:bool,
	pub disable_guest_accessed_bit:bool,
	pub gapic_phys_apic_int_enable:bool,
	pub tiered_mem_page_migration:bool,
	rsvd1:bool,
	pub gcr3_trp_mode:bool,
	pub irte_cache_disable:bool,
	pub gst_buffer_trp_mode:bool,
	#[bits(3)] pub snp_avic_enabled:u64
}

#[bitfield(u64)] pub struct IommuExclusionBaseRegister
{
	pub exclusion_enable:bool,
	pub allow_all_devices:bool,
	#[bits(10)] rsvd0:u64,
	#[bits(40)] pub base:u64,
	#[bits(12)] rsvd1:u64
}

#[bitfield(u64)] pub struct IommuExtendedFeatureRegister
{
	pub pref_sup:bool,
	pub ppr_sup:bool,
	pub xt_sup:bool,
	pub nx_sup:bool,
	pub gt_sup:bool,
	pub gappi_sup:bool,
	pub ia_sup:bool,
	pub ga_sup:bool,
	pub he_sup:bool,
	pub pc_sup:bool,
	#[bits(2)] pub hats:u64,
	#[bits(2)] pub gats:u64,
	#[bits(2)] pub glx_sup:u64,
	#[bits(2)] pub smif_sup:u64,
	#[bits(3)] pub smif_rc:u64,
	#[bits(3)] pub gam_sup:u64,
	#[bits(2)] pub dual_ppr_log_sup:u64,
	#[bits(2)] rsvd0:u64,
	#[bits(2)] pub dual_evt_log_sup:u64,
	rsvd1:bool,
	pub sats_sup:bool,
	#[bits(5)] pub pas_max:u64,
	pub us_sup:bool,
	#[bits(2)] pub dev_tbl_seg_sup:u64,
	pub ppr_ovrflw_early_sup:bool,
	pub ppr_auto_resp_sup:bool,
	#[bits(2)] pub marc_sup:u64,
	pub blk_stop_mrk_sup:bool,
	pub perf_opt_sup:bool,
	pub msi_cap_mmio_sup:bool,
	rsvd2:bool,
	pub gio_sup:bool,
	pub ha_sup:bool,
	pub eph_sup:bool,
	pub attr_fw_sup:bool,
	pub hd_sup:bool,
	rsvd3:bool,
	pub inv_iotlb_type_sup:bool,
	pub viommu_sup:bool,
	#[bits(5)] rsvd4:u64,
	pub ga_update_dis_sup:bool,
	pub force_phy_dest_sup:bool,
	pub snp_sup:bool
}

#[bitfield(u64)] pub struct PprLogBaseRegister
{
	#[bits(12)] rsvd0:u64,
	#[bits(40)] pub base:u64,
	#[bits(4)] rsvd1:u64,
	#[bits(4)] pub length:u64,
	#[bits(4)] rsvd2:u64
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
				let mut raw:[u32;4]=[store_address as u32,(store_address>>32) as u32,store_data as u32,(store_data>>32) as u32];
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