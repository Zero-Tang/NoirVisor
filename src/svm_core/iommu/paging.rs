/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file is the AMD-Vi paging driver of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{cmp::Ordering, slice};

use paste::paste;
use crate::{svm_core::iommu::SvmIommuManager, xpf_core::{dlalloc::{alloc_contd_pages, free_contd_pages}, nvbdk::*}, *};

/// ## SvmIommuPde
/// The concept of PDE in AMD-Vi is drastically different from AMD-V NPT, Intel EPT and Intel VT-d. \
/// In AMD-Vi, PDE simply means a non-terminal level in the map. It can be followed by a PDE or a PTE. \
/// In other words, PDE in AMD-Vi is equivalent to normal PDE, PDPTE, PML4E and PML5E in AMD-V NPT.
#[repr(C)] pub struct SvmIommuPde(pub u64);

impl SvmIommuPde
{
	build_bit_mut_method!(present,0);
	build_bit_mut_method!(accessed,5);
	build_int_mut_method!(next_level,9,3,u64);
	build_int_mut_method!(pte,12,40,u64);
	build_bit_mut_method!(read,61);
	build_bit_mut_method!(write,62);

	pub fn new_pde<const N:u64>(next_level:u64,read:bool,write:bool)->Self
	{
		let mut template=Self(next_level);
		template.set_present(true);
		template.set_next_level(N-1);
		template.set_read(read);
		template.set_write(write);
		template
	}
}

/// ## SvmIommuPte
/// The concept of PTE in AMD-Vi is drastically different from AMD-V NPT, Intel EPT and Intel VT-d. \
/// In AMD-Vi, PTE simply means a terminal level in the map. It determines the base of the page. \
/// In other words, PTE in AMD-Vi is equivalent to PTE, large PDE and huge PDPTE in AMD-V NPT.
#[repr(C)] pub struct SvmIommuPte(pub u64);

impl SvmIommuPte
{
	build_bit_mut_method!(present,0);
	build_bit_mut_method!(accessed,5);
	build_bit_mut_method!(dirty,6);
	build_int_get_method!(next_level,9,3,u64);
	build_int_mut_method!(pfn,12,40,u64);
	build_bit_mut_method!(u,59);
	build_bit_mut_method!(force_coherent,60);
	build_bit_mut_method!(read,61);
	build_bit_mut_method!(write,62);

	pub fn new_pte(page_base:u64,read:bool,write:bool)->Self
	{
		let mut template=Self(page_base);
		template.set_present(true);
		template.set_read(read);
		template.set_write(write);
		template
	}
}

pub struct SvmIommuPmlManager
{
	pub pxe:MemoryDescriptor,
	pub gpa_start:u64,
	pub length:u64
}

impl SvmIommuPmlManager
{
	/// ## `new` method
	/// It is **your** duty to split the pages after creating the manager. \
	/// This method initializes the all entries with PTEs (i.e.: terminal level)
	pub fn new(gpa_base:u64,page_size:usize)->Self
	{
		match alloc_contd_pages(PAGE_SIZE)
		{
			Some(md)=>
			{
				let pte_p:*mut SvmIommuPte=md.virt.cast();
				println!("Allocated PML at {pte_p:p} with page-size of 0x{page_size:X}...");
				for i in 0..512
				{
					unsafe{pte_p.add(i as usize).write(SvmIommuPte::new_pte(gpa_base+(page_size as u64)*i,true,true))};
				}
				Self
				{
					pxe:md,
					gpa_start:gpa_base,
					length:(page_size<<PAGE_SHIFT) as u64
				}
			}
			None=>panic!("Failed to allocate PTE entries for AMD-Vi!")
		}
	}

	fn compare_gpa(&self,phys:u64)->Ordering
	{
		if phys<self.gpa_start
		{
			Ordering::Greater
		}
		else if phys>=self.gpa_start+self.length
		{
			Ordering::Less
		}
		else
		{
			Ordering::Equal
		}
	}
}

impl Drop for SvmIommuPmlManager
{
	fn drop(&mut self)
	{
		free_contd_pages(self.pxe.virt,PAGE_SIZE);
	}
}

