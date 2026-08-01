/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file is the driver for Intel VT-d IOMMU hardware.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, hint::spin_loop, slice, sync::atomic::Ordering};
use alloc::{boxed::Box, sync::Arc, vec::Vec};
use log::{error, info, trace};
use nvcvm::status::Status;

use crate::{drv_core::{acpi::tables::DmaRemappingReportingStructure, iommu::intel::registers::*}, xpf_core::{ioflt::IoRegionOps, nvbdk::*}};
use super::IommuOps;

use paging::*;
use acpi::*;

use paste::paste;

#[allow(dead_code)] pub mod acpi;
#[allow(dead_code)] mod registers;
#[allow(dead_code)] mod paging;

pub struct IntelDmarUnit
{
	pub bar:MappedDescriptor<c_void>,
	pub fault_record_offset:usize,
	pub fault_record_limit:usize,
	pub iotlb_reg_offset:usize
}

pub struct IntelDmarUnitRegion(Arc<IntelDmarUnit>);

impl IoRegionOps for IntelDmarUnitRegion
{
	fn name(&self)->&str
	{
		"Intel VT-d IOMMU BAR"
	}

	fn base_size(&self)->(u64,usize)
	{
		(self.0.bar.phys,PAGE_SIZE)
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

pub trait IntelIommuPageTableOps
{
	/// Size per page entry.
	const PAGE_SIZE:usize;
	type PageSizeVariant;
}

pub struct IntelIommuPageEntryDescriptor<T:IntelIommuPageTableOps>
{
	pub gpa_start:u64,
	pub descriptor:MemoryDescriptor<1,T>
}

impl<T:IntelIommuPageTableOps> IntelIommuPageEntryDescriptor<T>
{
	fn compare_gpa(&self,gpa:u64)->core::cmp::Ordering
	{
		use core::cmp::Ordering;
		if gpa<self.gpa_start
		{
			Ordering::Greater
		}
		else if gpa>=self.gpa_start+(T::PAGE_SIZE<<PAGE_SHIFT_DIFF64) as u64
		{
			Ordering::Less
		}
		else
		{
			Ordering::Equal
		}
	}
}

pub struct IntelIommuManager
{
	root:MemoryDescriptor<1,RootEntry>,
	ctxt:MemoryDescriptor<256,ContextEntry>,
	pml5:Vec<IntelIommuPageEntryDescriptor<SsPml5e>>,
	pml4:Vec<IntelIommuPageEntryDescriptor<SsPml4e>>,
	pdpt:Vec<IntelIommuPageEntryDescriptor<SsPdpe>>,
	pd:Vec<IntelIommuPageEntryDescriptor<SsPde>>,
	pt:Vec<IntelIommuPageEntryDescriptor<SsPte>>,
	dmar_units:Vec<Arc<IntelDmarUnit>>,
	common_sagaw:u8
}

macro_rules! derive_pml_get_methods
{
	($name:tt,$this_type:ty,$size:tt)=>
	{
		paste!
		{
			fn [<get_ $name:lower e_mut>](&mut self,gpa:u64)->Option<&mut $this_type>
			{
				match self.$name.binary_search_by(|d| d.compare_gpa(gpa))
				{
					Ok(i)=>
					{
						let d=&mut self.$name[i];
						let j=page_entry_index([<page_ $size _count>](gpa as usize));
						Some(unsafe{&mut *d.descriptor.virt.add(j)})
					}
					Err(_)=>None
				}
			}
		}
	};
	($name:tt,$alt_name:tt,$this_type:ty,$size:tt)=>
	{
		paste!
		{
			fn [<get_ $alt_name:lower e_mut>](&mut self,gpa:u64)->Option<&mut $this_type>
			{
				match self.$name.binary_search_by(|d| d.compare_gpa(gpa))
				{
					Ok(i)=>
					{
						let d=&mut self.$name[i];
						let j=page_entry_index([<page_ $size _count>](gpa as usize));
						Some(unsafe{&mut *d.descriptor.virt.cast::<$this_type>().add(j)})
					}
					Err(_)=>None
				}
			}
		}
	};
}

impl IntelIommuManager
{
	fn new_pml5e(&mut self,gpa:u64)
	{
		if let Err(i)=self.pml4.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md)=>
				{
					// Initialize descriptor.
					let d:IntelIommuPageEntryDescriptor<SsPml4e>=IntelIommuPageEntryDescriptor
					{
						gpa_start:page_256tb_base(gpa),
						descriptor:md
					};
					trace!("IOMMU PML4E: 0x{:X}",d.descriptor.phys);
					// Initialize PML4E Page.
					let pml4s:&mut [SsPml4e]=unsafe{slice::from_raw_parts_mut(d.descriptor.virt,PAGE_TABLE_ENTRIES64)};
					// Set all to No-Access since PML4E has no page-size option.
					for pml4e in pml4s
					{
						*pml4e=SsPml4e::new();
					}
					// Update PML5 Entry.
					*self.get_pml5e_mut(gpa).unwrap()=SsPml5e::from_bits(d.descriptor.phys).with_r(true).with_w(true).with_x(true);
					// Insert to IOMMU Paging Manager.
					self.pml4.insert(i,d);
				}
				None=>panic!("Failed to split PML5E!")
			}
		}
	}

	fn new_pml4e(&mut self,gpa:u64)
	{
		if let Err(i)=self.pdpt.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md)=>
				{
					// Create PML5E first.
					self.new_pml5e(gpa);
					// Initialize descriptor.
					let d:IntelIommuPageEntryDescriptor<SsPdpe>=IntelIommuPageEntryDescriptor
					{
						gpa_start:page_512gb_base(gpa),
						descriptor:md
					};
					trace!("IOMMU PDPTE: 0x{:X}",d.descriptor.phys);
					// Initialize PDPTE Page.
					let pdpts:&mut [SsHugePdpe]=unsafe{slice::from_raw_parts_mut(d.descriptor.virt.cast(),PAGE_TABLE_ENTRIES64)};
					// Set all to RWX since PDPTE supports huge-page option.
					for (i,pdpe) in pdpts.iter_mut().enumerate()
					{
						*pdpe=SsHugePdpe::new().with_r(true).with_w(true).with_x(true).with_ps(true).with_page_base(page_512gb_count(d.gpa_start)+i as u64);
					}
					// Update PML4E Entry.
					*self.get_pml4e_mut(gpa).unwrap()=SsPml4e::from_bits(d.descriptor.phys).with_r(true).with_w(true).with_x(true);
					// Insert to IOMMU Paging Manager.
					self.pdpt.insert(i,d);
				}
				None=>panic!("Failed to split PML4E!")
			}
		}
	}

	fn split_pdpe(&mut self,gpa:u64)
	{
		if let Err(i)=self.pd.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md)=>
				{
					// Create PML4E first.
					self.new_pml4e(gpa);
					// Initialize descriptor.
					let d:IntelIommuPageEntryDescriptor<SsPde>=IntelIommuPageEntryDescriptor
					{
						gpa_start:page_1gb_base(gpa),
						descriptor:md
					};
					trace!("IOMMU PDE: 0x{:X}",d.descriptor.phys);
					// Initialize PDE Page.
					let pds:&mut [SsLargePde;PAGE_TABLE_ENTRIES64]=unsafe{&mut *d.descriptor.virt.cast()};
					// Set all to RWX since PDE supports large-page option.
					for (i,pde) in pds.iter_mut().enumerate()
					{
						*pde=SsLargePde::new().with_r(true).with_w(true).with_x(true).with_ps(true).with_page_base(page_1gb_count(d.gpa_start)+i as u64);
					}
					// Update PDPE Entry.
					*self.get_pdpte_mut(gpa).unwrap()=SsPdpe::from_bits(d.descriptor.phys).with_r(true).with_w(true).with_x(true);
					// Insert to IOMMU Paging Manager.
					self.pd.insert(i,d);
				}
				None=>panic!("Failed to split PDPTE!")
			}
		}
	}

	fn split_pde(&mut self,gpa:u64)
	{
		if let Err(i)=self.pt.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md)=>
				{
					// Create PDPTE first.
					self.split_pdpe(gpa);
					// Initialize descriptor.
					let d:IntelIommuPageEntryDescriptor<SsPte>=IntelIommuPageEntryDescriptor
					{
						gpa_start:page_2mb_base(gpa),
						descriptor:md
					};
					trace!("IOMMU PTE: 0x{:X}",d.descriptor.phys);
					// Initialize PTE Page.
					let pts:&mut [SsPte;PAGE_TABLE_ENTRIES64]=unsafe{&mut *d.descriptor.virt.cast()};
					// Set all to RWX since PTE is the last level.
					for (i,pte) in pts.iter_mut().enumerate()
					{
						*pte=SsPte::new().with_r(true).with_w(true).with_x(true).with_page_base(page_2mb_count(d.gpa_start)+i as u64);
					}
					// Update PDE Entry.
					*self.get_pde_mut(gpa).unwrap()=SsPde::from_bits(d.descriptor.phys).with_r(true).with_w(true).with_x(true);
					// Insert to IOMMU Paging Manager.
					self.pt.insert(i,d);
				}
				None=>panic!("Failed to split PDE!")
			}
		}
	}

	derive_pml_get_methods!(pml5,SsPml5e,256tb);
	derive_pml_get_methods!(pml4,SsPml4e,512gb);
	derive_pml_get_methods!(pdpt,huge_pdpt,SsHugePdpe,1gb);
	derive_pml_get_methods!(pdpt,SsPdpe,1gb);
	derive_pml_get_methods!(pd,large_pd,SsLargePde,2mb);
	derive_pml_get_methods!(pd,SsPde,2mb);
	derive_pml_get_methods!(pt,SsPte,4kb);
}

