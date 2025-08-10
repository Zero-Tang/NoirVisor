/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file manages EPT in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{cmp::Ordering, ffi::c_void};
use alloc::vec::Vec;

use crate::{vt_core::ia32::msr::VmxEptVpidCapMsr, xpf_core::{ci::CI_MANAGER, allocator::enum_allocated_large_pages}, *};
use xpf_core::{allocator::{alloc_2mb_page, alloc_contd_pages}, nvbdk::*, x86::caching::*};

use paste::paste;
use log::*;

macro_rules! derive_common_ept_fields
{
	() =>
	{
		build_bit_mut_method!(read,0);
		build_bit_mut_method!(write,1);
		build_bit_mut_method!(execute,2);
		build_bit_mut_method!(accessed,8);
		build_bit_mut_method!(user_execute,10);
	};
}

macro_rules! derive_last_entry_fields
{
	() =>
	{
		build_int_mut_method!(memory_type,3,3,u64);
		build_bit_mut_method!(ignore_pat,6);
		build_bit_mut_method!(dirty,9);
		build_bit_mut_method!(verify_guest_paging,57);
		build_bit_mut_method!(paging_write,58);
		build_bit_mut_method!(sss,60);
		build_bit_mut_method!(suppress_ve,63);
	};
	($ps_bit:literal) =>
	{
		derive_last_entry_fields!();
		build_bit_mut_method!(page_size,7);
		build_int_mut_method!(page_base,$ps_bit,(52-$ps_bit),u64);
	}
}

macro_rules! derive_intermediate_new_method
{
	($field_name:tt) =>
	{
		paste!
		{
			pub fn new(r:bool,w:bool,x:bool,$field_name:u64)->Self
			{
				let mut s=Self(0);
				s.set_read(r);
				s.set_write(w);
				s.set_execute(x);
				s.[<set_ $field_name>](page_4kb_count($field_name));
				s
			}
		}
	};
}

macro_rules! derive_last_new_method
{
	($page_size:tt,$large:literal) =>
	{
		paste!
		{
			pub fn new(r:bool,w:bool,x:bool,memory_type:u64,page_base:u64)->Self
			{
				let mut s=Self(($large as u64)<<7);
				s.set_read(r);
				s.set_write(w);
				s.set_execute(x);
				s.set_memory_type(memory_type);
				s.set_page_base([<page_ $page_size _count>](page_base));
				s
			}
		}
	};
}

pub struct EptPml4e(pub u64);
impl EptPml4e
{
	derive_common_ept_fields!();
	build_int_mut_method!(pdpte_entry,12,40,u64);
	derive_intermediate_new_method!(pdpte_entry);
}

pub struct EptHugePdpte(pub u64);
impl EptHugePdpte
{
	derive_common_ept_fields!();
	derive_last_entry_fields!(30);
	derive_last_new_method!(1gb,true);
}

pub struct EptPdpte(pub u64);
impl EptPdpte
{
	derive_common_ept_fields!();
	build_int_mut_method!(pde_entry,12,40,u64);
	derive_intermediate_new_method!(pde_entry);
}

pub struct EptLargePde(pub u64);
impl EptLargePde
{
	derive_common_ept_fields!();
	derive_last_entry_fields!(21);
	derive_last_new_method!(2mb,true);
}

pub struct EptPde(pub u64);
impl EptPde
{
	derive_common_ept_fields!();
	build_int_mut_method!(pte_entry,12,40,u64);
	derive_intermediate_new_method!(pte_entry);
}

pub struct EptPte(pub u64);
impl EptPte
{
	derive_common_ept_fields!();
	derive_last_entry_fields!();
	build_int_mut_method!(page_base,12,40,u64);
	build_bit_mut_method!(subpage_write,61);
	derive_last_new_method!(4kb,false);
}

pub struct VtEptPageTableDescriptor
{
	pub table:MemoryDescriptor,
	pub gpa_start:u64
}

