/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file manages VMCB in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{fmt::Display,ffi::c_void};
use alloc::vec::Vec;

use bitfield_struct::bitfield;
use log::*;

use crate::*;
use xpf_core::{ci::CI_MANAGER, ioflt::IoAddressSpace, nvbdk::*,allocator::*};

// Page-Map-Level-4 Entry (Bits 39-47)
#[bitfield(u64)] pub struct NptPml4e
{
	pub present:bool,
	pub write:bool,
	pub user:bool,
	pub pwt:bool,
	pub pcd:bool,
	pub accessed:bool,
	pub ignored0:bool,
	#[bits(2)] pub rsvd:u64,
	#[bits(3)] pub avl:u64,
	#[bits(40)] pub pdpte_base:u64,
	#[bits(11)] pub available:u64,
	pub nx:bool
}

impl NptPml4e
{
	pub fn construct(present:bool,write:bool,user:bool,pdpte_base:u64,nx:bool)->Self
	{
		let mut v=Self::from_bits(0);
		v.set_present(present);
		v.set_write(write);
		v.set_user(user);
		v.set_nx(nx);
		v.set_pdpte_base(page_4kb_count(pdpte_base));
		v
	}
}

// Page-Directory-Pointer-Table Entry (Bits 30-38)
#[bitfield(u64)] pub struct NptPdpte
{
	pub present:bool,
	pub write:bool,
	pub user:bool,
	pub pwt:bool,
	pub pcd:bool,
	pub accessed:bool,
	pub ignored0:bool,
	pub page_size:bool,
	pub ignored1:bool,
	#[bits(3)] pub avl:u64,
	#[bits(40)] pub pde_base:u64,
	#[bits(11)] pub available:u64,
	pub nx:bool
}

impl NptPdpte
{
	pub fn construct(present:bool,write:bool,user:bool,pde_base:u64,nx:bool)->Self
	{
		let mut v=Self::from_bits(0);
		v.set_present(present);
		v.set_write(write);
		v.set_user(user);
		v.set_nx(nx);
		v.set_pde_base(page_4kb_count(pde_base));
		v
	}
}

#[bitfield(u64)] pub struct NptHugePdpte
{
	pub present:bool,
	pub write:bool,
	pub user:bool,
	pub pwt:bool,
	pub pcd:bool,
	pub accessed:bool,
	pub dirty:bool,
	pub page_size:bool,
	pub global:bool,
	#[bits(3)] pub avl:u64,
	pub pat:bool,
	#[bits(17)] rsvd:u64,
	#[bits(22)] pub page_base:u64,
	#[bits(7)] pub available:u64,
	#[bits(4)] pub page_key:u64,
	pub nx:bool
}

impl NptHugePdpte
{
	pub fn construct(present:bool,write:bool,user:bool,page_base:u64,nx:bool)->Self
	{
		let mut v=Self::from_bits(0);
		v.set_present(present);
		v.set_write(write);
		v.set_user(user);
		v.set_page_size(true);
		v.set_page_base(page_1gb_count(page_base));
		v.set_nx(nx);
		v
	}
}

// Page-Directory Entry (Bits 21-29)
#[bitfield(u64)] pub struct NptPde
{
	pub present:bool,
	pub write:bool,
	pub user:bool,
	pub pwt:bool,
	pub pcd:bool,
	pub accessed:bool,
	pub ignored0:bool,
	pub page_size:bool,
	pub ignored1:bool,
	#[bits(3)] pub avl:u64,
	#[bits(40)] pub pte_base:u64,
	#[bits(11)] pub available:u64,
	pub nx:bool
}

impl NptPde
{
	pub fn construct(present:bool,write:bool,user:bool,pte_base:u64,nx:bool)->Self
	{
		let mut v=Self::from_bits(0);
		v.set_present(present);
		v.set_write(write);
		v.set_user(user);
		v.set_nx(nx);
		v.set_pte_base(page_4kb_count(pte_base));
		v
	}
}

#[bitfield(u64)] pub struct NptLargePde
{
	pub present:bool,
	pub write:bool,
	pub user:bool,
	pub pwt:bool,
	pub pcd:bool,
	pub accessed:bool,
	pub dirty:bool,
	pub page_size:bool,
	pub global:bool,
	#[bits(3)] pub avl:u64,
	pub pat:bool,
	#[bits(8)] rsvd:u64,
	#[bits(31)] pub page_base:u64,
	#[bits(7)] pub available:u64,
	#[bits(4)] pub page_key:u64,
	pub nx:bool
}

