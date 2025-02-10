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

use core::cmp::Ordering;

use alloc::vec::Vec;

use crate::*;
use super::ia32::msr::MSR_SMRR_PHYS_BASE;
use xpf_core::{asm::msr::rdmsr, dlalloc::{alloc_2mb_page, alloc_contd_pages}, nvbdk::*, x86::{caching::*, msr::*}};

use paste::paste;

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
	pub def_type:MtrrDefTypeMsr,
	pub mtrr_cap:MtrrCapMsr,
	pub pa_width:u64
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
			def_type:MtrrDefTypeMsr::read(),
			mtrr_cap:MtrrCapMsr::read(),
			pa_width:0
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
			println!("Splitting PDPTE for GPA 0x{gpa:X}...");
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
			println!("Splitting PDE for GPA 0x{gpa:X}...");
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

	pub fn update_pte_memory_type(&mut self,gpa:u64,new_type:u64,force_update:bool)
	{
		self.split_pde(gpa);
		let pte_p=self.locate_pte(gpa).unwrap();
		unsafe
		{
			if new_type<(*pte_p).get_memory_type() || force_update
			{
				(*pte_p).set_memory_type(new_type);
			}
		}
		
	}

	pub fn update_pde_memory_type(&mut self,gpa:u64,new_type:u64,force_update:bool)
	{
		self.split_pdpte(gpa);
		let pde_p=self.locate_pde(gpa).unwrap();
		unsafe
		{
			if (*pde_p).get_page_size()
			{
				if new_type<(*pde_p).get_memory_type() || force_update
				{
					(*pde_p).set_memory_type(new_type);
				}
			}
			else
			{
				panic!("Updating splitted PDE is not yet supported!");
			}
		}
	}

	pub fn update_pdpte_memory_type(&mut self,gpa:u64,new_type:u64,force_update:bool)
	{
		let pdpte_i=page_1gb_count(gpa as usize);
		unsafe
		{
			let pdpte_p=self.pdpte.virt.cast::<EptHugePdpte>().add(pdpte_i);
			if (*pdpte_p).get_page_size()
			{
				let old_type=(*pdpte_p).get_memory_type();
				if new_type<old_type || force_update
				{
					(*pdpte_p).set_memory_type(new_type);
				}
			}
			else
			{
				panic!("Updating splitted PDPTE is not yet supported!");
			}
		}
	}

	fn update_per_var_mtrr(&mut self,mtrr_msr_index:u32)
	{
		let mtrr_base=MtrrVariableRangeBaseMsr::read(mtrr_msr_index);
		let mtrr_mask=MtrrVariableRangeMaskMsr::read(mtrr_msr_index+1);
		if let Some((base,size,mem_type))=calculate_mtrr_range(mtrr_base,mtrr_mask,self.pa_width)
		{
			// Ignore MTRRs that define the same memory type as default MTRR.
			if mtrr_base.get_type()!=self.def_type.get_type()
			{
				println!("Base: 0x{base:X}, Size: 0x{size:X}, Type: {mem_type}");
				let mut addr=base;
				while addr<base+size
				{
					let remainder=(base+size-addr) as usize;
					let increment=if page_1gb_offset(addr as usize)==0
					{
						if (0..PAGE_2MB_SIZE).contains(&remainder)
						{
							PAGE_4KB_SIZE
						}
						else if (PAGE_2MB_SIZE..PAGE_1GB_SIZE).contains(&remainder)
						{
							PAGE_2MB_SIZE
						}
						else
						{
							PAGE_1GB_SIZE
						}
					}
					else if page_2mb_offset(addr as usize)==0
					{
						if (PAGE_2MB_SIZE..PAGE_1GB_SIZE).contains(&remainder)
						{
							PAGE_2MB_SIZE
						}
						else
						{
							PAGE_1GB_SIZE
						}
					}
					else
					{
						PAGE_4KB_SIZE
					};
					match increment
					{
						PAGE_1GB_SIZE=>self.update_pdpte_memory_type(addr,mem_type as u64,false),
						_=>println!("Unknown Increment: 0x{increment:X}!")
					}
					addr+=increment as u64;
				}
			}
		}
	}

	pub fn update_per_fixed_mtrr(&mut self,mtrr_msr_index:u32,gpa:u64,pages:usize)
	{
		let t=rdmsr(mtrr_msr_index).to_le_bytes();
		for (i,r )in t.iter().enumerate()
		{
			for j in 0..pages
			{
				self.update_pte_memory_type(gpa+page_4kb_mult(i*pages+j) as u64,*r as u64,true);
			}
		}
	}

	pub fn update_by_mtrr(&mut self)
	{
		println!("MTRR Default Type MSR: 0x{:X}, Capability: 0x{:X}",self.def_type.0,self.mtrr_cap.0);
		if self.def_type.get_enabled()
		{
			let mtrr_count=self.mtrr_cap.get_var_mtrr_count();
			// Traverse variable-range MTRRs.
			for i in 0..mtrr_count
			{
				self.update_per_var_mtrr(MSR_MTRR_PHYS_BASE0+(i<<1) as u32);
			}
			if self.mtrr_cap.get_support_smrr()
			{
				println!("SMRR is also supported!");
				self.update_per_var_mtrr(MSR_SMRR_PHYS_BASE);
			}
			// Traverse fixed-range MTRRs.
			if self.def_type.get_fixed_enabled()
			{
				// First of all, split PDE of first 2MiB.
				self.update_per_fixed_mtrr(MSR_MTRR_FIX64K_00000,0x00000,16);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX16K_80000,0x80000,4);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX16K_A0000,0xA0000,4);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_C0000,0xC0000,1);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_C8000,0xC8000,1);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_D0000,0xD0000,1);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_D8000,0xD8000,1);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_E0000,0xE0000,1);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_E8000,0xE8000,1);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_F0000,0xF0000,1);
				self.update_per_fixed_mtrr(MSR_MTRR_FIX4K_F8000,0xF8000,1);
			}
		}
	}

	pub fn build_identity_map(&mut self)
	{
		let (a,_,_,_)=cpuid2(CPUID_EXT_PROCESSOR_CAPABILITY_PARAMETERS_EXTENDED_ID,0);
		self.pa_width=(a&0xFF) as u64;
		match alloc_contd_pages(PAGE_SIZE)
		{
			Some(md)=>self.pml4e=md,
			None=>panic!("Failed to allocate PML4E")
		}
		println!("PML4E is allocated at {:p}",self.pml4e.virt);
		match alloc_2mb_page()
		{
			Some(md)=>self.pdpte=md,
			None=>panic!("Failed to allocate PDPTE")
		}
		println!("PDPTE is allocated at {:p}",self.pdpte.virt);
		for i in 0..PAGE_TABLE_ENTRIES64
		{
			for j in 0..PAGE_TABLE_ENTRIES64
			{
				let k=(i<<PAGE_SHIFT_DIFF)+j;
				let pdpte_v=EptHugePdpte::new(true,true,true,self.def_type.get_type(),page_1gb_mult(k) as u64);
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
		self.update_by_mtrr();
	}
}