impl IommuOps for IntelIommuManager
{
	unsafe fn init(&mut self)
	{
		unsafe
		{
			use core::ptr::write;
			write(&raw mut self.root,MemoryDescriptor::alloc().unwrap());
			write(&raw mut self.ctxt,MemoryDescriptor::alloc().unwrap());
			write(&raw mut self.pml5,Vec::new());
			write(&raw mut self.pml4,Vec::new());
			write(&raw mut self.pdpt,Vec::new());
			write(&raw mut self.pd,Vec::new());
			write(&raw mut self.pt,Vec::new());
			write(&raw mut self.dmar_units,Vec::new());
		}
		self.common_sagaw=u8::MAX;
	}

	fn subvert(&mut self)
	{
		// Initialize Root Entries.
		let root_vec:&mut [RootEntry;256]=unsafe{&mut *self.root.virt.cast()};
		for (i,rt) in root_vec.iter_mut().enumerate()
		{
			*rt=RootEntry::new().with_p(true).with_ctp(page_4kb_count(self.ctxt.phys)+i as u64);
		}
		// Initialize Context Entries.
		let ctxt_vec:&mut [ContextEntry;65536]=unsafe{&mut *self.ctxt.virt.cast()};
		for ct in ctxt_vec
		{
			let (phys,aw)=if self.common_sagaw&8==8
			{
				(self.pml5[0].descriptor.phys,3)
			}
			else if self.common_sagaw&4==4
			{
				(self.pml4[0].descriptor.phys,2)
			}
			else
			{
				(self.pdpt[0].descriptor.phys,1)
			};
			*ct=ContextEntry::new().with_p(true).with_ssptptr(page_4kb_count(phys)).with_aw(aw).with_did(1);
		}
		// Setup all IOMMUs.
		for dmar in &mut self.dmar_units
		{
			let virt=dmar.bar.virt.load(Ordering::Relaxed);
			unsafe
			{
				// Disable IOMMU fault notification.
				let fault_msi_ctrl:*mut FaultEventControlRegister=virt.byte_add(FaultEventControlRegister::MMIO_OFFSET).cast();
				fault_msi_ctrl.write_volatile(FaultEventControlRegister::new());
				// Set Root-Table Pointer.
				let root_reg:*mut RootTableAddressRegister=virt.byte_add(RootTableAddressRegister::MMIO_OFFSET).cast();
				root_reg.write_volatile(RootTableAddressRegister::from_bits(self.root.phys));
				// Inform the IOMMU unit that Root-Table Pointer is updated.
				let gcmd:*mut GlobalCommandRegister=virt.byte_add(GlobalCommandRegister::MMIO_OFFSET).cast();
				gcmd.write_volatile(GlobalCommandRegister::new().with_srtp(true));
				// Wait until Root-Table Pointer is updated.
				let gst:*const GlobalStatusRegister=virt.byte_add(GlobalStatusRegister::MMIO_OFFSET).cast();
				let mut st=gst.read_volatile();
				while !st.rtps()
				{
					spin_loop();
					st=gst.read_volatile();
				}
				// Invalidate Context-Cache.
				let ctxt_cache:*mut ContextCommandRegister=virt.byte_add(ContextCommandRegister::MMIO_OFFSET).cast();
				ctxt_cache.write_volatile(ContextCommandRegister::new().with_icc(true).with_cirg(1));
				let mut st=ctxt_cache.read_volatile();
				while st.icc()
				{
					spin_loop();
					st=ctxt_cache.read_volatile();
				}
				// Invalidate IOTLB.
				let iotlb_ctrl:*mut IotlbInvalidateRegister=virt.byte_add(dmar.iotlb_reg_offset+8).cast();
				iotlb_ctrl.write_volatile(IotlbInvalidateRegister::new().with_ivt(true).with_iirg(1).with_iaig(1));
				let mut st=iotlb_ctrl.read_volatile();
				while st.ivt()
				{
					spin_loop();
					st=iotlb_ctrl.read_volatile();
				}
				// Enable DMA-Remapping.
				gcmd.write_volatile(GlobalCommandRegister::new().with_te(true));
				// Wait until DMA-Remapping is enabled.
				let mut st=gst.read_volatile();
				while !st.tes()
				{
					spin_loop();
					st=gst.read_volatile();
				}
			}
			info!("Completed configuration for IOMMU at 0x{:X}!",dmar.bar.phys);
		}
	}