impl NptLargePde
{
	pub fn construct(present:bool,write:bool,user:bool,page_base:u64,nx:bool)->Self
	{
		let mut v=Self::from_bits(0);
		v.set_present(present);
		v.set_write(write);
		v.set_user(user);
		v.set_page_size(true);
		v.set_page_base(page_2mb_count(page_base));
		v.set_nx(nx);
		v
	}
}

// Page-Table Entry (Bits 12-20)
#[bitfield(u64)] pub struct NptPte
{
	pub present:bool,
	pub write:bool,
	pub user:bool,
	pub pwt:bool,
	pub pcd:bool,
	pub accessed:bool,
	pub dirty:bool,
	pub pat:bool,
	pub global:bool,
	#[bits(3)] pub avl:u64,
	#[bits(40)] pub page_base:u64,
	#[bits(7)] pub available:u64,
	#[bits(4)] pub page_key:u64,
	pub nx:bool
}

impl NptPte
{
	pub fn construct(present:bool,write:bool,user:bool,page_base:u64,nx:bool)->Self
	{
		let mut v=Self::from_bits(0);
		v.set_present(present);
		v.set_write(write);
		v.set_user(user);
		v.set_nx(nx);
		v.set_page_base(page_4kb_count(page_base));
		v
	}
}

pub struct SvmNptPageTableDescriptor
{
	pub table:MemoryDescriptor,
	pub gpa_start:u64
}

impl Default for SvmNptPageTableDescriptor
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

pub struct SvmNptManager
{
	pub pml4e:MemoryDescriptor,
	pub pdpte:MemoryDescriptor,
	pub pde:Vec<SvmNptPageTableDescriptor>,
	pub pte:Vec<SvmNptPageTableDescriptor>
}

impl Default for SvmNptManager
{
	fn default() -> Self
	{
		Self
		{
			pml4e:MemoryDescriptor::null(),
			pdpte:MemoryDescriptor::null(),
			pde:Vec::new(),
			pte:Vec::new()
		}
	}
}

impl SvmNptManager
{
	pub fn build_identity_map(&mut self)
	{
		let pml4e=alloc_contd_pages(PAGE_SIZE);
		match pml4e
		{
			Some(md)=>self.pml4e=md,
			None=>panic!("Failed to allocate PML4E!")
		}
		debug!("PML4E is allocated at {:p}",self.pml4e.virt);
		let pdpte=alloc_2mb_page();
		match pdpte
		{
			Some(md)=>self.pdpte=md,
			None=>panic!("Failed to allocate PDPTE!")
		}
		debug!("PDPTE is allocated at {:p}",self.pdpte.virt);
		for i in 0..512
		{
			for j in 0..512
			{
				let k=(i<<PAGE_SHIFT_DIFF)+j;
				let mut pdpte_v=NptPdpte::construct(true,true,true,page_1gb_mult(k) as u64,false);
				pdpte_v.set_page_size(true);
				unsafe 
				{
					let pdpte_p=(self.pdpte.virt as *mut NptPdpte).add(k);
					pdpte_p.write(pdpte_v);
				}
			}
			let pml4e_v=NptPml4e::construct(true,true,true,self.pdpte.phys+page_mult(i) as u64,false);
			unsafe
			{
				let pml4e_p=(self.pml4e.virt as *mut NptPml4e).add(i);
				pml4e_p.write(pml4e_v);
			}
		}
	}