impl VtEptPageTableDescriptor
{
	pub fn cmp_by_addr(&self,gpa:u64,range:usize)->Ordering
	{
		if gpa<self.gpa_start
		{
			Ordering::Less
		}
		else if gpa>=self.gpa_start+range as u64
		{
			Ordering::Greater
		}
		else
		{
			Ordering::Equal
		}
	}
}

impl Default for VtEptPageTableDescriptor
{
	fn default() -> Self
	{
		Self
		{
			table:MemoryDescriptor::null(),
			gpa_start:0
		}
	}
}

pub struct VtEptManager
{
	pub pml4e:MemoryDescriptor,
	pub pdpte:MemoryDescriptor,
	pub pde:Vec<VtEptPageTableDescriptor>,
	pub pte:Vec<VtEptPageTableDescriptor>,
	pub mtrr_mgr:MtrrManager,
	pub ept_cap:VmxEptVpidCapMsr
}

impl Default for VtEptManager
{
	fn default() -> Self
	{
		Self
		{
			pml4e:MemoryDescriptor::null(),
			pdpte:MemoryDescriptor::null(),
			pde:Vec::new(),
			pte:Vec::new(),
			mtrr_mgr:MtrrManager::default(),
			ept_cap:VmxEptVpidCapMsr(0)
		}
	}
}

impl VtEptManager
{
	pub fn locate_pdpte(&mut self,gpa:u64)->*mut EptHugePdpte
	{
		let pfn=page_1gb_count(gpa as usize);
		unsafe
		{
			let pdpte_p=self.pdpte.virt as *mut EptHugePdpte;
			pdpte_p.add(pfn)
		}
	}

	pub fn split_pdpte(&mut self,gpa:u64)
	{
		// Search PDE list.
		if let Err(i)=self.pde.binary_search_by(|d| d.cmp_by_addr(gpa,PAGE_1GB_SIZE))
		{
			// This 1GiB page has not been described yet.
			debug!("Splitting PDPTE for GPA 0x{gpa:X}...");
			match alloc_contd_pages(PAGE_SIZE)
			{
				Some(md)=>
				{
					// Initialize descriptor.
					let d=VtEptPageTableDescriptor
					{
						table:md,
						gpa_start:page_1gb_base(gpa)
					};
					// Initialize PDE Page.
					let pde_p:*mut EptLargePde=md.virt.cast();
					unsafe
					{
						let pdpte_v=self.locate_pdpte(gpa);
						let mt=(*pdpte_v).get_memory_type();
						for i in 0..PAGE_TABLE_ENTRIES64
						{
							*pde_p.add(i)=EptLargePde::new(true,true,true,mt,d.gpa_start+page_2mb_mult(i) as u64);
						}
						// Update PDPTE Entry.
						(*pdpte_v).set_memory_type(0);
						(*pdpte_v).set_page_size(false);
						let pdpte_p:*mut EptPdpte=pdpte_v.cast();
						(*pdpte_p).set_pde_entry(page_4kb_count(md.phys));
					}
					// Insert to EPT Manager.
					self.pde.insert(i,d);
				}
				None=>panic!("Failed to split PDPTE!")
			}
		}
	}

	pub fn locate_pde(&mut self,gpa:u64)->Option<*mut EptLargePde>
	{
		match self.pde.binary_search_by(|d| d.cmp_by_addr(gpa,PAGE_1GB_SIZE))
		{
			Ok(i)=>
			{
				let pde_d=&self.pde[i];
				let pde_pfn=page_entry_index(page_2mb_count(gpa as usize));
				Some(unsafe{pde_d.table.virt.cast::<EptLargePde>().add(pde_pfn)})
			}
			Err(_)=>None
		}
	}

