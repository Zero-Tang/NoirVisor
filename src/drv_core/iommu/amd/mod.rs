/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the AMD-Vi driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, hint::spin_loop, sync::atomic::{AtomicU64, AtomicUsize, Ordering}};
use alloc::{sync::Arc, vec::Vec};

use log::*;
use paste::paste;

use paging::*;
use crate::{drv_core::acpi::tables::IoVirtualizationReportingStructure, *};
use super::IommuOps;
use xpf_core::{ioflt::IoRegionOps, nvbdk::*};
use cmdbuf::*;
use registers::*;
use devtbl::*;
use acpi::*;

#[allow(dead_code)] mod acpi;
#[allow(dead_code)] mod devtbl;
#[allow(dead_code)] mod paging;
#[allow(dead_code)] mod registers;
#[allow(dead_code)] mod cmdbuf;

pub struct AmdIommuManager
{
	pub ivrs_units:Vec<Arc<AmdIommuBar>>,
	pub device_table:MemoryDescriptor<PAGE_TABLE_ENTRIES64,AmdIommuDeviceTableEntry>,
	pub pml4e:MemoryDescriptor<1,AmdIommuPde>,
	pub pml3e:Vec<AmdIommuPmlDescriptor<2>>,
	pub pml2e:Vec<AmdIommuPmlDescriptor<1>>,
	pub pml1e:Vec<AmdIommuPmlDescriptor<0>>
}

macro_rules! derive_pml_get_methods
{
	($name:tt,$size:tt)=>
	{
		paste!
		{
			fn [<get_ $name:lower _mut>](&mut self,gpa:u64)->Option<&mut AmdIommuPxe>
			{
				match self.$name.binary_search_by(|d| d.compare_gpa(gpa))
				{
					Ok(i)=>
					{
						let d=&mut self.$name[i];
						let j=page_entry_index([<page_ $size _count>](gpa as usize));
						Some(unsafe{&mut *d.pxe.virt.add(j)})
					}
					Err(_)=>None
				}
			}
		}
	};
}

impl AmdIommuManager
{
	fn push_bar(&mut self,bar:u64,cap_offset:u16)
	{
		#[allow(clippy::arc_with_non_send_sync)]
		match self.ivrs_units.binary_search_by(|iommu| iommu.bar.phys.cmp(&bar))
		{
			Ok(i)=>warn!("Base 0x{bar:X} already exist! Index={i}"),
			Err(i)=>self.ivrs_units.insert(i,Arc::new(AmdIommuBar
			{
				bar:MappedDescriptor::map(bar,4<<PAGE_SHIFT),
				cap_offset:cap_offset as usize,
				size:4<<PAGE_SHIFT,
				iotlb_sup:true,
				cmd_base:MemoryDescriptor::alloc().unwrap(),
				cmd_index:AtomicUsize::new(0),
				log_base:MemoryDescriptor::alloc().unwrap()
			}))
		}
	}

