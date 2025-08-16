/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the AMD-Vi driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, hint::spin_loop, slice};
use alloc::vec::Vec;

use bitfield_struct::bitfield;
use log::*;

use acpi::{Ivhd, IvhdLarge};
use paging::{SvmIommuPmlManager, SvmIommuPte};
use crate::{svm_core::iommu::mmio::*, xpf_core::{asm::io::{mmio_read, mmio_write}, ci::CI_MANAGER}, *};
use drv_core::acpi::{search_acpi_table, tables::{AcpiSystemDescriptorSignature, IoVirtualizationReportingStructure}};
use xpf_core::{nvbdk::*, nvstatus::*, ioflt::IoRegion, allocator::{alloc_2mb_page, alloc_contd_pages, free_contd_pages}};
use mmio::DeviceTableBaseRegister;

mod acpi;
mod paging;
mod mmio;

pub(super) fn svm_iommu_output_handler(_region:&IoRegion<u64>,address:u64,size:u64,value:*const c_void,_context:*mut c_void)
{
	debug!("Intercepted writes to IOMMU! Address=0x{address:X}, Size: {size}");
	// Current implementation is simply pass-thru.
	unsafe
	{
		match size
		{
			1=>(address as *mut u8).write(value.cast::<u8>().read()),
			2=>(address as *mut u16).write(value.cast::<u16>().read()),
			4=>(address as *mut u32).write(value.cast::<u32>().read()),
			8=>(address as *mut u64).write(value.cast::<u64>().read()),
			_=>panic!("Unknown size: {size}!")
		}
	}
}

pub struct SvmIommuManager
{
	/// This `Vec` must be sorted in the order of physical address.
	pub iommu_bars:Vec<SvmIommuBar>,
	pub device_table:MemoryDescriptor,
	pub pml4e:MemoryDescriptor,
	pub pml3e:Vec<SvmIommuPmlManager>,
	pub pml2e:Vec<SvmIommuPmlManager>,
	pub pml1e:Vec<SvmIommuPmlManager>
}

impl Default for SvmIommuManager
{
	fn default() -> Self
	{
		Self
		{
			iommu_bars:Vec::new(),
			device_table:MemoryDescriptor::null(),
			pml4e:MemoryDescriptor::null(),
			pml3e:Vec::new(),
			pml2e:Vec::new(),
			pml1e:Vec::new()
		}
	}
}

impl SvmIommuManager
{
	pub fn build_manager()->Result<SvmIommuManager,Status>
	{
		let mut mgr:Self=Self::default();
		// Enumerate the ACPI and find all IOMMU BARs.
		search_acpi_table(AcpiSystemDescriptorSignature::IO_VIRTUALIZATION_REPORTING_STRUCTURE,|x|
		{
			let ivrs:*const IoVirtualizationReportingStructure=x.cast();
			let mut ivd:*const u8=unsafe{(*ivrs).ivdb.as_ptr()};
			let end:*const u8=unsafe{ivrs.byte_add((*ivrs).header.get_length() as usize).cast()};
			while ivd<end
			{
				debug!("Found I/O Virtualization Definition Type 0x{:X} at {ivd:p}!",unsafe{*ivd});
				let len:usize=match unsafe{*ivd}
				{
					0x10|0x11=>
					{
						let ivhd:*const Ivhd=ivd.cast();
						let bar_phys:u64=unsafe{(*ivhd).iommu_base_pa};
						mgr.add_bar(bar_phys);
						unsafe{(*ivhd).length as usize}
					}
					0x40=>
					{
						let ivhd:*const IvhdLarge=ivd.cast();
						unsafe{(*ivhd).length as usize}
					}
					_=>panic!("This IVD Type is unrecognized!")
				};
				ivd=unsafe{ivd.add(len)};
			}
			true
		});
		// If we have BARs, we may initialize paging.
		if mgr.iommu_bars.is_empty()
		{
			Err(NOIR_IVRS_NOT_SUPPROTED)
		}
		else
		{
			// PML4E
			match alloc_contd_pages(PAGE_SIZE)
			{
				Some(md)=>mgr.pml4e=md,
				None=>panic!("Failed to allocate PML4E for AMD-Vi!")
			}
			debug!("PML4E is allocated to {:p} for AMD-Vi!",mgr.pml4e.virt);
			for i in 0..PAGE_TABLE_ENTRIES64
			{
				let pml4e_p=unsafe{mgr.pml4e.virt.cast::<SvmIommuPte>().add(i)};
				unsafe
				{
					*pml4e_p=SvmIommuPte::new_pte(page_256tb_mult(i as u64),true,true);
				}
			}
			// Device Table
			match alloc_2mb_page()
			{
				Some(md)=>mgr.device_table=md,
				None=>panic!("Failed to allocate Device-Table for AMD-Vi!")
			}
			debug!("Device Table is allocated to {:p} for AMD-Vi!",mgr.device_table.virt);
			let dev_tables:&mut [SvmIommuDeviceTableEntry]=unsafe{slice::from_raw_parts_mut(mgr.device_table.virt.cast(),0x10000)};
			for dte in dev_tables
			{
				*dte=SvmIommuDeviceTableEntry::default();
				dte.p1.set_valid(true);
				dte.p1.set_translation_valid(true);
				dte.p1.set_host_access(true);
				dte.p1.set_host_dirty(true);
				dte.p1.set_paging_mode(4);
				dte.p1.set_paging_base(page_4kb_count(mgr.pml4e.phys));
				dte.p1.set_io_read_permission(true);
				dte.p1.set_io_write_permission(true);
				dte.p2.set_domain_id(1);
				dte.p2.set_iotlb_enable(true);
				dte.p2.set_suppress_iopf_events(true);
			}
			for bar in &mut mgr.iommu_bars
			{
				// Command Buffer
				match alloc_contd_pages(PAGE_SIZE)
				{
					Some(md)=>bar.cmd_base=md,
					None=>panic!("Failed to allocate command buffer for AMD-Vi!")
				}
				// Event Logs
				match alloc_contd_pages(PAGE_SIZE)
				{
					Some(md)=>bar.log_base=md,
					None=>panic!("Failed to allocate event logs for AMD-Vi!")
				}
			}
			Ok(mgr)
		}
	}