impl SvmIommuManager
{
	pub fn split_pml4e(&mut self,gpa:u64)
	{
		// Check if the PML4E has been splitted.
		if let Err(i)=self.pml3e.binary_search_by(|m| m.compare_gpa(gpa))
		{
			// Not described yet.
			match alloc_contd_pages(PAGE_SIZE)
			{
				Some(md)=>
				{
					let gpa_start=page_512gb_base(gpa);
					let pml3e_array=unsafe{slice::from_raw_parts_mut(md.virt as *mut SvmIommuPte,PAGE_TABLE_ENTRIES)};
					let pml3e_d=SvmIommuPmlManager
					{
						gpa_start,
						pxe:md,
						length:PAGE_512GB_SIZE as u64
					};
					// Initialize all PML3Es with identity-mapping.
					for (i,p) in pml3e_array.iter_mut().enumerate()
					{
						*p=SvmIommuPte::new_pte(gpa_start+page_1gb_mult(i as u64),true,true);
					}
					// Locate and set the PDE.
					let pml4e_p=unsafe{&mut *self.locate_pml4e_mut(gpa).unwrap().cast::<SvmIommuPde>()};
					pml4e_p.set_next_level(3);
					pml4e_p.set_pte(page_count(md.phys));
					// Insert to the manager.
					self.pml3e.insert(i,pml3e_d);
				}
				None=>panic!("AMD-Vi: Failed to split PML4E while allocating PML3E! GPA=0x{gpa:016X}")
			}
		}
	}

	pub fn split_pml3e(&mut self,gpa:u64)
	{
		// Check if the PML3E has been splitted.
		if let Err(i)=self.pml2e.binary_search_by(|m| m.compare_gpa(gpa))
		{
			// Not described yet.
			match alloc_contd_pages(PAGE_SIZE)
			{
				Some(md)=>
				{
					// Also split the PML4E.
					self.split_pml4e(gpa);
					let gpa_start=page_1gb_base(gpa);
					let pml2e_array=unsafe{slice::from_raw_parts_mut(md.virt as *mut SvmIommuPte,PAGE_TABLE_ENTRIES)};
					let pml2e_d=SvmIommuPmlManager
					{
						gpa_start,
						pxe:md,
						length:PAGE_1GB_SIZE as u64
					};
					// Initialize all PML2Es with identity-mapping.
					for (i,p) in pml2e_array.iter_mut().enumerate()
					{
						*p=SvmIommuPte::new_pte(gpa_start+page_2mb_mult(i as u64),true,true);
					}
					// Locate and set the PDE.
					let pml3e_p=unsafe{&mut *self.locate_pml3e_mut(gpa).unwrap().cast::<SvmIommuPde>()};
					pml3e_p.set_next_level(2);
					pml3e_p.set_pte(page_count(md.phys));
					// Insert to the manager.
					self.pml2e.insert(i,pml2e_d);
				}
				None=>panic!("AMD-Vi: Failed to split PML3E while allocating PML2E! GPA=0x{gpa:016X}")
			}
		}
	}

	pub fn split_pml2e(&mut self,gpa:u64)
	{
		// Check if the PML2E has been splitted.
		if let Err(i)=self.pml1e.binary_search_by(|m| m.compare_gpa(gpa))
		{
			// Not described yet.
			match alloc_contd_pages(PAGE_SIZE)
			{
				Some(md)=>
				{
					// Also split the PML3E.
					self.split_pml3e(gpa);
					let gpa_start=page_2mb_base(gpa);
					let pml1e_array=unsafe{slice::from_raw_parts_mut(md.virt as *mut SvmIommuPte,PAGE_TABLE_ENTRIES)};
					let pml1e_d=SvmIommuPmlManager
					{
						gpa_start,
						pxe:md,
						length:PAGE_2MB_SIZE as u64
					};
					// Initialize all PML1Es with identity-mapping.
					for (i,p) in pml1e_array.iter_mut().enumerate()
					{
						*p=SvmIommuPte::new_pte(gpa_start+page_4kb_mult(i as u64),true,true);
					}
					// Locate and set the PDE.
					let pml2e_p=unsafe{&mut *self.locate_pml2e_mut(gpa).unwrap().cast::<SvmIommuPde>()};
					pml2e_p.set_next_level(1);
					pml2e_p.set_pte(page_count(md.phys));
					// Insert to the manager.
					self.pml1e.insert(i,pml1e_d);
				}
				None=>panic!("AMD-Vi: Failed to split PML2E while allocating PML1E! GPA=0x{gpa:016X}")
			}
		}
	}

