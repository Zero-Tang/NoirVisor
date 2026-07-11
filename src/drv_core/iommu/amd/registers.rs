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

use core::ffi::c_void;

use bitfield_struct::bitfield;

use crate::xpf_core::asm::io::{mmio_read, mmio_write};

pub(super) trait IommuRegister
{
	const MMIO_OFFSET:usize;

	unsafe fn read(base:*const c_void)->Self;
	unsafe fn write(self,base:*mut c_void);
}

/// Derives the implementation of `IommuRegister` trait.
macro_rules! derive_mmio_rw
{
	($type:ty,$offset:literal)=>
	{
		impl IommuRegister for $type
		{
			const MMIO_OFFSET:usize=$offset;

			unsafe fn read(base:*const c_void)->Self
			{
				unsafe
				{
					mmio_read(base.byte_add($offset).cast())
				}
			}

			unsafe fn write(self,base:*mut c_void)
			{
				unsafe
				{
					mmio_write(base.byte_add($offset).cast(),self);
				}
			}
		}
	};
}

#[bitfield(u64)] pub struct DeviceTableBaseRegister
{
	#[bits(9)] pub size:u64,
	#[bits(3)] rsvd0:u64,
	#[bits(40)] pub dev_tab_base:u64,
	#[bits(12)] rsvd1:u64
}

derive_mmio_rw!(DeviceTableBaseRegister,0x0000);

#[bitfield(u64)] pub struct CommandBufferBaseRegister
{
	#[bits(12)] rsvd0:u64,
	#[bits(40)] pub com_base:u64,
	#[bits(4)] rsvd1:u64,
	#[bits(4)] pub com_len:u64,
	#[bits(4)] rsvd2:u64
}

derive_mmio_rw!(CommandBufferBaseRegister,0x0008);

#[bitfield(u64)] pub struct EventLogBaseRegister
{
	#[bits(12)] rsvd0:u64,
	#[bits(40)] pub event_base:u64,
	#[bits(4)] rsvd1:u64,
	#[bits(4)] pub event_len:u64,
	#[bits(4)] rsvd2:u64
}

derive_mmio_rw!(EventLogBaseRegister,0x0010);

#[bitfield(u64)] pub struct IommuControlRegister
{
	pub iommu_en:bool,
	pub ht_tun_en:bool,
	pub event_log_en:bool,
	pub event_int_en:bool,
	pub com_wait_int_en:bool,
	#[bits(3)] pub inv_timeout:u64,
	pub pass_pw:bool,
	pub res_pass_pw:bool,
	pub coherent:bool,
	pub isoc:bool,
	pub cmd_buff_en:bool,
	pub ppr_log_en:bool,
	pub ppr_int_en:bool,
	pub ppr_en:bool,
	pub gt_en:bool,
	pub ga_en:bool,
	#[bits(4)] pub crw:u64,
	pub smi_f_en:bool,
	pub slf_wb_en:bool,
	pub smi_f_log_en:bool,
	#[bits(3)] pub gam_en:u64,
	pub ga_log_en:bool,
	pub ga_int_en:bool,
	#[bits(2)] pub dual_ppr_log_en:u64,
	#[bits(2)] pub dual_event_log_en:u64,
	#[bits(3)] pub dev_tbl_seg_en:u64,
	#[bits(2)] pub priv_abrt_en:u64,
	pub ppr_auto_rsp_en:bool,
	pub marc_en:bool,
	pub blk_stop_mrk_enable:bool,
	pub ppr_auto_rsp_aon:bool,
	#[bits(2)] pub num_int_remap_mode:u64,
	pub eph_en:bool,
	#[bits(2)] pub had_update:u64,
	pub gd_update_dis:bool,
	rsvd0:bool,
	pub xt_en:bool,
	pub int_cap_xt_en:bool,
	pub vcmd_en:bool,
	pub viommu_en:bool,
	pub ga_update_dis:bool,
	pub gappi_en:bool,
	pub tmpm_en:bool,
	rsvd1:bool,
	pub gcr3_trp_mode:bool,
	pub irte_cache_dis:bool,
	pub gst_buffer_trp_mode:bool,
	#[bits(3)] pub snp_avic_en:u64
}

derive_mmio_rw!(IommuControlRegister,0x0018);

#[bitfield(u64)] pub struct IommuExclusionRangeBaseRegister
{
	pub ex_en:bool,
	pub allow:bool,
	#[bits(10)] rsvd0:u64,
	#[bits(40)] pub exclusion_base:u64,
	#[bits(12)] rsvd1:u64
}

derive_mmio_rw!(IommuExclusionRangeBaseRegister,0x0020);

#[bitfield(u64)] pub struct IommuExclusionRangeLimitRegister
{
	#[bits(12)] rsvd0:u64,
	#[bits(40)] pub exclusion_limit:u64,
	#[bits(12)] rsvd1:u64
}

derive_mmio_rw!(IommuExclusionRangeLimitRegister,0x0028);

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

derive_mmio_rw!(IommuExtendedFeatureRegister,0x0030);

#[bitfield(u64)] pub struct PprLogBaseRegister
{
	#[bits(12)] rsvd0:u64,
	#[bits(40)] pub ppr_log_base:u64,
	#[bits(4)] rsvd1:u64,
	#[bits(4)] pub ppr_log_len:u64,
	#[bits(4)] rsvd2:u64
}

derive_mmio_rw!(PprLogBaseRegister,0x0038);

#[bitfield(u64)] pub struct CommandBufferHeadPointerRegister
{
	#[bits(4)] rsvd0:u64,
	#[bits(15)] pub cmd_head_ptr:usize,
	#[bits(45)] rsvd1:u64
}

derive_mmio_rw!(CommandBufferHeadPointerRegister,0x2000);

#[bitfield(u64)] pub struct CommandBufferTailPointerRegister
{
	#[bits(4)] rsvd0:u64,
	#[bits(15)] pub cmd_tail_ptr:usize,
	#[bits(45)] rsvd1:u64
}

derive_mmio_rw!(CommandBufferTailPointerRegister,0x2008);

#[bitfield(u64)] pub struct EventBufferHeadPointerRegister
{
	#[bits(4)] rsvd0:u64,
	#[bits(15)] pub event_head_ptr:usize,
	#[bits(45)] rsvd1:u64
}

derive_mmio_rw!(EventBufferHeadPointerRegister,0x2010);

#[bitfield(u64)] pub struct EventBufferTailPointerRegister
{
	#[bits(4)] rsvd0:u64,
	#[bits(15)] pub event_tail_ptr:usize,
	#[bits(45)] rsvd1:u64
}

derive_mmio_rw!(EventBufferTailPointerRegister,0x2018);

#[bitfield(u64)] pub struct IommuStatusRegister
{
	pub event_overflow:bool,
	pub event_log_int:bool,
	pub com_wait_int:bool,
	pub event_log_run:bool,
	pub cmd_buf_run:bool,
	pub ppr_overflow:bool,
	pub ppr_int:bool,
	pub ppr_log_run:bool,
	pub ga_log_run:bool,
	pub gal_overflow:bool,
	pub ga_int:bool,
	pub ppr_ovrflw_b:bool,
	pub ppr_log_active:bool,
	#[bits(2)] rsvd0:u64,
	pub event_ovrflw_b:bool,
	pub event_log_active:bool,
	pub ppr_ovrflw_early_b:bool,
	pub ppr_ovrflw_early:bool,
	#[bits(45)] rsvd1:u64
}

derive_mmio_rw!(IommuStatusRegister,0x2020);