	pub fn activate(&mut self)
	{
		let mut dev_table_reg=DeviceTableBaseRegister::default();
		dev_table_reg.set_size(0x1ff);
		dev_table_reg.set_base(page_4kb_count(self.device_table.phys));
		let mut iommu_cr=IommuControlRegister::default();
		iommu_cr.set_iommu_enable(true);
		iommu_cr.set_cmd_buff_enable(true);
		iommu_cr.set_event_log_enable(true);
		for x in &mut self.iommu_bars
		{
			let mut cmd_buff_reg=CommandBufferBaseRegister::default();
			cmd_buff_reg.set_base(page_4kb_count(x.cmd_base.phys));
			cmd_buff_reg.set_length(0x10);	// Our Command-Buffer has 4KiB.
			let mut log_buff_reg=EventLogBaseRegister::default();
			log_buff_reg.set_base(page_4kb_count(x.log_base.phys));
			log_buff_reg.set_length(0x10);	// Out EventLog-Buffer has 4KiB.
			info!("Activating IOMMU for BAR 0x{:X}...",x.bar.phys);
			unsafe
			{
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_DEVICE_TABLE_BASE).cast(),dev_table_reg.into_bits());
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_BASE).cast(),cmd_buff_reg.into_bits());
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_HEAD).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_TAIL).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_EVENT_LOG_BUFFER_BASE).cast(),log_buff_reg.into_bits());
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_EVENT_LOG_BUFFER_HEAD).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_EVENT_LOG_BUFFER_TAIL).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_IOMMU_CONTROL_REGISTER).cast(),iommu_cr.into_bits());
			}
			info!("Awaiting activation (Log-Base: 0x{:X})...",log_buff_reg.into_bits());
			let mut signal:u64=0;
			let signal_phys=unsafe{noir_get_physical_address((&raw mut signal).cast())};
			let cmd1=SvmIommuCommand::InvalidateIommuAll;
			let cmd2=SvmIommuCommand::CompletionWait
			{
				store_address:signal_phys,
				store_data:1,
				completion_store:true,
				incompletion_interrupt:false,
				flush_queue:true
			};
			x.issue_cmd(cmd1);
			x.issue_cmd(cmd2);
			while signal==0
			{
				spin_loop();
			}
			info!("Successfully activated IOMMU for BAR 0x{:X}...",x.bar.phys);
		}
	}

	pub fn protect_ci(&mut self)
	{
		let ci=CI_MANAGER.read();
		for p in ci.into_iter()
		{
			self.update_pml1e(*p,*p,false,false);
		}
	}

	fn add_bar(&mut self,bar:u64)
	{
		match self.iommu_bars.binary_search_by(|x| x.bar.phys.cmp(&bar))
		{
			Ok(i)=>warn!("IOMMU BAR 0x{bar:X} is already inserted at index {i}!"),
			Err(i)=>self.iommu_bars.insert(i,SvmIommuBar::new(bar))
		}
	}
}

impl Drop for SvmIommuManager
{
	fn drop(&mut self)
	{
		if !self.pml4e.virt.is_null()
		{
			free_contd_pages(self.pml4e.virt,PAGE_SIZE);
		}
		// Note that PML3E to PML1E are automatically freed by Drop trait.
	}
}

