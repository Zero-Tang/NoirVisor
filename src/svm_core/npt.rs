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

use paste::paste;

use crate::*;

use xpf_core::{ci::enum_ci_phys_page, ioflt::IoAddressSpace, nvbdk::*,dlalloc::*};

macro_rules! derive_npt_common_fields
{
	() =>
	{
		build_bit_mut_method!(present,0);
		build_bit_mut_method!(write,1);
		build_bit_mut_method!(user,2);
		build_bit_mut_method!(pwt,3);
		build_bit_mut_method!(pcd,4);
		build_bit_mut_method!(acceseed,5);
		build_int_mut_method!(avl,9,3,u64);
		build_bit_mut_method!(nx,63);
	};
}

macro_rules! derive_intermediate_new_method
{
	($field_name:tt) =>
	{
		paste!
		{
			build_int_mut_method!($field_name,12,40,u64);
			#[inline] pub fn new(present:bool,write:bool,user:bool,$field_name:u64,nx:bool)->Self
			{
				let mut s=Self(0);
				s.set_present(present);
				s.set_write(write);
				s.set_user(user);
				s.[<set_ $field_name>](page_4kb_count($field_name as usize) as u64);
				s.set_nx(nx);
				s
			}
		}
	};
}

macro_rules! derive_last_entry_fields
{
	($pat_bit:literal) =>
	{
		build_bit_mut_method!(pat,$pat_bit);
	};
	($pat_bit:literal,$ps_bit:literal) =>
	{
		derive_last_entry_fields!($pat_bit);
		build_bit_mut_method!(page_size,7);
		build_int_mut_method!(page_base,$ps_bit,(52-$ps_bit),u64);
	};
}

macro_rules! derive_last_new_method
{
	() =>
	{
		paste!
		{
			build_int_mut_method!(page_base,12,40,u64);
			#[inline] pub fn new(present:bool,write:bool,user:bool,page_base:u64,nx:bool)->Self
			{
				let mut s=Self(1<<7);
				s.set_present(present);
				s.set_write(write);
				s.set_user(user);
				s.set_page_base(page_4kb_count(page_base as usize) as u64);
				s.set_nx(nx);
				s
			}
		}
	};
	($page_size:tt,$adj:tt) =>
	{
		paste!
		{
			#[inline] pub fn [<new_ $adj>](present:bool,write:bool,user:bool,page_base:u64,nx:bool)->Self
			{
				let mut s=Self(1<<7);
				s.set_present(present);
				s.set_write(write);
				s.set_user(user);
				s.set_page_base([<page_ $page_size _count>](page_base as usize) as u64);
				s.set_nx(nx);
				s
			}
		}
	};
}

// Page-Map-Level-4 Entry (Bits 39-47)
#[repr(C)] pub struct NptPml4e(pub u64);

impl NptPml4e
{
	derive_npt_common_fields!();
	derive_intermediate_new_method!(pdpte);
}

// Page-Directory-Pointer-Table Entry (Bits 30-38)
#[repr(C)] pub struct NptPdpte(pub u64);

impl NptPdpte
{
	derive_npt_common_fields!();
	derive_last_entry_fields!(12,30);
	derive_intermediate_new_method!(pde);
	derive_last_new_method!(1gb,huge);
}

// Page-Directory Entry (Bits 21-29)
#[repr(C)] pub struct NptPde(pub u64);

impl NptPde
{
	derive_npt_common_fields!();
	derive_last_entry_fields!(12,21);
	derive_intermediate_new_method!(pte);
	derive_last_new_method!(2mb,large);
}

// Page-Table Entry (Bits 12-20)
#[repr(C)] pub struct NptPte(pub u64);