	pub fn update_pdpte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool,h:bool)
	{
		let pdpte_p=self.locate_pdpte_mut(gpa);
		*pdpte_p=NptPdpte::from_bits
		(
			if h
			{
				NptHugePdpte::construct(r,w,true,hpa,!x).into_bits()
			}
			else
			{
				NptPdpte::construct(r,w,true,page_mult(pdpte_p.pde_base()),!x).into_bits()
			}
		);
	}

	fn locate_pdpte_mut(&mut self,gpa:u64)->&mut NptPdpte
	{
		let pfn=page_1gb_count(gpa as usize);
		unsafe 
		{
			let pdpte_p=self.pdpte.virt as *mut NptPdpte;
			&mut *pdpte_p.add(pfn)
		}
	}

	fn split_pdpte(&mut self,gpa:u64)
	{
		// Check if we have splitted it before.
		if self.locate_pde_mut(gpa).is_none()
		{
			// Target PDE is absent.
			let pde_md=alloc_contd_pages(PAGE_SIZE);
			match pde_md
			{
				Some(md)=>
				{
					let gpa_start=page_1gb_base(gpa);
					let pde_array=md.virt as *mut NptLargePde;
					let pde_d=SvmNptPageTableDescriptor
					{
						gpa_start,
						table:md
					};
					let pdpte_p=self.locate_pdpte_mut(gpa);
					for i in 0..512
					{
						unsafe 
						{
							let new_pde=NptLargePde::construct(pdpte_p.present(),pdpte_p.write(),pdpte_p.user(),gpa_start+page_2mb_mult(i) as u64,pdpte_p.nx());
							pde_array.add(i).write(new_pde);
						}
					}
					debug!("Splitted PDPTE Entry: {pdpte_p:p} for GPA 0x{gpa:016X}");
					pdpte_p.set_page_size(false);
					pdpte_p.set_pde_base(page_count(md.phys));
					self.pde.push(pde_d);
				}
				None=>panic!("Failed to split PDPTE while allocating PDE!")
			}
		}
	}

	fn locate_pde_mut(&mut self,gpa:u64)->Option<&mut SvmNptPageTableDescriptor>
	{
		self.pde.iter_mut().find(|pde_p| gpa>=pde_p.gpa_start && gpa<pde_p.gpa_start+PAGE_1GB_SIZE as u64)
	}

	pub fn update_pde(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool,l:bool)
	{
		let mut pde_op=self.locate_pde_mut(gpa);
		if pde_op.is_none()
		{
			// Build PDE.
			self.split_pdpte(gpa);
			pde_op=self.locate_pde_mut(gpa);
		}
		match pde_op
		{
			Some(pde_d)=>
			{
				let pde_array=pde_d.table.virt as *mut NptPde;
				let index=page_entry_index(page_2mb_count(gpa as usize));
				unsafe 
				{
					let pde_p=&mut *pde_array.add(index);
					*pde_p=NptPde::from_bits
					(
						if l
						{
							NptLargePde::construct(r,w,true,hpa,!x).into_bits()
						}
						else
						{
							NptPde::construct(r,w,true,page_mult(pde_p.pte_base()),!x).into_bits()
						}
					);
				}
			}
			None=>panic!("Failed to update PDE!")
		}
	}

	fn split_pde(&mut self,gpa:u64)
	{
		// Check if we have splitted it before.
		if self.locate_pte_mut(gpa).is_none()
		{
			// This 2MiB page has not been described yet.
			let pte_md=alloc_contd_pages(PAGE_SIZE);
			match pte_md
			{
				Some(md)=>
				{
					// Also split the PDPTE.
					self.split_pdpte(gpa);
					let pde_op=self.locate_pde_mut(gpa);
					assert!(pde_op.is_some(),"PDPTE was not splitted!");
					if let Some(pde_d)=pde_op
					{
						let pfn_index=page_2mb_count(gpa as usize);
						let pde_index=page_entry_index(pfn_index);
						let pde_p=unsafe{pde_d.table.virt.byte_add(pde_index<<3)} as *mut NptPde;
						let pte_d=SvmNptPageTableDescriptor
						{
							gpa_start:page_2mb_base(gpa),
							table:md
						};
						let pte_array=md.virt as *mut NptPte;
						for i in 0..512
						{
							unsafe
							{
								let new_pte=NptPte::construct((*pde_p).present(),(*pde_p).write(),(*pde_p).user(),pte_d.gpa_start+page_4kb_mult(i) as u64,(*pde_p).nx());
								pte_array.add(i).write(new_pte);
							}
						}
						debug!("Splitted PDE Entry: {pde_p:p} for GPA 0x{gpa:016X}");
						unsafe
						{
							(*pde_p).set_page_size(false);
							(*pde_p).set_pte_base(page_count(md.phys));
						}
						self.pte.push(pte_d);
					}
				}
				None=>panic!("Failed to split PDE while allocating PDE! GPA=0x{gpa:016X}")
			}
		}
	}

	fn locate_pte_mut(&mut self,gpa:u64)->Option<&mut SvmNptPageTableDescriptor>
	{
		self.pte.iter_mut().find(|pte_p| gpa>=pte_p.gpa_start && gpa<pte_p.gpa_start+PAGE_2MB_SIZE as u64)
	}

	pub fn update_pte(&mut self,gpa:u64,hpa:u64,r:bool,w:bool,x:bool)
	{
		let mut pte_op=self.locate_pte_mut(gpa);
		if pte_op.is_none()
		{
			// Build PTE.
			self.split_pde(gpa);
			pte_op=self.locate_pte_mut(gpa);
		}
		match pte_op
		{
			Some(pte_d)=>
			{
				let pte_array=pte_d.table.virt as *mut NptPte;
				let index=page_entry_index(page_4kb_count(gpa as usize));
				unsafe
				{
					let pte_p=&mut *pte_array.add(index);
					*pte_p=NptPte::construct(r,w,true,hpa,!x);
				}
			}
			None=>panic!("Failed to update PTE for GPA 0x{:016X}!",gpa)
		}
	}

	extern "C" fn enum_page_rt(start:u64,length:u64,context:*mut c_void)
	{
		let s:&mut Self=unsafe{&mut *context.cast()};
		if length!=PAGE_2MB_SIZE as u64
		{
			panic!("While enumerating allocated large pages, Page 0x{:016X} does not have exactly 2MiB size! (0x{:X})",start,length);
		}
		debug!("Protecting page range 0x{:X} to 0x{:X}...",start,start+length);
		s.update_pde(start,start,true,false,false,true);
	}

	pub fn protect_allocated_pages(&mut self)
	{
		enum_allocated_large_pages(SvmNptManager::enum_page_rt,self as *mut Self as *mut c_void);
	}

	pub fn protect_ci(&mut self)
	{
		let ci=CI_MANAGER.read();
		for p in ci.into_iter()
		{
			self.update_pte(*p,*p,true,false,true);
		}
	}

	pub fn setup_mmio_filter(&mut self,mmio_space:&IoAddressSpace<u64>)
	{
		// NPT will handle MMIO filters in the host system.
		for r in &mmio_space.regions
		{
			let mut p=r.addr;
			let end=r.addr+r.size;
			while p<end
			{
				let remainder=end-p;
				let increment=if page_1gb_offset(remainder)==0 && page_1gb_offset(p)==0
				{
					PAGE_1GB_SIZE
				}
				else if page_2mb_offset(remainder)==0 && page_2mb_offset(p)==0
				{
					PAGE_2MB_SIZE
				}
				else
				{
					PAGE_4KB_SIZE
				};
				match increment
				{
					PAGE_1GB_SIZE=>self.update_pdpte(p,0,r.input_handler.is_none(),false,false,true),
					PAGE_2MB_SIZE=>self.update_pde(p,0,r.input_handler.is_none(),false,false,true),
					PAGE_4KB_SIZE=>self.update_pte(p,0,r.input_handler.is_none(),false,false),
					_=>panic!("Unknown increment size: 0x{:X}!",increment)
				}
				p+=increment as u64;
			}
		}
	}
}