pub struct SvmIommuBar
{
	pub bar:MemoryDescriptor,
	pub size:usize,
	pub iotlb_sup:bool,
	pub cmd_base:MemoryDescriptor,
	pub log_base:MemoryDescriptor
}

impl SvmIommuBar
{
	fn new(phys:u64)->Self
	{
		Self
		{
			bar:MemoryDescriptor
			{
				virt:unsafe{noir_map_uncached_memory(phys,0x4000)},
				phys
			},
			size:0x4000,
			iotlb_sup:false,
			cmd_base:MemoryDescriptor::null(),
			log_base:MemoryDescriptor::null()
		}
	}

	pub fn issue_cmd(&mut self,command:SvmIommuCommand)
	{
		let raw=command.into_raw();
		let tail_pos:u64=unsafe{mmio_read(self.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_TAIL).cast())};
		unsafe
		{
			*self.cmd_base.virt.byte_add(tail_pos as usize).cast()=raw;
			mmio_write(self.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_TAIL).cast(),tail_pos+16);
		}
	}
}

impl Drop for SvmIommuBar
{
	fn drop(&mut self)
	{
		unsafe
		{
			noir_unmap_physical_memory(self.bar.virt,0x4000);
		}
	}
}

#[bitfield(u64)] pub struct SvmIommuDteP1
{
	pub valid:bool,
	pub translation_valid:bool,
	#[bits(5)] rsvd0:u64,
	pub host_access:bool,
	pub host_dirty:bool,
	#[bits(3)] pub paging_mode:u64,
	#[bits(40)] pub paging_base:u64,
	pub periph_page_req:bool,
	pub guest_ppr_resp_pasid:bool,
	pub guest_io_valid:bool,
	pub guest_trans_valid:bool,
	#[bits(2)] pub guest_levels_valid:u64,
	#[bits(3)] pub gcr3_trp_lo:u64,
	pub io_read_permission:bool,
	pub io_write_permission:bool,
	pub rsvd2:bool
}

#[bitfield(u64)] pub struct SvmIommuDteP2
{
	pub domain_id:u16,
	pub gcr3_trp_mid:u16,
	pub iotlb_enable:bool,
	pub suppress_iopf_events:bool,
	pub suppress_all_iopf_events:bool,
	#[bits(2)] pub port_io_control:u64,
	pub iotlb_cache_hint:bool,
	pub snoop_disable:bool,
	pub allow_exclusion:bool,
	#[bits(2)] pub sysmgmt_message:u64,
	pub secure_ats:bool,
	#[bits(21)] pub gcr3_trp_hi:u64
}

#[bitfield(u64)] pub struct SvmIommuDteP3
{
	pub interrupt_map_valid:bool,
	#[bits(4)] pub int_table_length:u64,
	pub ignore_unmapped_int:bool,
	#[bits(46)] pub int_table_base:u64,
	#[bits(2)] rsvd:u64,
	#[bits(2)] pub guest_paging_mode:u64,
	pub init_signal_passthru:bool,
	pub extint_passthru:bool,
	pub nmi_passthru:bool,
	pub host_pt_mode:bool,
	#[bits(2)] pub int_ctrl:u64,
	pub lint0_passthru:bool,
	pub lint1_passthru:bool
}

#[bitfield(u64)] pub struct SvmIommuDteP4
{
	#[bits(15)] rsvd0:u64,
	pub viommu_enable:bool,
	pub guest_device_id:u16,
	pub guest_id:u16,
	#[bits(6)] rsvd1:u64,
	pub attrib_override_valid:bool,
	pub mode0_fc:bool,
	pub snoop_attrib:u8
}

#[repr(C)] pub struct SvmIommuDeviceTableEntry
{
	pub p1:SvmIommuDteP1,
	pub p2:SvmIommuDteP2,
	pub p3:SvmIommuDteP3,
	pub p4:SvmIommuDteP4,
}

impl Default for SvmIommuDeviceTableEntry
{
	fn default() -> Self
	{
		Self
		{
			p1:SvmIommuDteP1(0),
			p2:SvmIommuDteP2(0),
			p3:SvmIommuDteP3(0),
			p4:SvmIommuDteP4(0)
		}
	}
}

impl SvmIommuDeviceTableEntry
{
	pub fn get_gcr3_table_trp(&self)->u64
	{
		let lo=self.p1.gcr3_trp_lo();
		let mid=self.p2.gcr3_trp_mid() as u64;
		let hi=self.p2.gcr3_trp_hi();
		lo|(mid<<3)|(hi<<19)
	}

	pub fn set_gcr3_table_trp(&mut self,value:u64)
	{
		let lo=value&0x7;
		let mid=(value>>3) as u16;
		let hi=(value>>19)&0xFFFFF;
		self.p1.set_gcr3_trp_lo(lo);
		self.p2.set_gcr3_trp_mid(mid);
		self.p2.set_gcr3_trp_hi(hi);
	}
}