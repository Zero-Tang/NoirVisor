/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
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

use crate::{print,println,dbg_print,xpf_core::nvbdk::*};

// Use a macro to reduce effort making bit definitions for nested page table entries.
macro_rules! build_npt_entry_bit_def
{
	($name:tt,$pos:literal) =>
	{
		paste!
		{
			pub const [<NPT_ENTRY_ $name:upper>]:u64=$pos;
			pub const [<NPT_ENTRY_ $name:upper _BIT>]:u64=1<<$pos;
		}		
	};
}

build_npt_entry_bit_def!(PRESENT,0);
build_npt_entry_bit_def!(WRITE,1);
build_npt_entry_bit_def!(USER,2);
build_npt_entry_bit_def!(PWT,3);
build_npt_entry_bit_def!(PCD,4);
build_npt_entry_bit_def!(ACCESSED,5);
build_npt_entry_bit_def!(DIRTY,6);
build_npt_entry_bit_def!(PAGE_SIZE,7);
build_npt_entry_bit_def!(PAT,7);
build_npt_entry_bit_def!(GLOBAL,8);
build_npt_entry_bit_def!(PAT_HI,12);
build_npt_entry_bit_def!(NO_EXECUTE,63);

macro_rules! build_npt_entry_get_set_bit
{
	($field:tt) =>
	{
		paste!
		{
			#[inline] fn [<get_ $field:lower>](&self)->bool
			{
				(self.get_raw()&[<NPT_ENTRY_ $field:upper _BIT>])!=0
			}

			#[inline] fn [<set_ $field:lower>](&mut self,val:bool)
			{
				let old=self.get_raw();
				self.set_raw(if val {old|[<NPT_ENTRY_ $field:upper _BIT>]} else {old&![<NPT_ENTRY_ $field:upper _BIT>]});
			}
		}
	};
}

/// # Trait `NptField`
/// This trait defines methods for fields on NPT entries. \
/// Do not implement the optional trait methods!
trait NptField
{
	/// # `get_raw` trait method
	/// Use this method only when you are modifying page table entry.
	fn get_raw(&self)->u64;

	/// # `set_raw` trait method
	/// Use this method only when you are modifying page table entry.
	fn set_raw(&mut self,val:u64);

	/// # `get_next_level_base` trait method
	/// This method can act in ambiguous way:
	/// - If this entry is PTE, large PDE or huge PDPTE, this method returns the page base.
	/// - Otherwise, this method returns the base of next level.
	fn get_next_level_base(&self)->u64;

	/// # `set_next_level_base` trait method
	/// This method can act in ambiguous way:
	/// - If this entry is PTE, large PDE or huge PDPTE, this method sets the page base.
	/// - Otherwise, this method sets the base of next level.
	fn set_next_level_base(&mut self,val:u64);

	/// # `get_pat` trait method
	/// This method gets the `pat` bit. Implement this trait only if this entry defines `pat` (e.g.: PTE).
	/// - If this entry does not define `pat` (e.g.: PML4E), return None.
	/// - Otherwise, return Some<bool>.
	fn get_pat(&self)->Option<bool>
	{
		None
	}

	/// # `set_pat` trait method
	/// This method sets the `pat` bit. Implement this trait only if this entry defines `pat` (e.g.: PTE).
	/// - If this entry does not define `pat` (e.g.: PML4E), return None.
	/// - Otherwise, return Some<bool>.
	fn set_pat(&self,_val:bool) {}

	build_npt_entry_get_set_bit!(present);
	build_npt_entry_get_set_bit!(write);
	build_npt_entry_get_set_bit!(user);
	build_npt_entry_get_set_bit!(pwt);
	build_npt_entry_get_set_bit!(pcd);
	build_npt_entry_get_set_bit!(accessed);

	/// # `is_dirty` trait method
	/// This method checks if this entry is dirty. Implement this trait only if this entry defines dirty bit (e.g.: PTE).
	/// - If this entry does not define dirty bit (e.g.: PML4E), return None.
	/// - Otherwise, return Some<bool>.
	#[inline] fn is_dirty(&self)->Option<bool>
	{
		None
	}

	build_npt_entry_get_set_bit!(no_execute);
}

trait NptSizeField:NptField
{
	build_npt_entry_get_set_bit!(page_size);
}

macro_rules! derive_npt_field
{
	() =>
	{
		#[inline] fn get_raw(&self)->u64
		{
			self.0
		}

		#[inline] fn set_raw(&mut self,val:u64)
		{
			self.0=val;
		}
	};
}

// Page-Map-Level-4 Entry (Bits 39-47)
#[repr(C)] pub struct NptPml4e(pub u64);

impl NptField for NptPml4e
{
	derive_npt_field!();
	
	fn get_next_level_base(&self)->u64
	{
		phys_page_4kb_base(self.0 as usize) as u64
	}
	
	fn set_next_level_base(&mut self,val:u64) 
	{
		self.0&=0xFFF0000000000FFF;
		self.0|=page_4kb_base(val as usize) as u64;
	}
}