#[bitfield(u64)] pub struct NptFaultCode
{
	pub present:bool,
	pub write:bool,
	pub user:bool,
	pub reserved:bool,
	pub code_fetch:bool,
	pub rsvd0:bool,
	pub shadow_stack:bool,
	#[bits(25)] rsvd1:u64,
	pub translate_final_hpa:bool,
	pub translate_page_table:bool,
	#[bits(3)] rsvd2:u64,
	pub supervisor_shadow_stack:bool,
	#[bits(26)] rsvd3:u64
}

impl NptFaultCode
{
}

impl Display for NptFaultCode
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
	{
		write!(f,"Code={:X}. ",self.0)?;
		// Exhaust all bit definitions.
		write!(f,"Page is {}",if self.present() {"present"} else {"absent"})?;
		write!(f,", access is {}",if self.write() {"write"} else {"not write"})?;
		write!(f,", {}",if self.user() {"user"} else {"supervisor"})?;
		write!(f,", {} instruction fetch",if self.code_fetch() {"is"} else {"is not"})?;
		write!(f,", {} shadow stack",if self.shadow_stack() {"is"} else {"is not"})?;
		write!(f,", reserved bits {} set",if self.reserved() {"are"} else {"are not"})?;
		write!(f,", translating Final HPA {}",if self.translate_final_hpa() {"failed"} else {"succeeded"})?;
		write!(f,", translating page table {}",if self.translate_page_table() {"failed"} else {"succeeded"})?;
		write!(f,", page {} supervisor shadow stack.",if self.supervisor_shadow_stack() {"is"} else {"is not"})
	}
}