	pub fn update_pml4e(&mut self,gpa:u64,hpa:u64,r:bool,w:bool)
	{
		// PML4E is the top level.
		let pml4e_p=unsafe{&mut *self.locate_pml4e_mut(gpa).unwrap()};
		pml4e_p.set_read(r);
		pml4e_p.set_write(w);
		pml4e_p.set_pfn(page_512gb_count(hpa));
	}

	pub fn update_pml3e(&mut self,gpa:u64,hpa:u64,r:bool,w:bool)
	{
		// Check if the large page is described.
		if self.locate_pml3e_mut(gpa).is_none()
		{
			// Split the PML4E.
			self.split_pml4e(gpa);
		}
		// Now it should be splitted. Locate the PTE.
		let pml3e_p=unsafe{&mut *self.locate_pml3e_mut(gpa).unwrap()};
		pml3e_p.set_read(r);
		pml3e_p.set_write(w);
		pml3e_p.set_pfn(page_count(hpa));
	}

	pub fn update_pml2e(&mut self,gpa:u64,hpa:u64,r:bool,w:bool)
	{
		// Check if the large page is described.
		if self.locate_pml2e_mut(gpa).is_none()
		{
			// Split the PML3E.
			self.split_pml3e(gpa);
		}
		// Now it should be splitted. Locate the PTE.
		let pml2e_p=unsafe{&mut *self.locate_pml2e_mut(gpa).unwrap()};
		pml2e_p.set_read(r);
		pml2e_p.set_write(w);
		pml2e_p.set_pfn(page_count(hpa));
	}

	pub fn update_pml1e(&mut self,gpa:u64,hpa:u64,r:bool,w:bool)
	{
		// Check if the large page is described.
		if self.locate_pml1e_mut(gpa).is_none()
		{
			// Split the PML2E.
			self.split_pml2e(gpa);
		}
		// Now it should be splitted. Locate the PTE.
		let pml1e_p=unsafe{&mut *self.locate_pml1e_mut(gpa).unwrap()};
		pml1e_p.set_read(r);
		pml1e_p.set_write(w);
		pml1e_p.set_pfn(page_count(hpa));
	}

	pub fn locate_pml4e_mut(&mut self,gpa:u64)->Option<*mut SvmIommuPte>
	{
		let pfn=page_512gb_count(gpa as usize);
		assert!(pfn<PAGE_TABLE_ENTRIES);
		Some(unsafe{self.pml4e.virt.cast::<SvmIommuPte>().add(pfn)})
	}

	pub fn locate_pml3e_mut(&mut self,gpa:u64)->Option<*mut SvmIommuPte>
	{
		match self.pml3e.binary_search_by(|m| m.compare_gpa(gpa))
		{
			Ok(i)=>
			{
				let pfn=page_entry_index(page_1gb_count(gpa as usize));
				Some(unsafe{self.pml3e[i].pxe.virt.cast::<SvmIommuPte>().add(pfn)})
			}
			Err(_)=>None
		}
	}

	pub fn locate_pml2e_mut(&mut self,gpa:u64)->Option<*mut SvmIommuPte>
	{
		match self.pml2e.binary_search_by(|m| m.compare_gpa(gpa))
		{
			Ok(i)=>
			{
				let pfn=page_entry_index(page_2mb_count(gpa as usize));
				Some(unsafe{self.pml2e[i].pxe.virt.cast::<SvmIommuPte>().add(pfn)})
			}
			Err(_)=>None
		}
	}

	pub fn locate_pml1e_mut(&mut self,gpa:u64)->Option<*mut SvmIommuPte>
	{
		match self.pml1e.binary_search_by(|m| m.compare_gpa(gpa))
		{
			Ok(i)=>
			{
				let pfn=page_entry_index(page_4kb_count(gpa as usize));
				Some(unsafe{self.pml1e[i].pxe.virt.cast::<SvmIommuPte>().add(pfn)})
			}
			Err(_)=>None
		}
	}
}