	fn split_pml4e(&mut self,gpa:u64)
	{
		if let Err(i)=self.pml3e.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md)=>
				{
					// Initialize descriptor.
					let d:AmdIommuPmlDescriptor<2>=AmdIommuPmlDescriptor
					{
						gpa_start:page_512gb_base(gpa),
						pxe:md
					};
					trace!("IOMMU PML3E: 0x{:X}",d.pxe.phys);
					// Initialize PML3E Page.
					let pml3s:&mut [AmdIommuPte;PAGE_TABLE_ENTRIES64]=unsafe{&mut *d.pxe.virt.cast()};
					// Set all to RWX since PML3E supports huge-page allocation.
					for (i,pml3e) in pml3s.iter_mut().enumerate()
					{
						*pml3e=AmdIommuPte::new_pte(d.gpa_start+(page_1gb_mult(i) as u64),true,true);
					}
					// Update PML4 Entry.
					self.get_pml4e_mut(gpa).unwrap().pde=AmdIommuPde::new_pde(d.pxe.phys,3,true,true);
					// Insert to IOMMU Paging Manager.
					self.pml3e.insert(i,d);
				}
				None=>panic!("Failed to allocate PML3E to split PML4E!")
			}
		}
	}

	fn split_pml3e(&mut self,gpa:u64)
	{
		if let Err(i)=self.pml2e.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md)=>
				{
					// Create PML3E first.
					self.split_pml4e(gpa);
					// Initialize descriptor.
					let d:AmdIommuPmlDescriptor<1>=AmdIommuPmlDescriptor
					{
						gpa_start:page_1gb_base(gpa),
						pxe:md
					};
					trace!("IOMMU PML2E: 0x{:X}",d.pxe.phys);
					// Initialize PML2E Page.
					let pml2s:&mut [AmdIommuPte;PAGE_TABLE_ENTRIES64]=unsafe{&mut *d.pxe.virt.cast()};
					// Set all to RWX since PML2E supports large-page allocation.
					for (i,pml2e) in pml2s.iter_mut().enumerate()
					{
						*pml2e=AmdIommuPte::new_pte(d.gpa_start+(page_2mb_mult(i) as u64),true,true);
					}
					// Update PML3 Entry.
					self.get_pml3e_mut(gpa).unwrap().pde=AmdIommuPde::new_pde(d.pxe.phys,2,true,true);
					// Insert to IOMMU Paging Manager.
					self.pml2e.insert(i,d);
				}
				None=>panic!("Failed to allocate PML2E to split PML3E!")
			}
		}
	}

	fn split_pml2e(&mut self,gpa:u64)
	{
		if let Err(i)=self.pml1e.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md)=>
				{
					// Create PML2E first.
					self.split_pml3e(gpa);
					// Initialize descriptor.
					let d:AmdIommuPmlDescriptor<0>=AmdIommuPmlDescriptor
					{
						gpa_start:page_2mb_base(gpa),
						pxe:md
					};
					trace!("IOMMU PML1E: 0x{:X}",d.pxe.phys);
					// Initialize PML1E Page.
					let pml1s:&mut [AmdIommuPte;PAGE_TABLE_ENTRIES64]=unsafe{&mut *d.pxe.virt.cast()};
					// Set all to RWX since PML1E is the last level of paging.
					for (i,pml1e) in pml1s.iter_mut().enumerate()
					{
						*pml1e=AmdIommuPte::new_pte(d.gpa_start+(page_4kb_mult(i) as u64),true,true);
					}
					// Update PML2 Entry.
					self.get_pml2e_mut(gpa).unwrap().pde=AmdIommuPde::new_pde(d.pxe.phys,1,true,true);
					// Insert to IOMMU Paging Manager.
					self.pml1e.insert(i,d);
				}
				None=>panic!("Failed to allocate PML1E to split PML2E!")
			}
		}
	}

	fn get_pml4e_mut(&mut self,gpa:u64)->Option<&mut AmdIommuPxe>
	{
		let i=page_512gb_count(gpa as usize);
		if i<PAGE_TABLE_ENTRIES64
		{
			Some(unsafe{&mut *self.pml4e.virt.add(i).cast()})
		}
		else
		{
			None
		}
	}

	derive_pml_get_methods!(pml3e,1gb);
	derive_pml_get_methods!(pml2e,2mb);
	derive_pml_get_methods!(pml1e,4kb);
}

impl IommuOps for AmdIommuManager
{
	unsafe fn init(&mut self)
	{
		unsafe
		{
			use core::ptr::write;
			write(&raw mut self.ivrs_units,Vec::new());
			write(&raw mut self.device_table,MemoryDescriptor::alloc_2mb_page().unwrap());
			write(&raw mut self.pml4e,MemoryDescriptor::alloc().unwrap());
			write(&raw mut self.pml3e,Vec::new());
			write(&raw mut self.pml2e,Vec::new());
			write(&raw mut self.pml1e,Vec::new());
		}
	}