	fn restore(&mut self)
	{
		
	}

	fn get_bar_pages(&self)->Vec<Box<dyn IoRegionOps>>
	{
		let mut v:Vec<Box<dyn IoRegionOps>>=Vec::new();
		for x in &self.dmar_units
		{
			v.push(Box::new(IntelDmarUnitRegion(Arc::clone(x))));
		}
		v
	}

	fn set_pdpte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool)
	{
		self.new_pml4e(gpa);
		*self.get_huge_pdpte_mut(gpa).unwrap()=SsHugePdpe::from_bits(hpa).with_r(r).with_w(w).with_x(x).with_ps(true);
	}

	fn set_pde(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool)
	{
		self.split_pdpe(gpa);
		*self.get_large_pde_mut(gpa).unwrap()=SsLargePde::from_bits(hpa).with_r(r).with_w(w).with_x(x).with_ps(true);
	}

	fn set_pte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool)
	{
		self.split_pde(gpa);
		*self.get_pte_mut(gpa).unwrap()=SsPte::from_bits(hpa).with_r(r).with_w(w).with_x(x);
	}
}

pub(super) fn create_iommu(acpi_tables:&[*const DmaRemappingReportingStructure])->Option<Box<dyn IommuOps>>
{
	let mut x:Box<IntelIommuManager>=unsafe
	{
		let mut x:Box<IntelIommuManager>=Box::new_uninit().assume_init();
		x.init();
		x
	};
	for &t in acpi_tables
	{
		unsafe
		{
			// Destroy the DMAR table so that Guest OS will not try to run Intel VT-d IOMMU.
			let p=(&raw const (*t).header.signature) as *mut u32;
			p.write_unaligned(u32::from_ne_bytes(*b"????"));
		}
		let table=unsafe{&*t};
		let limit=table.header.length.get() as usize-size_of::<DmaRemappingReportingStructure>();
		let mut cursor=0;
		while cursor<limit
		{
			let header:&DmarRemappingStructure=unsafe{&*table.remap_structs.as_ptr().add(cursor).cast()};
			let remap_type=unsafe{header.head.r#type()};
			if remap_type==RemappingTypeHeader::TYPE_DRHD
			{
				let bar=unsafe{header.drhd.bar};
				trace!("Detected Intel VT-d DRHD BAR: 0x{bar:X}");
				x.dmar_units.push(Arc::new(IntelDmarUnit
				{
					bar:MappedDescriptor::map(bar,PAGE_SIZE),
					fault_record_offset:0,
					fault_record_limit:0,
					iotlb_reg_offset:0
				}));
			}
			else if remap_type==RemappingTypeHeader::TYPE_RMRR
			{
				let (base,lim,seg)=unsafe{(header.rmrr.region_base,header.rmrr.region_limit,header.rmrr.segment)};
				trace!("RMRR Base: 0x{base:X}, Limit: 0x{lim:X}, PCI Segment: 0x{seg:X}");
			}
			cursor+=unsafe{header.head.len()};
		}
	}
	let mut req_sup=true;
	for bar in x.dmar_units.iter_mut()
	{
		let virt=bar.bar.virt.load(Ordering::Relaxed);
		let cap=unsafe{virt.byte_add(CapabilityRegister::MMIO_OFFSET).cast::<CapabilityRegister>().read_volatile()};
		let ext_cap=unsafe{virt.add(ExtendedCapabilityRegister::MMIO_OFFSET).cast::<ExtendedCapabilityRegister>().read_volatile()};
		{
			let bar=Arc::get_mut(bar).unwrap();
			bar.fault_record_offset=cap.fro()<<4;
			bar.fault_record_limit=cap.nfr()+1;
			bar.iotlb_reg_offset=ext_cap.iro()<<4;
		}
		info!("IOMMU Fault-Record Offset: 0x{:X}, Limit: {}, IOTLB Register Offset: 0x{:X}",bar.fault_record_offset,bar.fault_record_limit,bar.iotlb_reg_offset);
		let gst=unsafe{virt.add(GlobalStatusRegister::MMIO_OFFSET).cast::<GlobalStatusRegister>().read_volatile()};
		if gst.tes()
		{
			info!("Note: IOMMU has already enabled translation!");
		}
		// Require at least 39-bit address width.
		req_sup&=cap.sagaw()>1;
		// Require 1GiB and 2MiB page frame sizes.
		// This requirement may be lifted in future, as VMware's emulated IOMMU does not support them.
		req_sup&=cap.sslps()&3==3;
		x.common_sagaw&=cap.sagaw();
		trace!("Common SAGAW: 0x{:X}",x.common_sagaw);
	}
	if req_sup
	{
		x.pml5.push(IntelIommuPageEntryDescriptor{gpa_start:0,descriptor:MemoryDescriptor::alloc().unwrap()});
		trace!("IOMMU PML5E: 0x{:X}",x.pml5[0].descriptor.phys);
		Some(x)
	}
	else
	{
		error!("The Intel VT-d implementation of this machine does not meet minimum requirement!");
		None
	}
}