impl NptPml4e
{
	pub fn new(present:bool,write:bool,user:bool,pdpte:u64,nx:bool)->Self
	{
		let val:u64=(present as u64)<<NPT_ENTRY_PRESENT |
					(write as u64)<<NPT_ENTRY_WRITE |
					(user as u64)<<NPT_ENTRY_USER |
					page_4kb_base(pdpte as usize) as u64 |
					(nx as u64)<<NPT_ENTRY_NO_EXECUTE;
		Self(val)
	}
}

// Page-Directory-Pointer-Table Entry (Bits 30-38)
#[repr(C)] pub struct NptPdpte(pub u64);

impl NptSizeField for NptPdpte {}

impl NptField for NptPdpte
{
	derive_npt_field!();
	
	fn get_next_level_base(&self)->u64
	{
		if self.get_page_size()
		{
			phys_page_1gb_base(self.0 as usize) as u64
		}
		else
		{
			phys_page_4kb_base(self.0 as usize) as u64
		}
	}
	
	fn set_next_level_base(&mut self,val:u64)
	{
		if self.get_page_size()
		{
			self.0&=PHYS_PAGE_1GB_MASK;
			self.0|=phys_page_1gb_base(val as usize) as u64;
		}
		else
		{
			self.0&=PHYS_PAGE_4KB_MASK;
			self.0|=phys_page_4kb_base(val as usize) as u64;
		}
	}
}

impl NptPdpte
{
	pub fn new(present:bool,write:bool,user:bool,pde:u64,nx:bool)->Self
	{
		let val:u64=(present as u64)<<NPT_ENTRY_PRESENT |
					(write as u64)<<NPT_ENTRY_WRITE |
					(user as u64)<<NPT_ENTRY_USER |
					page_4kb_base(pde as usize) as u64 |
					(nx as u64)<<NPT_ENTRY_NO_EXECUTE;
		Self(val)
	}
}

// Page-Directory Entry (Bits 21-29)
#[repr(C)] pub struct NptPde(pub u64);

impl NptSizeField for NptPde {}

impl NptField for NptPde
{
	derive_npt_field!();
	
	fn get_next_level_base(&self)->u64
	{
		if self.get_page_size()
		{
			phys_page_2mb_base(self.0 as usize) as u64
		}
		else
		{
			phys_page_4kb_base(self.0 as usize) as u64
		}
	}
	
	fn set_next_level_base(&mut self,val:u64)
	{
		if self.get_page_size()
		{
			self.0&=PHYS_PAGE_2MB_MASK;
			self.0|=phys_page_2mb_base(val as usize) as u64;
		}
		else
		{
			self.0&=PHYS_PAGE_4KB_MASK;
			self.0|=phys_page_4kb_base(val as usize) as u64;
		}
	}
}

impl NptPde
{
	pub fn new(present:bool,write:bool,user:bool,pte:u64,nx:bool)->Self
	{
		let val:u64=(present as u64)<<NPT_ENTRY_PRESENT |
					(write as u64)<<NPT_ENTRY_WRITE |
					(user as u64)<<NPT_ENTRY_USER |
					page_4kb_base(pte as usize) as u64 |
					(nx as u64)<<NPT_ENTRY_NO_EXECUTE;
		Self(val)
	}

	pub fn new_large(present:bool,write:bool,user:bool,page_base:u64,nx:bool)->Self
	{
		let val:u64=(present as u64)<<NPT_ENTRY_PRESENT |
					(write as u64)<<NPT_ENTRY_WRITE |
					(user as u64)<<NPT_ENTRY_USER |
					NPT_ENTRY_PAGE_SIZE_BIT |
					page_2mb_base(page_base as usize) as u64 |
					(nx as u64)<<NPT_ENTRY_NO_EXECUTE;
		Self(val)
	}
}

// Page-Table Entry (Bits 12-20)
#[repr(C)] pub struct NptPte(pub u64);

impl NptField for NptPte
{
	derive_npt_field!();
	
	fn get_next_level_base(&self)->u64
	{
		phys_page_4kb_base(self.0 as usize) as u64
	}
	
	fn set_next_level_base(&mut self,val:u64)
	{
		self.0&=PHYS_PAGE_4KB_MASK;
		self.0|=phys_page_4kb_base(val as usize) as u64;
	}
}