impl NptPte
{
	derive_npt_common_fields!();
	derive_last_entry_fields!(7);
	derive_last_new_method!();
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
		println!("PML4E is allocated at {:p}",self.pml4e.virt);
		let pdpte=alloc_2mb_page();
		match pdpte
		{
			Some(md)=>self.pdpte=md,
			None=>panic!("Failed to allocate PDPTE!")
		}
		println!("PDPTE is allocated at {:p}",self.pdpte.virt);
		for i in 0..512
		{
			for j in 0..512
			{
				let k=(i<<PAGE_SHIFT_DIFF)+j;
				let mut pdpte_v=NptPdpte::new(true,true,true,page_1gb_mult(k) as u64,false);
				pdpte_v.set_page_size(true);
				unsafe 
				{
					let pdpte_p=(self.pdpte.virt as *mut NptPdpte).add(k);
					pdpte_p.write(pdpte_v);
				}
			}
			let pml4e_v=NptPml4e::new(true,true,true,self.pdpte.phys+page_mult(i) as u64,false);
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
		*pdpte_p=if h {NptPdpte::new_huge(r,w,true,hpa,!x)} else {NptPdpte::new(r,w,true,page_mult(pdpte_p.get_pde() as usize) as u64,!x)};
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
					let gpa_start=page_1gb_base(gpa as usize) as u64;
					let pde_array=md.virt as *mut NptPde;
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
							let new_pde=NptPde::new_large(pdpte_p.get_present(),pdpte_p.get_write(),pdpte_p.get_user(),gpa_start+page_2mb_mult(i) as u64,pdpte_p.get_nx());
							pde_array.add(i).write(new_pde);
						}
					}
					println!("Splitted PDPTE Entry: {:p} for GPA 0x{:016X}",pdpte_p,gpa);
					pdpte_p.set_page_size(false);
					pdpte_p.set_pde(page_count(md.phys as usize) as u64);
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
					let pde_v=if l {NptPde::new_large(r,w,true,hpa,!x)} else {NptPde::new(r,w,true,page_mult(pde_p.get_pte() as usize) as u64,!x)};
					*pde_p=pde_v;
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
							gpa_start:page_2mb_base(gpa as usize) as u64,
							table:md
						};
						let pte_array=md.virt as *mut NptPte;
						for i in 0..512
						{
							unsafe
							{
								let new_pte=NptPte::new((*pde_p).get_present(),(*pde_p).get_write(),(*pde_p).get_user(),pte_d.gpa_start+page_4kb_mult(i) as u64,(*pde_p).get_nx());
								pte_array.add(i).write(new_pte);
							}
						}
						println!("Splitted PDE Entry: {:p} for GPA 0x{:016X}",pde_p,gpa);
						unsafe
						{
							(*pde_p).set_page_size(false);
							(*pde_p).set_pte(page_count(md.phys as usize) as u64);
						}
						self.pte.push(pte_d);
					}
				}
				None=>panic!("Failed to split PDE while allocating PDE! GPA=0x{:016X}",gpa)
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
					let pte_v=NptPte::new(r,w,true,hpa,!x);
					*pte_p=pte_v;
				}
			}
			None=>panic!("Failed to update PTE for GPA 0x{:016X}!",gpa)
		}
	}

	extern "C" fn enum_page_rt(start:u64,length:u64,context:*mut c_void)
	{
		let s:&mut Self=unsafe{&mut *(context as *mut Self)};
		if length!=PAGE_2MB_SIZE as u64
		{
			panic!("While enumerating allocated large pages, Page 0x{:016X} does not have exactly 2MiB size! (0x{:X})",start,length);
		}
		println!("Protecting page range 0x{:X} to 0x{:X}...",start,start+length);
		s.update_pde(start,start,true,false,false,true);
	}

	pub fn protect_allocated_pages(&mut self)
	{
		enum_allocated_large_pages(SvmNptManager::enum_page_rt,self as *mut Self as *mut c_void);
	}

	pub fn protect_ci(&mut self)
	{
		let ci_pages=unsafe{&*enum_ci_phys_page()};
		// Enumerate all pages in CI and update the PTEs.
		for p in ci_pages
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
				let increment:u64=if page_1gb_offset(remainder as usize)==0 && page_1gb_offset(p as usize)==0
				{
					PAGE_1GB_SIZE as u64
				}
				else if page_2mb_offset(remainder as usize)==0 && page_2mb_offset(p as usize)==0
				{
					PAGE_2MB_SIZE as u64
				}
				else
				{
					PAGE_4KB_SIZE as u64
				};
				match increment as usize
				{
					PAGE_1GB_SIZE=>self.update_pdpte(p,0,r.input_handler.is_none(),false,false,true),
					PAGE_2MB_SIZE=>self.update_pde(p,0,r.input_handler.is_none(),false,false,true),
					PAGE_4KB_SIZE=>self.update_pte(p,0,r.input_handler.is_none(),false,false),
					_=>panic!("Unknown increment size: 0x{:X}!",increment)
				}
				p+=increment;
			}
		}
	}

	pub fn cleanup(&mut self)
	{
		unimplemented!("Cleaning up NPT Manager...");
	}
}

pub struct NptFaultCode(pub u64);

impl NptFaultCode
{
	pub fn from_u64(v:u64)->Self
	{
		Self(v)
	}

	build_bit_mut_method!(present,0);
	build_bit_mut_method!(write,1);
	build_bit_mut_method!(user,2);
	build_bit_mut_method!(reserved,3);
	build_bit_mut_method!(code_read,4);
	build_bit_mut_method!(shadow_stack,6);
	build_bit_mut_method!(translate_final_hpa,32);
	build_bit_mut_method!(translate_page_table,33);
	build_bit_mut_method!(supervisor_shadow_stack,37);
}

impl Display for NptFaultCode
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
	{
		write!(f,"Code={:X}. ",self.0)?;
		// Exhaust all bit definitions.
		write!(f,"Page is {}",if self.get_present() {"present"} else {"absent"})?;
		write!(f,", access is {}",if self.get_write() {"write"} else {"not write"})?;
		write!(f,", {}",if self.get_user() {"user"} else {"supervisor"})?;
		write!(f,", {} instruction fetch",if self.get_code_read() {"is"} else {"is not"})?;
		write!(f,", {} shadow stack",if self.get_shadow_stack() {"is"} else {"is not"})?;
		write!(f,", reserved bits {} set",if self.get_reserved() {"are"} else {"are not"})?;
		write!(f,", translating Final HPA {}",if self.get_translate_final_hpa() {"failed"} else {"succeeded"})?;
		write!(f,", translating page table {}",if self.get_translate_page_table() {"failed"} else {"succeeded"})?;
		write!(f,", page {} supervisor shadow stack.",if self.get_supervisor_shadow_stack() {"is"} else {"is not"})
	}
}