	pub fn split_pde(&mut self,gpa:u64)
	{
		// Search PTE list.
		if let Err(i)=self.pte.binary_search_by(|d| d.cmp_by_addr(gpa,PAGE_2MB_SIZE))
		{
			// This 2MiB page has not been described yet.
			debug!("Splitting PDE for GPA 0x{gpa:X}...");
			match alloc_contd_pages(PAGE_SIZE)
			{
				Some(md)=>
				{
					// Split the PDPTE first.
					self.split_pdpte(gpa);
					// Initialize descriptor.
					let d=VtEptPageTableDescriptor
					{
						table:md,
						gpa_start:page_2mb_base(gpa)
					};
					// Initialize PTE page.
					let pte_p:*mut EptPte=md.virt.cast();
					unsafe
					{
						let pde_v=self.locate_pde(gpa).unwrap();
						let mt=(*pde_v).get_memory_type();
						for i in 0..PAGE_TABLE_ENTRIES64
						{
							*pte_p.add(i)=EptPte::new(true,true,true,mt,d.gpa_start+page_4kb_mult(i) as u64);
						}
						// Update PDE Entry.
						(*pde_v).set_memory_type(0);
						(*pde_v).set_page_size(false);
						let pde_p:*mut EptPde=pde_v.cast();
						(*pde_p).set_pte_entry(page_4kb_count(md.phys));
					}
					// Insert to EPT Manager.
					self.pte.insert(i,d);
				}
				None=>panic!("Failed to split PDE!")
			}
		}
	}

	pub fn locate_pte(&mut self,gpa:u64)->Option<*mut EptPte>
	{
		match self.pte.binary_search_by(|d| d.cmp_by_addr(gpa,PAGE_2MB_SIZE))
		{
			Ok(i)=>
			{
				let pte_d=&self.pte[i];
				let pte_pfn=page_entry_index(page_4kb_count(gpa as usize));
				Some(unsafe{pte_d.table.virt.cast::<EptPte>().add(pte_pfn)})
			}
			Err(_)=>None
		}
	}

	pub fn update_pte(&mut self,gpa:u64,memory_type:Option<(u64,bool)>,r:Option<bool>,w:Option<bool>,x:Option<bool>)
	{
		self.split_pde(gpa);
		let pte_p=self.locate_pte(gpa).unwrap();
		unsafe
		{
			if let Some((new_type,force_update))=memory_type && (new_type<(*pte_p).get_memory_type() || force_update)
			{
				(*pte_p).set_memory_type(new_type);
			}
			if let Some(p)=r {(*pte_p).set_read(p);}
			if let Some(p)=w {(*pte_p).set_write(p);}
			if let Some(p)=x {(*pte_p).set_execute(p);}
		}
		
	}

	pub fn update_pde(&mut self,gpa:u64,memory_type:Option<(u64,bool)>,r:Option<bool>,w:Option<bool>,x:Option<bool>)
	{
		self.split_pdpte(gpa);
		let pde_p=self.locate_pde(gpa).unwrap();
		unsafe
		{
			if (*pde_p).get_page_size()
			{
				if let Some((new_type,force_update))=memory_type && (new_type<(*pde_p).get_memory_type() || force_update)
				{
					(*pde_p).set_memory_type(new_type);
				}
				if let Some(p)=r {(*pde_p).set_read(p);}
				if let Some(p)=w {(*pde_p).set_write(p);}
				if let Some(p)=x {(*pde_p).set_execute(p);}
			}
			else
			{
				for i in 0..512
				{
					self.update_pte(gpa+page_4kb_mult(i),memory_type,r,w,x);
				}
			}
		}
	}

	pub fn update_pdpte(&mut self,gpa:u64,memory_type:Option<(u64,bool)>,r:Option<bool>,w:Option<bool>,x:Option<bool>)
	{
		let pdpte_i=page_1gb_count(gpa as usize);
		unsafe
		{
			let pdpte_p=self.pdpte.virt.cast::<EptHugePdpte>().add(pdpte_i);
			if (*pdpte_p).get_page_size()
			{
				if let Some((new_type,force_update))=memory_type && (new_type<(*pdpte_p).get_memory_type() || force_update)
				{
					(*pdpte_p).set_memory_type(new_type);
				}
				if let Some(p)=r {(*pdpte_p).set_read(p);}
				if let Some(p)=w {(*pdpte_p).set_write(p);}
				if let Some(p)=x {(*pdpte_p).set_execute(p);}
			}
			else
			{
				for i in 0..512
				{
					self.update_pde(gpa+page_2mb_mult(i),memory_type,r,w,x);
				}
			}
		}
	}