	fn subvert(&mut self)
	{
		// Initialize Device Table Entries.
		let devtbl_vec:&mut [AmdIommuDeviceTableEntry;65536]=unsafe{&mut *self.device_table.virt.cast()};
		for (i,dt) in devtbl_vec.iter_mut().enumerate()
		{
			dt.p1=AmdIommuDteP1::from_bits(self.pml4e.phys+page_4kb_count(i) as u64).with_v(true).with_tv(true).with_ha(true).with_hd(true).with_mode(4).with_ir(true).with_iw(true);
			dt.p2=AmdIommuDteP2::new().with_did(1).with_i(true).with_se(true).with_sa(true);
			dt.p3=AmdIommuDteP3::new();
			dt.p4=AmdIommuDteP4::new();
		}
		// Setup all IOMMUs.
		for ivrs in &mut self.ivrs_units
		{
			let virt=ivrs.bar.virt.load(Ordering::Relaxed);
			let devtbl_base=DeviceTableBaseRegister::from_bits(self.device_table.phys).with_size(0x1FF);
			let cmd_buff_base=CommandBufferBaseRegister::from_bits(ivrs.cmd_base.phys).with_com_len(0b1000);
			let cmd_head=CommandBufferHeadPointerRegister::new();
			let cmd_tail=CommandBufferTailPointerRegister::new();
			let log_buff_base=EventLogBaseRegister::from_bits(ivrs.log_base.phys).with_event_len(0b1000);
			let log_head=EventBufferHeadPointerRegister::new();
			let log_tail=EventBufferTailPointerRegister::new();
			let iommu_ctrl=IommuControlRegister::new().with_iommu_en(true).with_gt_en(true).with_cmd_buff_en(true).with_event_log_en(true);
			unsafe
			{
				// Set Device Table Base.
				devtbl_base.write(virt);
				// Set Command Buffer.
				cmd_buff_base.write(virt);
				cmd_head.write(virt);
				cmd_tail.write(virt);
				// Set Event Log Buffer.
				log_buff_base.write(virt);
				log_head.write(virt);
				log_tail.write(virt);
				// Enable IOMMU.
				iommu_ctrl.write(virt);
			}
			// Invalidate all device IDs.
			for i in 0..=u16::MAX
			{
				let inv_dte_cmd=InvalidateDevtabEntryCommand::new().with_device_id(i);
				let j=ivrs.issue_cmd(inv_dte_cmd);
				if j==0xFE
				{
					ivrs.wait();
				}
			}
			ivrs.wait();
			info!("AMD-Vi IOMMU completed subversion! Log-Base: 0x{:X}",ivrs.log_base.phys);
		}
	}

	fn restore(&mut self)
	{
		
	}

	fn set_pdpte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,_x:bool)
	{
		self.split_pml4e(gpa);
		let pte=unsafe{&mut self.get_pml3e_mut(gpa).unwrap().pte};
		*pte=AmdIommuPte::new_pte(hpa,r,w);
	}

	fn set_pde(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,_x:bool)
	{
		self.split_pml3e(gpa);
		let pte=unsafe{&mut self.get_pml2e_mut(gpa).unwrap().pte};
		*pte=AmdIommuPte::new_pte(hpa,r,w);
	}

	fn set_pte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,_x:bool)
	{
		self.split_pml2e(gpa);
		let pte=unsafe{&mut self.get_pml1e_mut(gpa).unwrap().pte};
		*pte=AmdIommuPte::new_pte(hpa,r,w);
	}

	fn get_bar_pages(&self)->Vec<Box<dyn IoRegionOps>>
	{
		let mut v:Vec<Box<dyn IoRegionOps>>=Vec::new();
		for x in &self.ivrs_units
		{
			v.push(Box::new(AmdIommuBarRegion(Arc::clone(x))));
		}
		v
	}
}

pub struct AmdIommuBar
{
	pub bar:MappedDescriptor<c_void>,
	pub cap_offset:usize,
	pub size:usize,
	pub iotlb_sup:bool,
	pub cmd_index:AtomicUsize,
	pub cmd_base:MemoryDescriptor<1,u128>,
	pub log_base:MemoryDescriptor<1,u128>
}

impl AmdIommuBar
{
	fn wait(&self)
	{
		let virt=self.bar.virt.load(Ordering::Relaxed);
		let i=self.cmd_index.load(Ordering::SeqCst);
		let tail=CommandBufferTailPointerRegister::new().with_cmd_tail_ptr(i);
		unsafe
		{
			tail.write(virt);
		}
		let signal=AtomicU64::new(0);
		let compl_wait_cmd=CompletionWaitCommand::new().with_s(true).with_f(true).with_store_data(1)
		.with_store_address(unsafe{noir_get_physical_address(signal.as_ptr().cast())}>>3);
		self.issue_cmd(compl_wait_cmd);
		unsafe
		{
			tail.with_cmd_tail_ptr(tail.cmd_tail_ptr()+1).write(virt);
		}
		while signal.load(Ordering::SeqCst)==0
		{
			spin_loop();
		}
	}

	fn issue_cmd(&self,cmd:impl CommandBufferItem)->usize
	{
		let i=self.cmd_index.fetch_add(1,Ordering::SeqCst);
		self.cmd_index.fetch_and(0xFF,Ordering::SeqCst);
		unsafe
		{
			*self.cmd_base.virt.add(i)=cmd.into();
		}
		i
	}
}

pub struct AmdIommuBarRegion(Arc<AmdIommuBar>);

