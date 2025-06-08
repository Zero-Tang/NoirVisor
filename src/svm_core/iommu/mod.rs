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

use core::{ffi::c_void, slice};
use alloc::vec::Vec;
use mmio::DeviceTableBaseRegister;
use paste::paste;

use acpi::{Ivhd, IvhdLarge};
use paging::{SvmIommuPmlManager, SvmIommuPte};
use crate::{svm_core::iommu::mmio::*, xpf_core::{asm::io::{mmio_read, mmio_write}, ci::enum_ci_phys_page}, *};
use drv_core::acpi::{search_acpi_table, tables::{AcpiSystemDescriptorSignature, IoVirtualizationReportingStructure}};
use xpf_core::{nvbdk::*, nvstatus::*, ioflt::IoRegion, dlalloc::{alloc_2mb_page, alloc_contd_pages, free_contd_pages}};

mod acpi;
mod paging;
mod mmio;

pub(super) fn svm_iommu_output_handler(_region:&IoRegion<u64>,address:u64,size:u64,value:*const c_void,_context:*mut c_void)
{
	println!("Intercepted writes to IOMMU! Address=0x{address:X}, Size: {size}");
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
			let end:*const u8=unsafe{ivrs.byte_add((*ivrs).header.length as usize).cast()};
			while ivd<end
			{
				println!("Found I/O Virtualization Definition Type 0x{:X} at {ivd:p}!",unsafe{*ivd});
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
			println!("PML4E is allocated to {:p} for AMD-Vi!",mgr.pml4e.virt);
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
			println!("Device Table is allocated to {:p} for AMD-Vi!",mgr.device_table.virt);
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
			cmd_buff_reg.set_base(x.cmd_base.phys);
			cmd_buff_reg.set_length(0x10);	// Our Command-Buffer has 4KiB.
			let mut log_buff_reg=EventLogBaseRegister::default();
			log_buff_reg.set_base(x.log_base.phys);
			log_buff_reg.set_length(0x10);	// Out EventLog-Buffer has 4KiB.
			println!("Activating IOMMU for BAR 0x{:X}...",x.bar.phys);
			unsafe
			{
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_DEVICE_TABLE_BASE).cast(),dev_table_reg.0);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_BASE).cast(),cmd_buff_reg.0);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_HEAD).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_COMMAND_BUFFER_TAIL).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_EVENT_LOG_BUFFER_BASE).cast(),log_buff_reg.0);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_EVENT_LOG_BUFFER_HEAD).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_EVENT_LOG_BUFFER_TAIL).cast(),0u64);
				mmio_write(x.bar.virt.byte_add(MMIO_BASE_IOMMU_CONTROL_REGISTER).cast(),iommu_cr.0);
			}
			println!("Awaiting activation...");
			println!("Successfully activated IOMMU for BAR 0x{:X}...",x.bar.phys);

		}
	}

	pub fn protect_ci(&mut self)
	{
		let ci_pages=unsafe{&*enum_ci_phys_page()};
		for p in ci_pages
		{
			self.update_pml1e(*p,*p,false,false);
		}
	}

	fn add_bar(&mut self,bar:u64)
	{
		match self.iommu_bars.binary_search_by(|x| x.bar.phys.cmp(&bar))
		{
			Ok(i)=>println!("IOMMU BAR 0x{bar:X} is already inserted at index {i}!"),
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

#[repr(C)] pub struct SvmIommuDteP1(pub u64);
#[repr(C)] pub struct SvmIommuDteP2(pub u64);
#[repr(C)] pub struct SvmIommuDteP3(pub u64);
#[repr(C)] pub struct SvmIommuDteP4(pub u64);

impl SvmIommuDteP1
{
	build_bit_mut_method!(valid,0);
	build_bit_mut_method!(translation_valid,1);
	build_bit_mut_method!(host_access,7);
	build_bit_mut_method!(host_dirty,8);
	build_int_mut_method!(paging_mode,9,3,u64);
	build_int_mut_method!(paging_base,12,40,u64);
	build_bit_mut_method!(periph_page_req,52);
	build_bit_mut_method!(guest_ppr_resp_pasid,53);
	build_bit_mut_method!(guest_io_valid,54);
	build_bit_mut_method!(guest_trans_valid,55);
	build_int_mut_method!(guest_levels_valid,56,2,u64);
	build_int_mut_method!(gcr3_trp_lo,58,3,u64);
	build_bit_mut_method!(io_read_permission,61);
	build_bit_mut_method!(io_write_permission,62);
}

impl SvmIommuDteP2
{
	build_int_mut_method!(domain_id,0,16,u64);
	build_int_mut_method!(gcr3_trp_mid,16,16,u64);
	build_bit_mut_method!(iotlb_enable,32);
	build_bit_mut_method!(suppress_iopf_events,33);
	build_bit_mut_method!(suppress_all_iopf_events,34);
	build_int_mut_method!(port_io_control,35,2,u64);
	build_bit_mut_method!(iotlb_cache_hint,37);
	build_bit_mut_method!(snoop_disable,38);
	build_bit_mut_method!(allow_exclusion,39);
	build_int_mut_method!(sysmgt_message,40,2,u64);
	build_bit_mut_method!(secure_ats,42);
	build_int_mut_method!(gcr3_trp_hi,43,21,u64);
}

impl SvmIommuDteP3
{
	build_bit_mut_method!(interrupt_map_valid,0);
	build_int_mut_method!(int_table_length,1,4,u64);
	build_bit_mut_method!(ignore_unmapped_int,5);
	build_int_mut_method!(int_table_base,6,46,u64);
	build_int_mut_method!(guest_paging_mode,54,2,u64);
	build_bit_mut_method!(init_signal_passthrough,56);
	build_bit_mut_method!(extint_passthrough,57);
	build_bit_mut_method!(nmi_passthrough,58);
	build_bit_mut_method!(host_pt_mode,59);
	build_int_mut_method!(int_ctrl,60,2,u64);
	build_bit_mut_method!(lint0_passthrough,62);
	build_bit_mut_method!(lint1_passthrough,63);
}

impl SvmIommuDteP4
{
	build_bit_mut_method!(viommu_enable,15);
	build_int_mut_method!(guest_device_id,16,16,u64);
	build_int_mut_method!(guest_id,32,16,u64);
	build_bit_mut_method!(attrib_override_valid,54);
	build_bit_mut_method!(mode0_fc,55);
	build_int_mut_method!(snoop_attribute,56,8,u64);
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
		let lo=self.p1.get_gcr3_trp_lo();
		let mid=self.p2.get_gcr3_trp_mid();
		let hi=self.p2.get_gcr3_trp_hi();
		lo|(mid<<3)|(hi<<19)
	}

	pub fn set_gcr3_table_trp(&mut self,value:u64)
	{
		let lo=value&0x7;
		let mid=(value>>3)&0xFFFF;
		let hi=(value>>19)&0xFFFFF;
		self.p1.set_gcr3_trp_lo(lo);
		self.p2.set_gcr3_trp_mid(mid);
		self.p2.set_gcr3_trp_hi(hi);
	}
}