	pub fn update_by_mtrr(&mut self)
	{
		let mut mtrr_mgr=MtrrManager::default();
		mtrr_mgr.init();
		for r in mtrr_mgr.iter()
		{
			for p in r.iter()
			{
				let force_update=match p.source
				{
					MtrrSource::DefaultType=>true,
					MtrrSource::VariableRange=>false,
					MtrrSource::FixedRange=>true,
				};
				let updater_fn=match p.page_size
				{
					0=>Self::update_pte,
					1=>Self::update_pde,
					2=>Self::update_pdpte,
					_=>panic!("Unrecognized page-size identifier: {}!",p.page_size)
				};
				updater_fn(self,p.base,Some((p.memory_type as u64,force_update)),None,None,None);
			}
		}
		self.mtrr_mgr=mtrr_mgr;
	}

	extern "C" fn enum_page_rt(start:u64,length:u64,context:*mut c_void)
	{
		let s:&mut Self=unsafe{&mut *context.cast()};
		if length!=PAGE_2MB_SIZE as u64
		{
			panic!("While enumerating allocated large pages, Page 0x{:016X} does not have exactly 2MiB size! (0x{:X})",start,length);
		}
		debug!("Protecting page range 0x{:X} to 0x{:X}...",start,start+length);
		s.update_pde(start,None,Some(true),Some(false),Some(false));
	}

	pub fn protect_allocated_pages(&mut self)
	{
		enum_allocated_large_pages(Self::enum_page_rt,(self as *mut Self).cast());
	}

	pub fn protect_ci(&mut self)
	{
		let ci=CI_MANAGER.read();
		for p in ci.into_iter()
		{
			self.update_pte(*p,None,None,Some(false),None);
		}
	}

	pub fn build_identity_map(&mut self)
	{
		self.ept_cap=VmxEptVpidCapMsr::read();
		match alloc_contd_pages(PAGE_SIZE)
		{
			Some(md)=>self.pml4e=md,
			None=>panic!("Failed to allocate PML4E")
		}
		debug!("PML4E is allocated at {:p}",self.pml4e.virt);
		match alloc_2mb_page()
		{
			Some(md)=>self.pdpte=md,
			None=>panic!("Failed to allocate PDPTE")
		}
		debug!("PDPTE is allocated at {:p}",self.pdpte.virt);
		for i in 0..PAGE_TABLE_ENTRIES64
		{
			for j in 0..PAGE_TABLE_ENTRIES64
			{
				let k=(i<<PAGE_SHIFT_DIFF)+j;
				let pdpte_v=EptHugePdpte::new(true,true,true,MEMORY_TYPE_WB as u64,page_1gb_mult(k) as u64);
				unsafe
				{
					let pdpte_p=self.pdpte.virt.cast::<EptHugePdpte>().add(k);
					pdpte_p.write(pdpte_v);
				}
			}
			let pml4e_v=EptPml4e::new(true,true,true,self.pdpte.phys+page_mult(i) as u64);
			unsafe
			{
				let pml4e_p=self.pml4e.virt.cast::<EptPml4e>().add(i);
				pml4e_p.write(pml4e_v);
			}
		}
		if !self.ept_cap.get_support_1gb_paging()
		{
			// 1GiB-paging is unsupported in this system. Split all PDPTEs in the lowest 512GiB.
			// Nested-Virtualization provided by VMware doesn't support 1GiB Paging.
			warn!("This system does not support EPT 1GiB-paging!");
			for i in 0..PAGE_TABLE_ENTRIES64
			{
				self.split_pdpte(page_1gb_mult(i as u64));
			}
		}
		self.update_by_mtrr();
	}
}