impl IoRegionOps for AmdIommuBarRegion
{
	fn name(&self)->&str
	{
		"AMD-Vi IOMMU BAR"
	}

	fn base_size(&self)->(u64,usize)
	{
		(self.0.bar.phys,4<<PAGE_SHIFT)
	}

	fn forward_input(&self)->bool
	{
		false
	}

	fn forward_output(&self)->bool
	{
		false
	}

	fn input(&mut self,_address:u64,value:&mut [u8],_context:*mut c_void)->Status
	{
		// Ignore the input. Emulate none device.
		value.fill(u8::MAX);
		Status::SUCCESS
	}

	fn output(&mut self,_address:u64,_value:&[u8],_context:*mut c_void)->Status
	{
		// Ignore the output.
		Status::SUCCESS
	}
}

pub(super) fn create_iommu(acpi_tables:&[*const IoVirtualizationReportingStructure])->Option<Box<dyn IommuOps>>
{
	let mut x:Box<AmdIommuManager>=unsafe
	{
		let mut x:Box<AmdIommuManager>=Box::new_uninit().assume_init();
		x.init();
		x
	};
	for &t in acpi_tables
	{
		let table=unsafe{&*t};
		let limit=table.header.length.get() as usize-size_of::<IoVirtualizationReportingStructure>();
		let mut cursor=0;
		unsafe
		{
			// Destroy the IVRS table so that Guest OS will not try to run AMD-Vi IOMMU.
			let p=(&raw const (*t).header.signature) as *mut u32;
			p.write_unaligned(u32::from_ne_bytes(*b"????"));
		}
		while cursor<limit
		{
			unsafe
			{
				let ivdb:&Ivdb=&*table.ivdb.as_ptr().add(cursor).cast();
				enum IvdbOption<'a>
				{
					Ivhd10(&'a Ivhd10),
					Ivhd11(&'a Ivhd11),
					Ivhd40(&'a Ivhd40),
					Ivmd(&'a Ivmd),
					Unknown
				}
				let opt=match ivdb.head.r#type
				{
					0x10=>IvdbOption::Ivhd10(&ivdb.ivhd10),
					0x11=>IvdbOption::Ivhd11(&ivdb.ivhd11),
					0x40=>IvdbOption::Ivhd40(&ivdb.ivhd40),
					0x20..0x22=>IvdbOption::Ivmd(&ivdb.ivmd),
					_=>IvdbOption::Unknown
				};
				match opt
				{
					IvdbOption::Ivhd10(ivhd)=>
					{
						info!("IVHD-10H Features: {:X?}",ivhd.iommu_feature_reporting.get());
						x.push_bar(ivhd.iommu_base_pa.get(),ivhd.capability_offset.get());
					}
					IvdbOption::Ivhd11(ivhd)=>
					{
						info!("IVHD-11H Attributes: {:X?}",ivhd.iommu_attributes.get());
						x.push_bar(ivhd.iommu_base_pa.get(),ivhd.capability_offset.get());
					}
					IvdbOption::Ivhd40(ivhd)=>
					{
						info!("IVHD-40H Attributes: {:X?}",ivhd.iommu_attributes.get());
						x.push_bar(ivhd.iommu_base_pa.get(),ivhd.capability_offset.get());
					}
					IvdbOption::Ivmd(ivmd)=>
					{
						info!("IVMD Start: 0x{:X}, Length: 0x{:X}",ivmd.start_address.get(),ivmd.block_length.get());
					}
					IvdbOption::Unknown=>
					{
						error!("Unknown IVDB Type: 0x{:X}!",ivdb.head.r#type);
					}
				}
				cursor+=ivdb.head.length.get() as usize;
			}
		}
	}
	let mut req_sup=true;
	for iommu in &x.ivrs_units
	{
		let ext_cap=unsafe{IommuExtendedFeatureRegister::read(iommu.bar.virt.load(Ordering::Relaxed))};
		info!("Extended Capability of IOMMU BAR 0x{:X}, {ext_cap:X?}",iommu.bar.phys);
		// Guest Translation must be supported in order to protect from DMA attacks.
		req_sup&=ext_cap.gt_sup();
		// Hardware Error Registers must be supported in order to keep track of errors.
		req_sup&=ext_cap.he_sup();
	}
	if req_sup
	{
		Some(x)
	}
	else
	{
		error!("The AMD-Vi implementation of this machine does not meet minimum requirement!");
		None
	}
}