impl NptPte
{
	pub fn new(present:bool,write:bool,user:bool,page_base:u64,nx:bool)->Self
	{
		let val:u64=(present as u64)<<NPT_ENTRY_PRESENT |
					(write as u64)<<NPT_ENTRY_WRITE |
					(user as u64)<<NPT_ENTRY_USER |
					page_4kb_base(page_base as usize) as u64 |
					(nx as u64)<<NPT_ENTRY_NO_EXECUTE;
		Self(val)
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

	pub fn locate_pdpte_mut(&mut self,gpa:u64)->&mut NptPdpte
	{
		let pfn=page_1gb_count(gpa as usize);
		unsafe 
		{
			let pdpte_p=self.pdpte.virt as *mut NptPdpte;
			&mut *pdpte_p.add(pfn)
		}
	}

	pub fn split_pdpte(&mut self,gpa:u64)
	{
		let pde_md=alloc_contd_pages(PAGE_SIZE);
		match pde_md
		{
			Some(md)=>
			{
				let gpa_start=page_1gb_base(gpa as usize) as u64;
				let pde_array=md.virt as *mut NptPde;
				let pde_d=SvmNptPageTableDescriptor
				{
					gpa_start:gpa_start,
					table:md
				};
				let pdpte_p=self.locate_pdpte_mut(gpa);
				for i in 0..512
				{
					unsafe 
					{
						let new_pde=NptPde::new_large(pdpte_p.get_present(),pdpte_p.get_write(),pdpte_p.get_user(),gpa_start+page_2mb_mult(i) as u64,pdpte_p.get_no_execute());
						pde_array.add(i).write(new_pde);
					}
				}
				println!("Splitted PDPTE Entry: {:p} for GPA 0x{:016X}",pdpte_p,gpa);
				pdpte_p.set_page_size(false);
				pdpte_p.set_next_level_base(md.phys);
				self.pde.push(pde_d);
			}
			None=>panic!("Failed to split PDPTE while allocating PDE!")
		}
	}

	fn locate_pde_mut(&mut self,gpa:u64)->Option<&mut SvmNptPageTableDescriptor>
	{
		for pde_p in &mut self.pde
		{
			if gpa>=pde_p.gpa_start && gpa<pde_p.gpa_start+PAGE_1GB_SIZE as u64
			{
				return Some(pde_p);
			}
		}
		None
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
					let pde_v=if l {NptPde::new_large(r,w,true,hpa,!x)} else {NptPde::new(r,w,true,pde_p.get_next_level_base(),!x)};
					*pde_p=pde_v;
				}
			}
			None=>panic!("Failed to update PDE!")
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
		unsafe
		{
			noir_enum_allocated_large_pages(SvmNptManager::enum_page_rt,self as *mut Self as *mut c_void);
		}
	}

	pub fn cleanup(&mut self)
	{
		unimplemented!("Cleaning up NPT Manager...");
	}
}

// Page Fault Code
// Use a macro to reduce effort in defining fault codes.
macro_rules! build_fault_code_bit_def
{
	($field:tt,$pos:literal) =>
	{
		paste!
		{
			const [<NPT_FAULT_IS_ $field:upper>]:u64=$pos;
			const [<NPT_FAULT_IS_ $field:upper _BIT>]:u64=1<<$pos;
		}
	};
}

build_fault_code_bit_def!(PRESENT,0);
build_fault_code_bit_def!(WRITE,1);
build_fault_code_bit_def!(USER,2);
build_fault_code_bit_def!(RESERVED,3);
build_fault_code_bit_def!(CODE_READ,4);
build_fault_code_bit_def!(SHADOW_STACK,6);
build_fault_code_bit_def!(TRANSLATE_FINAL_HPA,32);
build_fault_code_bit_def!(TRANSLATE_PAGE_TABLE,33);
build_fault_code_bit_def!(SUPERVISOR_SHADOW_STACK,37);

pub struct NptFaultCode(pub u64);

// Use macro to reduce effort in making fault checker.
macro_rules! build_fault_code_checker
{
	($field:tt) =>
	{
		paste!
		{
			#[inline] pub fn [<is_ $field:lower>](&self)->bool
			{
				(self.0 & [<NPT_FAULT_IS_ $field:upper _BIT>])==[<NPT_FAULT_IS_ $field:upper _BIT>]
			}
		}
	};
}

impl NptFaultCode
{
	build_fault_code_checker!(present);
	build_fault_code_checker!(write);
	build_fault_code_checker!(user);
	build_fault_code_checker!(reserved);
	build_fault_code_checker!(code_read);
	build_fault_code_checker!(shadow_stack);
	build_fault_code_checker!(translate_final_hpa);
	build_fault_code_checker!(translate_page_table);
	build_fault_code_checker!(supervisor_shadow_stack);
}

impl Display for NptFaultCode
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
	{
		write!(f,"Code={:X}. ",self.0)?;
		// Exhaust all bit definitions.
		write!(f,"Page is {}",if self.is_present() {"present"} else {"absent"})?;
		write!(f,", access is {}",if self.is_write() {"write"} else {"not write"})?;
		write!(f,", {}",if self.is_user() {"user"} else {"supervisor"})?;
		write!(f,", {} instruction fetch",if self.is_code_read() {"is"} else {"is not"})?;
		write!(f,", {} shadow stack",if self.is_shadow_stack() {"is"} else {"is not"})?;
		write!(f,", reserved bits {} set",if self.is_reserved() {"are"} else {"are not"})?;
		write!(f,", translating Final HPA {}",if self.is_translate_final_hpa() {"failed"} else {"succeeded"})?;
		write!(f,", translating page table {}",if self.is_translate_page_table() {"failed"} else {"succeeded"})?;
		write!(f,", page {} supervisor shadow stack.",if self.is_supervisor_shadow_stack() {"is"} else {"is not"})
	}
}