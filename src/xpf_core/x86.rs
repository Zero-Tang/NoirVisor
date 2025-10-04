/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file lists universal x86 definitions for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

pub mod caching
{
    use super::{cpuid::*,msr::*};
	use crate::xpf_core::{asm::msr::rdmsr, nvbdk::{page_1gb_offset, page_2mb_offset, page_4kb_mult, PAGE_1GB_SIZE, PAGE_2MB_SIZE, PAGE_4KB_SIZE}};

	use log::*;
	use bitfield_struct::bitfield;

	pub const MEMORY_TYPE_UC:u8=0;
	pub const MEMORY_TYPE_WC:u8=1;
	pub const MEMORY_TYPE_WT:u8=4;
	pub const MEMORY_TYPE_WP:u8=5;
	pub const MEMORY_TYPE_WB:u8=6;

	#[bitfield(u64)] pub struct MtrrCapMsr
	{
		pub var_mtrr_count:u8,
		pub support_fixed:bool,
		rsvd0:bool,
		pub support_wc:bool,
		pub support_smrr:bool,
		#[bits(52)] rsvd2:u64
	}

	impl MtrrCapMsr
	{
		#[inline] pub fn read()->Self
		{
			Self(rdmsr(MSR_MTRR_CAP))
		}
	}

	#[bitfield(u64)] pub struct MtrrDefTypeMsr
	{
		pub mtrr_type:u8,
		#[bits(2)] pub rsvd0:u64,
		pub fixed_enabled:bool,
		pub enabled:bool,
		#[bits(52)] rsvd1:u64
	}

	impl MtrrDefTypeMsr
	{
		#[inline] pub fn read()->Self
		{
			Self(rdmsr(MSR_MTRR_DEF_TYPE))
		}
	}

	#[bitfield(u64)] pub struct MtrrVariableRangeBaseMsr
	{
		pub mtrr_type:u8,
		#[bits(4)] rsvd:u64,
		#[bits(52)] pub phys_base:u64,
	}

	impl MtrrVariableRangeBaseMsr
	{
		#[inline] pub fn read(index:u32)->Self
		{
			Self(rdmsr(index))
		}
	}

	#[bitfield(u64)] pub struct MtrrVariableRangeMaskMsr
	{
		#[bits(11)] rsvd:u64,
		pub valid:bool,
		#[bits(52)] pub phys_mask:u64
	}

	impl MtrrVariableRangeMaskMsr
	{
		#[inline] pub fn read(index:u32)->Self
		{
			Self(rdmsr(index))
		}
	}

	#[derive(Clone, Copy)]
	pub enum MtrrSource
	{
		DefaultType,
		VariableRange,
		FixedRange
	}

	pub struct MtrrPage
	{
		pub base:u64,
		pub page_size:u8,
		pub memory_type:u8,
		pub source:MtrrSource
	}

	pub struct MtrrRangeIter<'a>
	{
		source:&'a MtrrRange,
		current:u64
	}

	impl<'a> Iterator for MtrrRangeIter<'a>
	{
		type Item = MtrrPage;
		fn next(&mut self) -> Option<Self::Item>
		{
			let remainder=(self.source.base+self.source.length-self.current) as usize;
			if remainder>0
			{
				// Check alignment
				let (length,increment)=if page_1gb_offset(self.current)==0
				{
					// Current address is 1GiB-aligned. Check remaining size.
					match remainder
					{
						0..PAGE_2MB_SIZE=>(0,PAGE_4KB_SIZE),
						PAGE_2MB_SIZE..PAGE_1GB_SIZE=>(1,PAGE_2MB_SIZE),
						_=>(2,PAGE_1GB_SIZE)
					}
				}
				else if page_2mb_offset(self.current)==0
				{
					// Current address is 2MiB-aligned. Check remaining size.
					match remainder
					{
						0..PAGE_2MB_SIZE=>(0,PAGE_4KB_SIZE),
						_=>(1,PAGE_2MB_SIZE)
					}
				}
				else
				{
					(0,PAGE_4KB_SIZE)
				};
				self.current+=increment as u64;
				Some
				(
					MtrrPage
					{
						base:self.current-increment as u64,
						page_size:length,
						memory_type:self.source.memory_type,
						source:self.source.source
					}
				)
			}
			else
			{
				None
			}
		}
	}

	#[derive(Clone, Copy)]
	pub struct MtrrRange
	{
		pub base:u64,
		pub length:u64,
		pub memory_type:u8,
		pub source:MtrrSource
	}

	impl MtrrRange
	{
		pub fn from_var_mtrr(base:MtrrVariableRangeBaseMsr,mask:MtrrVariableRangeMaskMsr,pa_width:u64)->Option<Self>
		{
			if mask.valid()
			{
				Some
				(
					Self
					{
						base:page_4kb_mult(base.phys_base()),
						length:(1<<pa_width)-page_4kb_mult(mask.phys_mask()),
						memory_type:base.mtrr_type(),
						source:MtrrSource::VariableRange
					}
				)
			}
			else
			{
				None
			}
		}

		pub fn from_raw_parts(base:u64,length:u64,memory_type:u8,source:MtrrSource)->Self
		{
			Self
			{
				base,
				length,
				memory_type,
				source
			}
		}

		pub fn iter(&self)->MtrrRangeIter<'_>
		{
			MtrrRangeIter
			{
				source:self,
				current:self.base
			}
		}
	}

	#[derive(Default)]
	pub struct FixedMtrrManager
	{
		pub fixed64k_00000:[u8;8],
		pub fixed16k_80000:[u8;8],
		pub fixed16k_a0000:[u8;8],
		pub fixed4k_c0000:[u8;8],
		pub fixed4k_c8000:[u8;8],
		pub fixed4k_d0000:[u8;8],
		pub fixed4k_d8000:[u8;8],
		pub fixed4k_e0000:[u8;8],
		pub fixed4k_e8000:[u8;8],
		pub fixed4k_f0000:[u8;8],
		pub fixed4k_f8000:[u8;8],
	}

	pub struct MtrrManagerIter<'a>
	{
		source:&'a MtrrManager,
		// Default-range
		default_base:bool,
		// Variable-range
		var_index:usize,
		smrr:bool,
		// Fixed-range
		fixed_index:usize
	}

	impl<'a> Iterator for MtrrManagerIter<'a>
	{
		type Item = MtrrRange;
		fn next(&mut self) -> Option<Self::Item>
		{
			use MtrrSource::*;
			// The first iteration checks the default base.
			if !self.default_base
			{
				self.default_base=true;
				return Some(MtrrRange::from_raw_parts(0,self.source.max_pa,self.source.def_type,DefaultType));
			}
			// Next, iterate variable-length MTRRs
			if self.var_index<self.source.var_mtrrs.len()
			{
				self.var_index+=1;
				if let Some(range)=self.source.var_mtrrs[self.var_index-1]
				{
					trace!("Iterating Variable MTRR (Base: 0x{:016X}, Length: 0x{:016X})",range.base,range.length);
					return Some(range);
				}
			}
			// After that, check SMRR.
			if !self.smrr
			{
				self.smrr=true;
				// SMRR might be disabled or unsupported.
				if let Some(range)=self.source.smrr
				{
					return Some(range);
				}
			}
			// Finally, iterate all fixed range MTRRs.
			match &self.source.fixed_mtrrs
			{
				Some(fixed_mgr)=>
				{
					let i=&mut self.fixed_index;
					let result=match *i
					{
						0x0..0x8=>Some(MtrrRange::from_raw_parts((*i as u64)<<16,64<<10,fixed_mgr.fixed64k_00000[*i],FixedRange)),
						0x8..0x10=>Some(MtrrRange::from_raw_parts(0x80000+(((*i-0x8) as u64)<<14),16<<10,fixed_mgr.fixed16k_80000[*i-0x8],FixedRange)),
						0x10..0x18=>Some(MtrrRange::from_raw_parts(0xA0000+(((*i-0x10) as u64)<<14),16<<10,fixed_mgr.fixed16k_a0000[*i-0x10],FixedRange)),
						0x18..0x20=>Some(MtrrRange::from_raw_parts(0xC0000+(((*i-0x18) as u64)<<12),4<<10,fixed_mgr.fixed4k_c0000[*i-0x18],FixedRange)),
						0x20..0x28=>Some(MtrrRange::from_raw_parts(0xC8000+(((*i-0x20) as u64)<<12),4<<10,fixed_mgr.fixed4k_c8000[*i-0x20],FixedRange)),
						0x28..0x30=>Some(MtrrRange::from_raw_parts(0xD0000+(((*i-0x28) as u64)<<12),4<<10,fixed_mgr.fixed4k_d0000[*i-0x28],FixedRange)),
						0x30..0x38=>Some(MtrrRange::from_raw_parts(0xD8000+(((*i-0x30) as u64)<<12),4<<10,fixed_mgr.fixed4k_d8000[*i-0x30],FixedRange)),
						0x38..0x40=>Some(MtrrRange::from_raw_parts(0xE0000+(((*i-0x38) as u64)<<12),4<<10,fixed_mgr.fixed4k_e0000[*i-0x38],FixedRange)),
						0x40..0x48=>Some(MtrrRange::from_raw_parts(0xE8000+(((*i-0x40) as u64)<<12),4<<10,fixed_mgr.fixed4k_e8000[*i-0x40],FixedRange)),
						0x48..0x50=>Some(MtrrRange::from_raw_parts(0xF0000+(((*i-0x48) as u64)<<12),4<<10,fixed_mgr.fixed4k_f0000[*i-0x48],FixedRange)),
						0x50..0x58=>Some(MtrrRange::from_raw_parts(0xF8000+(((*i-0x50) as u64)<<12),4<<10,fixed_mgr.fixed4k_f8000[*i-0x50],FixedRange)),
						_=>None
					};
					*i+=1;
					result
				}
				None=>None
			}
		}
	}

	#[derive(Default)]
	pub struct MtrrManager
	{
		pub def_type:u8,
		pub fixed_mtrrs:Option<FixedMtrrManager>,
		pub var_mtrrs:[Option<MtrrRange>;16],
		pub smrr:Option<MtrrRange>,
		pub max_pa:u64
	}

	impl MtrrManager
	{
		pub fn init(&mut self)
		{
			let mtrr_def=MtrrDefTypeMsr::read();
			// Clear the structure.
			self.def_type=0;
			self.fixed_mtrrs=None;
			self.var_mtrrs=[const{None};16];
			self.smrr=None;
			let cap_params=ProcessorCapabilityParameters::cpuid();
			let pa_width=cap_params.phys_addr_size() as u64;
			self.max_pa=1<<pa_width;
			if mtrr_def.enabled()
			{
				// Setup default type.
				self.def_type=mtrr_def.mtrr_type();
				// Setup Fixed MTRRs.
				if mtrr_def.fixed_enabled()
				{
					self.fixed_mtrrs=Some
					(
						FixedMtrrManager
						{
							fixed64k_00000:rdmsr(MSR_MTRR_FIX64K_00000).to_le_bytes(),
							fixed16k_80000:rdmsr(MSR_MTRR_FIX16K_80000).to_le_bytes(),
							fixed16k_a0000:rdmsr(MSR_MTRR_FIX16K_A0000).to_le_bytes(),
							fixed4k_c0000:rdmsr(MSR_MTRR_FIX4K_C0000).to_le_bytes(),
							fixed4k_c8000:rdmsr(MSR_MTRR_FIX4K_C8000).to_le_bytes(),
							fixed4k_d0000:rdmsr(MSR_MTRR_FIX4K_D0000).to_le_bytes(),
							fixed4k_d8000:rdmsr(MSR_MTRR_FIX4K_D8000).to_le_bytes(),
							fixed4k_e0000:rdmsr(MSR_MTRR_FIX4K_E0000).to_le_bytes(),
							fixed4k_e8000:rdmsr(MSR_MTRR_FIX4K_E8000).to_le_bytes(),
							fixed4k_f0000:rdmsr(MSR_MTRR_FIX4K_F0000).to_le_bytes(),
							fixed4k_f8000:rdmsr(MSR_MTRR_FIX4K_F8000).to_le_bytes()
						}
					);
				}
				// Setup Variable MTRRs.
				let mtrr_cap=MtrrCapMsr::read();
				for i in 0..mtrr_cap.var_mtrr_count() as usize
				{
					let mtrr_mask=MtrrVariableRangeMaskMsr::read(MSR_MTRR_PHYS_MASK0+i as u32);
					if mtrr_mask.valid()
					{
						let mtrr_base=MtrrVariableRangeBaseMsr::read(MSR_MTRR_PHYS_BASE0+i as u32);
						self.var_mtrrs[i]=MtrrRange::from_var_mtrr(mtrr_base,mtrr_mask,pa_width);
						trace!("Detected Variable-MTRR (Base=0x{:016X}, Mask=0x{:016X})",mtrr_base.0,mtrr_mask.0);
					}
				}
				// Setup SMRR.
				if mtrr_cap.support_smrr()
				{
					let mask=MtrrVariableRangeMaskMsr::read(MSR_SMRR_PHYS_MASK);
					if mask.valid()
					{
						let base=MtrrVariableRangeBaseMsr::read(MSR_SMRR_PHYS_BASE);
						// Note that SMRR uses only 32-bit physical address.
						self.smrr=MtrrRange::from_var_mtrr(base,mask,32);
					}
				}
			}
		}

		pub fn iter(&self)->MtrrManagerIter<'_>
		{
			MtrrManagerIter
			{
				source:self,
				default_base:false,
				var_index:0,
				smrr:false,
				fixed_index:0
			}
		}
	}
}

pub mod paging
{
	use core::{fmt::{self,Display,Formatter}, slice};	

	use bitfield_struct::bitfield;
	use paste::paste;

	use crate::*;
	use svm_core::amd64::msr::MSR_EFER_LMA;
	use xpf_core::{nvbdk::*, x86::crdr::*};

	macro_rules! build_paging_def
	{
		($prefix:tt,$def:tt,$shift:literal) =>
		{
			paste!
			{
				pub const [<$prefix:upper _PAGING_ $def:upper _BIT>]:u64=$shift;
				pub const [<$prefix:upper _PAGING_ $def:upper>]:u64=1<<$shift;
			}
		};
	}

	// Page-Map-Level-4 Entry (Bits 39-47)
	#[bitfield(u64)] pub struct Pml4e
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

	build_paging_def!(x86,present,0);
	build_paging_def!(x86,write,1);
	build_paging_def!(x86,user,2);
	build_paging_def!(x86,pwt,3);
	build_paging_def!(x86,pcd,4);
	build_paging_def!(x86,accessed,5);
	build_paging_def!(x86,dirty,6);
	build_paging_def!(x86,page_size,7);
	build_paging_def!(x86,pte_pat,7);
	build_paging_def!(x86,global,8);
	build_paging_def!(x86,pat,12);
	build_paging_def!(x86,nx,63);

	pub const PAGING_AVL_BIT:u64=9;
	impl Pml4e
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
	#[bitfield(u64)] pub struct Pdpte
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

	impl Pdpte
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

	#[bitfield(u64)] pub struct HugePdpte
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

	impl HugePdpte
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
	#[bitfield(u64)] pub struct Pde
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

	impl Pde
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

	#[bitfield(u64)] pub struct LargePde
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

	impl LargePde
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
	#[bitfield(u64)] pub struct Pte
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

	impl Pte
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

	#[bitfield(u32)] pub struct PageFaultErrorCode
	{
		pub present:bool,
		pub write:bool,
		pub user:bool,
		pub reserved:bool,
		pub execute:bool,
		pub protection_key:bool,
		pub shadow_stack:bool,
		#[bits(25)] rsvd:u32
	}

	impl PageFaultErrorCode
	{
		fn construct(p:bool,w:bool,u:bool,r:bool,x:bool,pk:bool,ss:bool)->Self
		{
			let mut v=Self::from_bits(0);
			v.set_present(p);
			v.set_write(w);
			v.set_user(u);
			v.set_reserved(r);
			v.set_execute(x);
			v.set_protection_key(pk);
			v.set_shadow_stack(ss);
			v
		}
	}

	impl Display for PageFaultErrorCode
	{
		fn fmt(&self, f: &mut Formatter) -> fmt::Result
		{
			write!(f,"Code={:X}. ",self.0)?;
			// Exhaust all bit definitions.
			write!(f,"Page is {}",if self.present() {"present"} else {"absent"})?;
			write!(f,", access is {}",if self.write() {"write"} else {"not write"})?;
			write!(f,", {}",if self.user() {"user"} else {"supervisor"})?;
			write!(f,", {} instruction fetch",if self.execute() {"is"} else {"is not"})?;
			write!(f,", {} shadow stack",if self.shadow_stack() {"is"} else {"is not"})?;
			write!(f,", reserved bits {} set",if self.reserved() {"are"} else {"are not"})?;
			Ok(())
		}
	}

	/// ## `PageTranslator` trait
	/// This trait is intended to help translating the virtual addresses to physical addresses on vCPU.
	/// Implement this trait on vCPU objects.
	pub trait PageTranslationHelper
	{
		fn read_virt(&mut self,va:u64,buffer:&mut [u8],fault_va:&mut Option<u64>)->Result<(),PageFaultErrorCode> where Self:Sized
		{
			read_virtual_address(va,self,buffer,fault_va)
		}

		fn write_virt(&mut self,va:u64,buffer:&[u8],fault_va:&mut Option<u64>)->Result<(),PageFaultErrorCode> where Self:Sized
		{
			write_virtual_address(va,self,buffer,fault_va)
		}

		fn get_cr0(&self)->u64;
		fn get_cr3(&self)->u64;
		fn get_cr4(&self)->u64;
		fn get_efer(&self)->u64;
		fn is_user_mode(&self)->bool;

		fn read_phys_mem(&self,pa:u64,buffer:&mut [u8])->usize;
		fn write_phys_mem(&self,pa:u64,buffer:&[u8])->usize;
	}

	/// ## `translate_64_bit_va_routine`
	/// This routine is recursive!
	#[allow(clippy::too_many_arguments)]
	fn translate_64bit_va_routine(va:u64,vcpu:&mut impl PageTranslationHelper,pt_base:u64,level:u64,w:bool,x:bool,ss:bool)->Result<u64,PageFaultErrorCode>
	{
		let u=vcpu.is_user_mode();
		// Calculate the address of entry.
		let shift_amount:u64=(level-1)*(PAGE_SHIFT_DIFF64 as u64)+PAGE_SHIFT as u64;
		let pt_index:u64=(va>>shift_amount)&(PAGE_TABLE_ENTRIES64 as u64 - 1);
		let pml_pa:u64=pt_base+(pt_index<<3);
		// Fetch current level entry.
		let mut pml_raw:[u8;8]=[0;8];
		let rsize=vcpu.read_phys_mem(pml_pa,&mut pml_raw);
		assert_eq!(pml_raw.len(),rsize);
		let pml_e:u64=u64::from_le_bytes(pml_raw);
		let pml_p=(pml_e&X86_PAGING_PRESENT)==X86_PAGING_PRESENT;
		let pml_w=(pml_e&X86_PAGING_WRITE)==X86_PAGING_WRITE;
		let pml_u=(pml_e&X86_PAGING_USER)==X86_PAGING_USER;
		let pml_nx=(pml_e&X86_PAGING_NX)==X86_PAGING_NX;
		let pml_ps=(pml_e&X86_PAGING_PAGE_SIZE)==X86_PAGING_PAGE_SIZE;
		// Set accessed & dirty bits.
		let new_pml_d:u64=pml_e|X86_PAGING_ACCESSED|if w {X86_PAGING_DIRTY} else {0};
		vcpu.write_phys_mem(pml_pa,&new_pml_d.to_le_bytes());
		// Check access rights.
		if !pml_p
		{
			return Err(PageFaultErrorCode::construct(false,w,u,false,x,false,false));
		}
		if !pml_w && w
		{
			return Err(PageFaultErrorCode::construct(pml_p,w,u,false,x,false,false));
		}
		if !pml_u && u
		{
			return Err(PageFaultErrorCode::construct(pml_p,w,u,false,x,false,false));
		}
		if pml_nx && !x
		{
			return Err(PageFaultErrorCode::construct(pml_p,w,u,false,x,false,false));
		}
		if pml_ps || level==1
		{
			// This is the last level!
			// Check Shadow-Stack: R/W bit is cleared while D bit is set means a shadow-stack page.
			let pml_d=(pml_e&X86_PAGING_DIRTY)==X86_PAGING_DIRTY;
			let pml_ss=pml_d&!pml_w;
			if pml_ss && !ss
			{
				Err(PageFaultErrorCode::construct(pml_p,w,u,false,x,false,true))
			}
			else
			{
				let offset_mask:u64=(1<<shift_amount)-1;
				let base:u64=(pml_e>>shift_amount)<<shift_amount;
				let pa=(va&offset_mask)|phys_addr_mask(base);
				Ok(pa)
			}
		}
		else
		{
			// This is the intermediate level!
			let next_pt_base=phys_page_4kb_base(pml_e);
			translate_64bit_va_routine(va,vcpu,next_pt_base,level-1,w,x,ss)
		}
	}

	pub fn translate_virtual_address(va:u64,vcpu:&mut impl PageTranslationHelper,w:bool,x:bool,ss:bool)->Result<u64,PageFaultErrorCode>
	{
		let cr0=vcpu.get_cr0();
		if (cr0&CR0_PG)==CR0_PG
		{
			let cr3=vcpu.get_cr3();
			// Paging is enabled! Determine which mode we are working with!
			let cr4=vcpu.get_cr4();
			if (cr4&CR4_PAE)==CR4_PAE
			{
				// Physical-Address Extension is enabled!
				let mut va:u64=va;
				let efer=vcpu.get_efer();
				if (efer&MSR_EFER_LMA)==MSR_EFER_LMA
				{
					// Long-Mode is activated. Virtual-Address is 64-bit!
					let level=if (cr4&CR4_LA57)==CR4_LA57
					{
						// 5-level 57-bit Linear-Address.
						5
					}
					else
					{
						// 4-level 48-bit Linear-Address.
						4
					};
					translate_64bit_va_routine(va,vcpu,cr3,level,w,x,ss)
				}
				else
				{
					// 3-level 32-bit PAE paging.
					va&=u32::MAX as u64;
					let pdpe_index=va>>PAGE_1GB_SHIFT;
					let mut pdpe_buff:[u8;8]=[0;8];
					vcpu.read_phys_mem(vcpu.get_cr3()+(pdpe_index<<3),&mut pdpe_buff);
					let pdpe_pa=u64::from_le_bytes(pdpe_buff);
					// PDPE only has a present bit, no W/NX bits.
					if (pdpe_pa&X86_PAGING_PRESENT)==0
					{
						Err(PageFaultErrorCode(0))
					}
					else
					{
						// Only 2 levels remaining.
						translate_64bit_va_routine(va,vcpu,page_4kb_base(pdpe_pa),2,w,x,ss)
					}
				}
			}
			else
			{
				// 2-level 32-bit legacy paging.
				unimplemented!("32-bit legacy paging is not supported!")
			}
		}
		else
		{
			// Paging is disabled! Just return the address.
			Ok(va)
		}
	}

	unsafe fn read_virtual_address_in_page(va:u64,vcpu:&mut impl PageTranslationHelper,buffer:*mut u8,copy_size:usize)->Result<(),PageFaultErrorCode>
	{
		let r=translate_virtual_address(va,vcpu,false,false,false);
		match r
		{
			Ok(pa)=>
			{
				let buff=unsafe{slice::from_raw_parts_mut(buffer,copy_size)};
				vcpu.read_phys_mem(pa,buff);
				Ok(())
			}
			Err(e)=>
			{
				Err(e)
			}
		}
	}

	unsafe fn write_virtual_address_in_page(va:u64,vcpu:&mut impl PageTranslationHelper,buffer:*const u8,copy_size:usize)->Result<(),PageFaultErrorCode>
	{
		let r=translate_virtual_address(va,vcpu,true,false,false);
		match r
		{
			Ok(pa)=>
			{
				let buff=unsafe{slice::from_raw_parts(buffer,copy_size)};
				vcpu.write_phys_mem(pa,buff);
				Ok(())
			}
			Err(e)=>
			{
				Err(e)
			}
		}
	}

	pub fn read_virtual_address(va:u64,vcpu:&mut impl PageTranslationHelper,buffer:&mut [u8],fault_va:&mut Option<u64>)->Result<(),PageFaultErrorCode>
	{
		let mut cur_va=va;
		let mut copied_size:u64=0;
		let end_va=va+buffer.len() as u64;
		while cur_va<end_va
		{
			let end_len=PAGE_SIZE as u64-page_offset(va);
			let rem_len=end_va-cur_va;
			let copy_size=if end_len<rem_len {end_len} else {rem_len};
			let r=unsafe
			{
				read_virtual_address_in_page(va+copied_size,vcpu,buffer.as_mut_ptr().add(copied_size as usize),copy_size as usize)
			};
			if r.is_err()
			{
				*fault_va=Some(cur_va);
				return r;
			}
			copied_size+=copy_size;
			cur_va+=copy_size;
		}
		*fault_va=None;
		Ok(())
	}

	pub fn write_virtual_address(va:u64,vcpu:&mut impl PageTranslationHelper,buffer:&[u8],fault_va:&mut Option<u64>)->Result<(),PageFaultErrorCode>
	{
		let mut cur_va=va;
		let mut copied_size:u64=0;
		let end_va=va+buffer.len() as u64;
		while cur_va<end_va
		{
			let end_len=PAGE_SIZE as u64-page_offset(va);
			let rem_len=end_va-cur_va;
			let copy_size=if end_len<rem_len {end_len} else {rem_len};
			let r=unsafe
			{
				write_virtual_address_in_page(va+copied_size,vcpu,buffer.as_ptr().add(copied_size as usize),copy_size as usize)
			};
			if r.is_err()
			{
				*fault_va=Some(cur_va);
				return r;
			}
			copied_size+=copy_size;
			cur_va+=copy_size;
		}
		*fault_va=None;
		Ok(())
	}
}

pub mod descriptors
{
	use bitfield_struct::bitfield;
	use core::fmt::{self,Display};
	use crate::*;
	use xpf_core::{hv_host::x86::AsmInterruptHandler, nvbdk::PAGE_SHIFT};

	pub const SELECTOR_RPLTI_MASK:u16=0xFFF8;

	// Descriptor Table register forbids any paddings.
	#[repr(C,packed)] pub struct DescriptorTable
	{
		pub limit:u16,
		pub base:u64
	}

	impl Display for DescriptorTable
	{
		fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
		{
			write!(f,"Limit: 0x{:04X}, Base: 0x{:016X}",{self.limit},{self.base})
		}
	}

	#[bitfield(u16)] pub struct SegmentFlags
	{
		#[bits(4)] pub segment_type:u16,
		pub system_segment:bool,
		#[bits(2)] pub dpl:u16,
		pub present:bool,
		#[bits(4)] pub limit_hi:u16,
		pub avl:bool,
		pub long_mode:bool,
		pub default_big:bool,
		pub granularity:bool
	}

	impl SegmentFlags
	{
		pub const AVAILABLE_TSS_16BIT:u16=0x1;
		pub const LDT:u16=0x2;
		pub const BUSY_TSS_16BIT:u16=0x3;
		pub const CALL_GATE_16BIT:u16=0x4;
		pub const TASK_GATE:u16=0x5;
		pub const INTERRUPT_GATE_16BIT:u16=0x6;
		pub const TRAP_GATE_16BIT:u16=0x7;
		pub const AVAILABLE_TSS:u16=0x9;
		pub const BUSY_TSS:u16=0xB;
		pub const CALL_GATE:u16=0xC;
		pub const INTERRUPT_GATE:u16=0xE;
		pub const TRAP_GATE:u16=0xF;
	}

	#[repr(C,packed)] pub struct UserSegmentDescriptor
	{
		pub limit_lo:u16,
		pub base_lo:u16,
		pub base_mid:u8,
		pub flags:u16,
		pub base_hi:u8
	}

	#[repr(C,packed)] pub struct SystemSegmentDescriptor
	{
		pub limit_lo:u16,
		pub base_lo:u16,
		pub base_mid1:u8,
		pub flags:u16,
		pub base_mid2:u8,
		pub base_hi:u32,
		pub reserved:u32
	}

	impl SystemSegmentDescriptor
	{
		pub fn new(limit:u32,base:u64,descriptor_type:u16,dpl:u8,present:bool)->Self
		{
			let limit_lo=(if limit<=0xFFFFF {limit&0xFFFF} else {(limit>>PAGE_SHIFT)&0xFFFF}) as u16;
			let limit_hi=(if limit<=0xFFFFF {limit>>16} else {limit>>28}) as u16;
			let granularity=limit>0xFFFFF;
			Self
			{
				limit_lo,
				base_lo:(base&0xFFFF) as u16,
				base_mid1:((base>>16)&0xFF) as u8,
				flags:(descriptor_type|((dpl as u16)<<5)|((present as u16)<<7)|(limit_hi<<8)|((granularity as u16)<<15)),
				base_mid2:((base>>24)&0xFF) as u8,
				base_hi:(base>>32) as u32,
				reserved:0
			}
		}
	}

	#[bitfield(u16)] pub struct GateFlags
	{
		#[bits(3)] pub ist:u16,
		#[bits(5)] rsvd0:u16,
		#[bits(4)] pub gate_type:u16,
		pub rsvd1:bool,
		#[bits(2)] pub dpl:u16,
		pub present:bool
	}

	#[derive(Default,Clone,Copy)]
	#[repr(C,packed)] pub struct GateDescriptor
	{
		pub offset_lo:u16,
		pub selector:u16,
		pub flags:GateFlags,
		pub offset_mid:u16,
		pub offset_hi:u32,
		pub reserved:u32
	}

	impl GateDescriptor
	{
		pub fn new_intgate(target_handler:AsmInterruptHandler,selector:u16,dpl:u16,ist:u16)->Option<Self>
		{
			if dpl>3
			{
				return None;
			}
			if ist>7
			{
				return None;
			}
			let offset_lo=(target_handler as usize & 0xFFFF) as u16;
			let offset_mid=((target_handler as usize >> 16) & 0xFFFF) as u16;
			let offset_hi=(target_handler as usize >> 32) as u32;
			// let flags=ist|(GATE_DESCRIPTOR_INTERRUPT_GATE<<GATE_DESCRIPTOR_TYPE_BIT)|(dpl<<GATE_DESCRIPTOR_DPL_BIT)|(1<<GATE_DESCRIPTOR_PRESENT_BIT);
			let mut flags=GateFlags(0);
			flags.set_present(true);
			flags.set_gate_type(SegmentFlags::INTERRUPT_GATE);
			flags.set_dpl(dpl);
			flags.set_ist(ist);
			Some
			(
				Self
				{
					offset_lo,
					offset_mid,
					selector,
					flags,
					offset_hi,
					reserved:0
				}
			)
		}
	}

	#[derive(Default)]
	#[repr(C,packed)] pub struct TaskSegmentState64
	{
		reserved0:u32,
		pub rsp0:u64,
		pub rsp1:u64,
		pub rsp2:u64,
		reserved1:u64,
		pub ist1:u64,
		pub ist2:u64,
		pub ist3:u64,
		pub ist4:u64,
		pub ist5:u64,
		pub ist6:u64,
		pub ist7:u64,
		reserved2:u64,
		reserved3:u16,
		iomap_base:u16
	}
}

pub mod crdr
{
	use bitfield_struct::bitfield;
	use paste::paste;

	#[macro_export] macro_rules! define_bit
	{
		($name:tt,$pos:literal) =>
		{
			paste!
			{
				pub const [<$name:upper _BIT>]:u64=$pos;
				pub const [<$name:upper>]:u64=1<<$pos;
			}
		};
	}

	define_bit!(CR0_PE,0);
	define_bit!(CR0_MP,1);
	define_bit!(CR0_EM,2);
	define_bit!(CR0_TS,3);
	define_bit!(CR0_ET,4);
	define_bit!(CR0_NE,5);
	define_bit!(CR0_WP,16);
	define_bit!(CR0_AM,18);
	define_bit!(CR0_NW,29);
	define_bit!(CR0_CD,30);
	define_bit!(CR0_PG,31);

	define_bit!(CR4_VME,0);
	define_bit!(CR4_PVI,1);
	define_bit!(CR4_TSD,2);
	define_bit!(CR4_DE,3);
	define_bit!(CR4_PSE,4);
	define_bit!(CR4_PAE,5);
	define_bit!(CR4_MCE,6);
	define_bit!(CR4_PGE,7);
	define_bit!(CR4_PCE,8);
	define_bit!(CR4_OSFXSR,9);
	define_bit!(CR4_OSXMMEXCEPT,10);
	define_bit!(CR4_UMIP,11);
	define_bit!(CR4_LA57,12);
	define_bit!(CR4_VMXE,13);
	define_bit!(CR4_SMXE,14);
	define_bit!(CR4_FSGSBASE,16);
	define_bit!(CR4_PCIDE,17);
	define_bit!(CR4_OSXSAVE,18);
	define_bit!(CR4_SMEP,20);
	define_bit!(CR4_SMAP,21);
	define_bit!(CR4_PKE,22);
	define_bit!(CR4_CET,23);
	define_bit!(CR4_PKS,24);

	define_bit!(DR6_B0,0);
	define_bit!(DR6_B1,1);
	define_bit!(DR6_B2,2);
	define_bit!(DR6_B3,3);
	define_bit!(DR6_BUSLOCK_DETECTED,11);
	define_bit!(DR6_BD,13);
	define_bit!(DR6_BS,14);
	define_bit!(DR6_BT,15);

	#[bitfield(u64)] pub struct Dr7
	{
		pub l0:bool,
		pub g0:bool,
		pub l1:bool,
		pub g1:bool,
		pub l2:bool,
		pub g2:bool,
		pub l3:bool,
		pub g3:bool,
		pub le:bool,
		pub ge:bool,
		#[bits(3)] rsvd0:u64,
		pub gd:bool,
		#[bits(2)] rsvd1:u64,
		#[bits(2)] pub rw0:u64,
		#[bits(2)] pub len0:u64,
		#[bits(2)] pub rw1:u64,
		#[bits(2)] pub len1:u64,
		#[bits(2)] pub rw2:u64,
		#[bits(2)] pub len2:u64,
		#[bits(2)] pub rw3:u64,
		#[bits(2)] pub len3:u64,
		rsvd2:u32
	}

	impl Dr7
	{
		pub const LENGTH_CONVERTER:[u64;4]=[1,2,8,4];
		pub const INSTRUCTION_EXECUTION:u64=0;
		pub const DATA_WRITE:u64=1;
		pub const IO_BREAK:u64=2;
		pub const DATA_READWRITE:u64=3;
	}
}

pub mod cpuid
{
    use core::{arch::x86_64::{CpuidResult, __cpuid, __cpuid_count}, mem::MaybeUninit, slice};

	use bitfield_struct::bitfield;
	use log::*;

	// Standard Leaf
	pub const CPUID_STD_MAX_NUMBER_VENDOR_STRING:u32=0x0;
	pub const CPUID_STD_PROCESSOR_FEATURE:u32=0x1;
	pub const CPUID_STD_MONITOR_FEATURE:u32=0x5;
	pub const CPUID_STD_THERMAL_FEATURE:u32=0x6;
	pub const CPUID_STD_STRUCTURED_EXTENDED_FEATURE_ID:u32=0x7;
	pub const CPUID_STD_EXTENDED_TOPOLOGY_INFORMATION:u32=0xB;
	pub const CPUID_STD_PROCESOR_EXTENDED_STATE_ENUMERATION:u32=0xD;
	// Extended Leaf
	pub const CPUID_EXT_MAX_NUMBER_VENDOR_STRING:u32=0x80000000;
	pub const CPUID_EXT_PROCESSOR_FEATURE:u32=0x80000001;
	pub const CPUID_EXT_BRAND_STRING_P1:u32=0x80000002;
	pub const CPUID_EXT_BRAND_STRING_P2:u32=0x80000003;
	pub const CPUID_EXT_BRAND_STRING_P3:u32=0x80000004;
	pub const CPUID_EXT_L1_CACHE_TLBS:u32=0x80000005;
	pub const CPUID_EXT_L2_L3_CACHE_TLBS:u32=0x80000006;
	pub const CPUID_EXT_POWER_MANAGEMENT_RAS_CAPABILITY:u32=0x80000007;
	pub const CPUID_EXT_PROCESSOR_CAPABILITY_PARAMETERS_EXTENDED_ID:u32=0x80000008;
	/// Use this flag for CPUID[EAX=0x00000001].ECX
	pub const CPUID_AVX:u32=1<<28;
	/// Use this flag for CPUID[EAX=0x00000001].ECX
	pub const CPUID_UNDER_HYPERVISOR:u32=0x80000000;
	/// Use this flag for CPUID[EAX=0x00000001].EDX
	pub const CPUID_SSE:u32=1<<25;
	/// Use this flag for CPUID[EAX=0x80000001].EDX
	pub const CPUID_1GB_PAGE:u32=0x4000000;


	/// The `CpuidLeaf` trait abstracts a certain `cpuid` leaf of the CPU. \
	/// You must implement `LEAF_INDEX`, `SUBLEAF_INDEX` and `init` method.
	/// 
	/// The `cpuid` method is provided by this trait so that you may obtain info from the current processor. \
	/// You must not implement the `cpuid` method on your own!
	/// 
	/// You may derive required constants and method with `derive_cpuid_trait!` macro,
	/// albeit there're certain requirements to derive implementation of this trait.
	pub trait CpuidLeaf
	{
		const LEAF_INDEX:u32;
		const SUBLEAF_INDEX:Option<u32>;

		/// The `init` method is a required method to initialize the leaf. \
		/// This method will be called only once per `cpuid` method. \
		/// All fields in `self` before the `init` method is called are undefined. \
		/// You must ensure no fields of `self` are still undefined after `init`.
		fn init(&mut self,result:&CpuidResult);
		/// The `as_result` will convert the leaf back to the four raw registers.
		fn as_result(&self)->CpuidResult;

		/// The `cpuid` method queries the processor info with the given `LEAF_INDEX` and `SUBLEAF_INDEX`. \
		/// Do not implement this method. You must always use the provided method!
		fn cpuid()->Self where Self:Sized
		{
			let r:CpuidResult=unsafe
			{
				match Self::SUBLEAF_INDEX
				{
					Some(subleaf)=>__cpuid_count(Self::LEAF_INDEX,subleaf),
					None=>__cpuid(Self::LEAF_INDEX)
				}
			};
			let mut s:MaybeUninit<Self>=MaybeUninit::uninit();
			unsafe
			{
				s.assume_init_mut().init(&r);
				s.assume_init()
			}
		}
	}

	/// The `CoalescedCpuidLeaf` trait abstracts information that must be queried via multiple leaves. \
	/// You must implement `LEAF_INDICES` and `init` method. `pre_init` and `post_init` are not required.
	pub trait CoalescedCpuidLeaf<const N:usize>
	{
		const LEAF_INDICES:[(u32,Option<u32>);N];

		/// The `init` method is a required method to intiailize the leaf. \
		/// This method will be called for `N` times per `cpuid` method. \
		/// All fields in `self` before `init` method is called are initialized
		/// to the extent of how `pre_init` method is implemented.
		fn init(&mut self,index:usize,result:&CpuidResult);
		/// The `pre_init` method is an optioal method to initialize the leaf. \
		/// The method will be called only once per `cpuid` method and it's called before any `init`. \
		/// All fields in `self` before `init` method is called are undefined. \
		/// You must ensure fields are initialized to the extent that `init` can run properly.
		fn pre_init(&mut self) {}
		/// The `post_init` method is an optional method to initialize the leaf. \
		/// The method will be called only once per `cpuid` method and it's called after any `init`. \
		/// You must ensure no fields in `self` are undefined after `post_init`!
		fn post_init(&mut self) {}
		/// The `as_results` method will convert the leaves back to raw registers.
		fn as_results(&self)->[CpuidResult;N];

		/// The `cpuid` method queries the processor info with the given `LEAF_INDICES`. \
		/// Do not implement this method. You must always use the provided method!
		fn cpuid()->Self where Self:Sized
		{
			let mut s:MaybeUninit<Self>=MaybeUninit::uninit();
			unsafe
			{
				s.assume_init_mut().pre_init();
			}
			for (i,(a,c)) in Self::LEAF_INDICES.iter().enumerate()
			{
				unsafe
				{
					let r:CpuidResult=match c
					{
						Some(c)=>__cpuid_count(*a,*c),
						None=>__cpuid(*a)
					};
					s.assume_init_mut().init(i,&r);
				}
			}
			unsafe
			{
				s.assume_init_mut().post_init();
				s.assume_init()
			}
		}
	}

	/// To derive `CpuidLeaf` trait with `derive_cpuid_trait` macro, your structure must
	/// have `a`, `b` `c`, `d` being defined as `bitfield_struct(u32)`. \
	/// You must also import `core::{slice,arch::x86_64::CpuidResult}` to use this macro!
	#[macro_export] macro_rules! derive_cpuid_trait
	{
		($type:ty,$leaf:expr,$subleaf:expr)=>
		{
			impl CpuidLeaf for $type
			{
				const LEAF_INDEX:u32=$leaf;
				const SUBLEAF_INDEX:Option<u32>=$subleaf;

				fn init(&mut self,result:&CpuidResult)
				{
					let s:&mut [u32]=unsafe{slice::from_raw_parts_mut((&raw mut self.0).cast(),4)};
					s[0]=result.eax;
					s[1]=result.ebx;
					s[2]=result.ecx;
					s[3]=result.edx;
				}

				fn as_result(&self)->CpuidResult
				{
					let s:&[u32]=unsafe{slice::from_raw_parts((&raw const self.0).cast(),4)};
					CpuidResult
					{
						eax:s[0],
						ebx:s[1],
						ecx:s[2],
						edx:s[3]
					}
				}
			}
		};
	}

	pub struct MaxStandardLeafAndVendorString
	{
		pub max_leaf:u32,
		vendor_string:[u8;12]
	}

	impl MaxStandardLeafAndVendorString
	{
		pub fn vendor_name(&self)->&str
		{
			unsafe
			{
				str::from_utf8_unchecked(&self.vendor_string)
			}
		}
	}

	impl CpuidLeaf for MaxStandardLeafAndVendorString
	{
		const LEAF_INDEX:u32=0x00000000;
		const SUBLEAF_INDEX:Option<u32>=None;
		
		fn init(&mut self,result:&CpuidResult)
		{
			self.max_leaf=result.eax;
			self.vendor_string[0..0x4].copy_from_slice(&result.ebx.to_le_bytes());
			self.vendor_string[4..0x8].copy_from_slice(&result.edx.to_le_bytes());
			self.vendor_string[8..0xC].copy_from_slice(&result.ecx.to_le_bytes());
		}

		fn as_result(&self)->CpuidResult
		{
			let s:&[u32]=unsafe{slice::from_raw_parts(self.vendor_string.as_ptr().cast(),3)};
			CpuidResult
			{
				eax:self.max_leaf,
				ebx:s[0],
				ecx:s[1],
				edx:s[2]
			}
		}
	}

	#[bitfield(u128)] pub struct StandardProcessorFeatureIdentifiers
	{
		#[bits(4)] pub stepping:u32,
		#[bits(4)] pub base_model:u32,
		#[bits(4)] pub base_family:u32,
		#[bits(4)] rsvd0:u32,
		#[bits(4)] pub ext_model:u32,
		pub ext_family:u8,
		#[bits(4)] rsvd1:u32,
		pub brand_id:u8,
		pub clflush_size:u8,
		pub logical_cpu_count:u8,
		pub local_apic_id:u8,
		pub sse3:bool,
		pub pclmulqdq:bool,
		pub dtes64:bool,
		pub monitor:bool,
		pub ds_cpl:bool,
		pub vmx:bool,
		pub smx:bool,
		pub eist:bool,
		pub tm2:bool,
		pub ssse3:bool,
		pub l1_ctxt_id:bool,
		pub sdbg:bool,
		pub fma:bool,
		pub cmpxchg16b:bool,
		pub xtpr_update_ctrl:bool,
		pub pdcm:bool,
		rsvd2:bool,
		pub pcid:bool,
		pub dca:bool,
		pub sse41:bool,
		pub sse42:bool,
		pub x2apic:bool,
		pub movbe:bool,
		pub popcnt:bool,
		pub tsc_deadline:bool,
		pub aes:bool,
		pub xsave:bool,
		pub osxsave:bool,
		pub avx:bool,
		pub f16c:bool,
		pub rdrand:bool,
		pub hypervisor:bool,
		pub fpu:bool,
		pub vme:bool,
		pub de:bool,
		pub pse:bool,
		pub tsc:bool,
		pub msr:bool,
		pub pae:bool,
		pub mce:bool,
		pub cmpxchg8b:bool,
		pub apic:bool,
		rsvd3:bool,
		pub sysenter_sysexit:bool,
		pub mtrr:bool,
		pub pge:bool,
		pub mca:bool,
		pub cmov:bool,
		pub pat:bool,
		pub pse36:bool,
		rsvd6:bool,
		pub clflush:bool,
		#[bits(3)] rsvd4:u32,
		pub mmx:bool,
		pub fxsr:bool,
		pub sse:bool,
		pub sse2:bool,
		rsvd8:bool,
		pub htt:bool,
		#[bits(3)] rsvd5:u32
	}

	derive_cpuid_trait!(StandardProcessorFeatureIdentifiers,1,None);

	#[bitfield(u128)] pub struct MonitorMwaitIdentifiers
	{
		pub smallest_monitor_line_size:u16,
		rsvd0:u16,
		pub largest_monitor_line_size:u16,
		rsvd1:u16,
		pub emx:bool,
		pub ibe:bool,
		#[bits(62)] rsvd2:u128
	}

	derive_cpuid_trait!(MonitorMwaitIdentifiers,5,None);

	#[bitfield(u128)] pub struct PowerManagementFeatures
	{
		#[bits(2)] rsvd0:u128,
		pub arat:bool,
		#[bits(61)] rsvd1:u128,
		pub eff_freq:bool,
		#[bits(63)] rsvd2:u128,
	}

	derive_cpuid_trait!(PowerManagementFeatures,6,None);

	#[bitfield(u128)] pub struct StructuredExtendedFeatures0
	{
		pub max_sub_fn:u32,
		pub fsgsbase:bool,
		pub tsc_adjust:bool,
		rsvd0:bool,
		pub bmi1:bool,
		rsvd1:bool,
		pub avx2:bool,
		rsvd2:bool,
		pub smep:bool,
		pub bmi2:bool,
		pub erms:bool,
		pub invpcid:bool,
		rsvd3:bool,
		pub pqm:bool,
		#[bits(2)] rsvd4:u32,
		pub pqe:bool,
		pub avx512f:bool,
		pub avx512dq:bool,
		pub rdseed:bool,
		pub adx:bool,
		pub smap:bool,
		pub avx512_ifma:bool,
		rsvd5:bool,
		pub clflushopt:bool,
		pub clwb:bool,
		#[bits(3)] rsvd6:u32,
		pub avx512cd:bool,
		pub sha:bool,
		pub avx512bw:bool,
		pub avx512vl:bool,
		rsvd7:bool,
		pub avx512_vbmi:bool,
		pub umip:bool,
		pub pku:bool,
		pub ospke:bool,
		rsvd8:bool,
		pub avx512_vbmi2:bool,
		pub cet_ss:bool,
		pub gfni:bool,
		pub vaes:bool,
		pub vpcmulqdq:bool,
		pub avx512_vnni:bool,
		pub avx512_bitalg:bool,
		rsvd9:bool,
		pub avx512_vpopcntdq:bool,
		rsvd10:bool,
		pub la57:bool,
		#[bits(5)] rsvd11:u32,
		pub rdpid:bool,
		rsvd12:bool,
		pub buslock_trap:bool,
		#[bits(2)] rsvd13:u32,
		pub movdiri:bool,
		pub movdir64b:bool,
		#[bits(3)] rsvd14:u32,
		rsvd15:u32
	}

	derive_cpuid_trait!(StructuredExtendedFeatures0,7,Some(0));
	
	#[bitfield(u128)] pub struct StructuredExtendedFeatures1
	{
		#[bits(4)] rsvd0:u32,
		pub avx_vnni:bool,
		pub avx512_bf16:bool,
		#[bits(122)] rsvd1:u128
	}

	derive_cpuid_trait!(StructuredExtendedFeatures1,7,Some(1));

	#[bitfield(u32)] pub struct ExtendedTopologyEnumerationEax
	{
		#[bits(5)] pub mask_width:u32,
		#[bits(27)] rsvd:u32
	}

	#[bitfield(u32)] pub struct ExtendedTopologyEnumerationEbx
	{
		pub logical_units:u16,
		rsvd:u16
	}

	#[bitfield(u32)] pub struct ExtendedTopologyEnumerationEcx
	{
		pub input_ecx:u8,
		pub hierarchy_level:u8,
		rsvd:u16
	}
	
	#[bitfield(u32)] pub struct ExtendedTopologyEnumerationEdx
	{
		pub x2apic_id:u32
	}

	pub struct ExtendedTopologyEnumeration<const N:u32>
	{
		pub a:ExtendedTopologyEnumerationEax,
		pub b:ExtendedTopologyEnumerationEbx,
		pub c:ExtendedTopologyEnumerationEcx,
		pub d:ExtendedTopologyEnumerationEdx
	}

	impl<const N:u32> CpuidLeaf for ExtendedTopologyEnumeration<N>
	{
		const LEAF_INDEX:u32=0xB;
		const SUBLEAF_INDEX:Option<u32>=Some(N);

		fn init(&mut self,result:&CpuidResult)
		{
			self.a.0=result.eax;
			self.b.0=result.ebx;
			self.c.0=result.ecx;
			self.d.0=result.edx;
		}

		fn as_result(&self)->CpuidResult
		{
			CpuidResult
			{
				eax:self.a.0,
				ebx:self.b.0,
				ecx:self.c.0,
				edx:self.d.0
			}
		}
	}

	#[bitfield(u128)] pub struct ExtendedStateEnumeration0
	{
		pub mask:u64,
		pub size:u64
	}

	derive_cpuid_trait!(ExtendedStateEnumeration0,0xD,Some(0));

	#[bitfield(u128)] pub struct ExtendedStateEnumeration1
	{
		pub xsaveopt:bool,
		pub xsavec:bool,
		pub xgetbv:bool,
		pub xsaves:bool,
		#[bits(28)] rsvd0:u32,
		pub fixed_xstate_size:u32,
		#[bits(11)] rsvd1:u32,
		pub cet_u:bool,
		pub cet_s:bool,
		#[bits(19)] rsvd2:u32,
		pub rsvd3:u32
	}

	derive_cpuid_trait!(ExtendedStateEnumeration1,0xD,Some(1));

	pub struct ExtendedStateEnumerationN<const N:u32>
	{
		pub size:u32,
		pub offset:u32,
		pub rsvd_c:u32,
		pub rsvd_d:u32
	}

	impl<const N:u32> CpuidLeaf for ExtendedStateEnumerationN<N>
	{
		const LEAF_INDEX:u32=0xD;
		const SUBLEAF_INDEX:Option<u32>=Some(N);
		
		fn init(&mut self,result:&CpuidResult)
		{
			assert!(N>=0x2 && N<=0x3E);
			self.size=result.eax;
			self.offset=result.ebx;
			self.rsvd_c=result.ecx;
			self.rsvd_d=result.edx;
		}

		fn as_result(&self)->CpuidResult
		{
			CpuidResult
			{
				eax:self.size,
				ebx:self.offset,
				ecx:self.rsvd_c,
				edx:self.rsvd_d
			}
		}
	}

	pub struct MaxExtendedLeafAndVendorString
	{
		pub max_leaf:u32,
		vendor_string:[u8;12]
	}

	impl MaxExtendedLeafAndVendorString
	{
		pub fn vendor_name(&self)->&str
		{
			unsafe
			{
				str::from_utf8_unchecked(&self.vendor_string)
			}
		}
	}

	impl CpuidLeaf for MaxExtendedLeafAndVendorString
	{
		const LEAF_INDEX:u32=0x80000000;
		const SUBLEAF_INDEX:Option<u32>=None;
		
		fn init(&mut self,result:&CpuidResult)
		{
			self.max_leaf=result.eax;
			self.vendor_string[0..0x4].copy_from_slice(&result.ebx.to_le_bytes());
			self.vendor_string[4..0x8].copy_from_slice(&result.edx.to_le_bytes());
			self.vendor_string[8..0xC].copy_from_slice(&result.ecx.to_le_bytes());
		}

		fn as_result(&self)->CpuidResult
		{
			let s:&[u32]=unsafe{slice::from_raw_parts(self.vendor_string.as_ptr().cast(),3)};
			CpuidResult
			{
				eax:self.max_leaf,
				ebx:s[0],
				ecx:s[1],
				edx:s[2]
			}
		}
	}

	#[bitfield(u128)] pub struct ExtendedFeatureIdentifier
	{
		#[bits(4)] pub stepping:u32,
		#[bits(4)] pub base_model:u32,
		#[bits(4)] pub base_family:u32,
		#[bits(4)] rsvd0:u32,
		#[bits(4)] pub ext_model:u32,
		pub ext_family:u8,
		#[bits(4)] rsvd1:u32,
		pub brand_id:u16,
		#[bits(12)] rsvd2:u32,
		#[bits(4)] pub pkg_type:u32,
		pub lahf_sahf:bool,
		pub cmp_legacy:bool,
		pub svm:bool,
		pub ext_apic_space:bool,
		pub alt_mov_cr8:bool,
		pub abm:bool,
		pub sse4a:bool,
		pub misalign_sse:bool,
		pub prefetch_3dnow:bool,
		pub osvw:bool,
		pub ibs:bool,
		pub xop:bool,
		pub skinit:bool,
		pub wdt:bool,
		rsvd3:bool,
		pub lwp:bool,
		pub fma4:bool,
		pub tce:bool,
		#[bits(3)] rsvd4:u32,
		pub tbm:bool,
		pub topology_extensions:bool,
		pub perf_ctrl_ext_core:bool,
		pub perf_crtl_ext_nb:bool,
		rsvd5:bool,
		pub data_bkpt_ext:bool,
		pub perf_tsc:bool,
		pub perf_ctrl_ext_llc:bool,
		pub monitorx:bool,
		pub addr_mask_ext:bool,
		rsvd6:bool,
		pub fpu:bool,
		pub vme:bool,
		pub de:bool,
		pub pse:bool,
		pub tsc:bool,
		pub msr:bool,
		pub pae:bool,
		pub mce:bool,
		pub cmpxchg8b:bool,
		pub apic:bool,
		rsvd7:bool,
		pub syscall_sysret:bool,
		pub mtrr:bool,
		pub pge:bool,
		pub mca:bool,
		pub cmov:bool,
		pub pat:bool,
		pub pse36:bool,
		#[bits(2)] rsvd8:u32,
		pub nx:bool,
		rsvd9:bool,
		pub mmx_ext:bool,
		pub mmx:bool,
		pub fxsr:bool,
		pub fast_fxsr:bool,
		pub page_1gb:bool,
		pub rdtscp:bool,
		rsvd10:bool,
		pub long_mode:bool,
		pub amd_3dnow_ext:bool,
		pub amd_3dnow:bool
	}

	derive_cpuid_trait!(ExtendedFeatureIdentifier,0x80000001,None);

	pub struct ProcessorBrandString
	{
		len:usize,
		pub buffer:[u8;0x30]
	}

	impl ProcessorBrandString
	{
		pub fn brand_string(&self)->&str
		{
			unsafe
			{
				str::from_utf8_unchecked(&self.buffer[..self.len])
			}
		}
	}

	impl CoalescedCpuidLeaf<3> for ProcessorBrandString
	{
		const LEAF_INDICES:[(u32,Option<u32>);3]=[(0x80000002,None),(0x80000003,None),(0x80000004,None)];

		fn init(&mut self,index:usize,result:&CpuidResult)
		{
			let s:&mut [u32]=unsafe{slice::from_raw_parts_mut(self.buffer.as_mut_ptr().cast(),12)};
			s[index<<2]=result.eax;
			s[(index<<2)+1]=result.ebx;
			s[(index<<2)+2]=result.ecx;
			s[(index<<2)+3]=result.edx;
		}

		fn post_init(&mut self)
		{
			self.len=match self.buffer.iter().position(|&x| x==0)
			{
				Some(l)=>l,
				None=>self.buffer.len()
			};
		}

		fn as_results(&self)->[CpuidResult;3]
		{
			let s:&[u32]=unsafe{slice::from_raw_parts(self.buffer.as_ptr().cast(),12)};
			[
				CpuidResult{eax:s[0x0],ebx:s[0x1],ecx:s[0x2],edx:s[0x3]},
				CpuidResult{eax:s[0x5],ebx:s[0x6],ecx:s[0x6],edx:s[0x7]},
				CpuidResult{eax:s[0x9],ebx:s[0xA],ecx:s[0xB],edx:s[0xB]}
			]
		}
	}

	#[bitfield(u32)] pub struct L1TlbInfo
	{
		pub i_entries:u8,
		pub i_associativity:u8,
		pub d_entries:u8,
		pub d_associativity:u8
	}

	#[bitfield(u32)] pub struct L1CacheInfo
	{
		pub line_size:u8,
		pub lines_per_tag:u8,
		pub associativity:u8,
		pub size_kb:u8,
	}

	#[bitfield(u32)] pub struct L2TlbInfo
	{
		#[bits(12)] pub i_entries:u32,
		#[bits(4)] pub i_associativity:u32,
		#[bits(12)] pub d_entries:u32,
		#[bits(4)] pub d_associativity:u32,
	}

	#[bitfield(u32)] pub struct L2CacheInfo
	{
		pub line_size:u8,
		#[bits(4)] lines_per_tag:u32,
		#[bits(4)] associativity:u32,
		pub size_kb:u16
	}

	#[bitfield(u32)] pub struct L3CacheInfo
	{
		pub line_size:u8,
		#[bits(4)] lines_per_tag:u32,
		#[bits(4)] associativity:u32,
		#[bits(2)] rsvd:u32,
		#[bits(14)] pub size_kb:u32
	}

	pub struct CacheAndTlbInformation
	{
		pub l1_large_tlb:L1TlbInfo,
		pub l1_small_tlb:L1TlbInfo,
		pub l1d_cache:L1CacheInfo,
		pub l1i_cache:L1CacheInfo,
		pub l2_large_tlb:L2TlbInfo,
		pub l2_small_tlb:L2TlbInfo,
		pub l2_cache:L2CacheInfo,
		pub l3_cache:L3CacheInfo
	}

	impl CoalescedCpuidLeaf<2> for CacheAndTlbInformation
	{
		const LEAF_INDICES:[(u32,Option<u32>);2]=[(0x80000005,None),(0x80000006,None)];

		fn init(&mut self,index:usize,result:&CpuidResult)
		{
			match index
			{
				0=>
				{
					self.l1_large_tlb.0=result.eax;
					self.l1_small_tlb.0=result.ebx;
					self.l1d_cache.0=result.ecx;
					self.l1i_cache.0=result.edx;
				}
				1=>
				{
					self.l2_large_tlb.0=result.eax;
					self.l2_small_tlb.0=result.ebx;
					self.l2_cache.0=result.ecx;
					self.l3_cache.0=result.edx;
				}
				_=>warn!("Unrecognized index 0x{index:X} while enumerating Cache and TLB Information in CPUID!")
			}
		}

		fn as_results(&self)->[CpuidResult;2]
		{
			[
				CpuidResult{eax:self.l1_large_tlb.0,ebx:self.l1_small_tlb.0,ecx:self.l1d_cache.0,edx:self.l1i_cache.0},
				CpuidResult{eax:self.l2_large_tlb.0,ebx:self.l2_small_tlb.0,ecx:self.l2_cache.0,edx:self.l3_cache.0}
			]
		}
	}

	#[bitfield(u128)] pub struct ProcessorCapabilityParameters
	{
		pub phys_addr_size:u8,
		pub virt_addr_size:u8,
		pub guest_phys_addr_size:u8,
		rsvd0:u8,
		pub clzero:bool,
		pub inst_retire_count_msr:bool,
		pub restore_fp_err_ptr:bool,
		pub invlpgb:bool,
		pub rdpru:bool,
		rsvd1:bool,
		pub be:bool,
		rsvd2:bool,
		pub mcommit:bool,
		pub wbnoinvd:bool,
		#[bits(2)] rsvd3:u128,
		pub ibpb:bool,
		pub int_wbinvd:bool,
		pub ibrs:bool,
		pub stibp:bool,
		pub ibrs_always_on:bool,
		pub stibp_always_on:bool,
		pub ibrs_preferred:bool,
		pub ibrs_same_mode:bool,
		pub efer_lmsle_unsupported:bool,
		pub invlpgb_nested:bool,
		#[bits(2)] rsvd4:u128,
		pub ssbd:bool,
		pub ssbd_virt_spec_ctrl:bool,
		pub ssbd_not_required:bool,
		pub cppc:bool,
		pub psfd:bool,
		pub btc_no:bool,
		pub ibpb_ret:bool,
		rsvd5:bool,
		pub phys_threads:u8,
		#[bits(4)] rsvd6:u128,
		#[bits(4)] pub apic_id_size:u8,
		#[bits(2)] pub perf_tsc_size:u32,
		#[bits(14)] rsvd7:u128,
		pub invlpgb_count_max:u16,
		pub max_rdpru_id:u16
	}

	derive_cpuid_trait!(ProcessorCapabilityParameters,0x80000008,None);
}

pub mod rflags
{
	pub const RFLAGS_CF_BIT:u32=0;
	pub const RFLAGS_PF_BIT:u32=2;
	pub const RFLAGS_AF_BIT:u32=4;
	pub const RFLAGS_ZF_BIT:u32=6;
	pub const RFLAGS_SF_BIT:u32=7;
	pub const RFLAGS_TF_BIT:u32=8;
	pub const RFLAGS_IF_BIT:u32=9;
	pub const RFLAGS_DF_BIT:u32=10;
	pub const RFLAGS_OF_BIT:u32=11;
	pub const RFLAGS_NT_BIT:u32=14;
	pub const RFLAGS_RF_BIT:u32=16;
	pub const RFLAGS_VM_BIT:u32=17;
	pub const RFLAGS_AC_BIT:u32=18;
	pub const RFLAGS_VIF_BIT:u32=19;
	pub const RFLAGS_VIP_BIT:u32=20;
	pub const RFLAGS_ID_BIT:u32=21;
}

pub mod interrupts
{
	use core::fmt::Display;

	pub const DIVIDE_ERROR_FAULT:u8=0;
	pub const DEBUG_FAULT_OR_TRAP:u8=1;
	pub const NMI_INTERRUPT:u8=2;
	pub const BREAKPOINT_TRAP:u8=3;
	pub const OVERFLOW_TRAP:u8=4;
	pub const EXCEED_BOUND_RANGE_FAULT:u8=5;
	pub const INVALID_OPCODE_FAULT:u8=6;
	pub const NO_MATH_COPROCESSOR_FAULT:u8=7;
	pub const DOUBLE_FAULT_ABORT:u8=8;
	pub const SEGMENT_OVERRUN_FAULT:u8=9;
	pub const INVALID_TSS_FAULT:u8=10;
	pub const SEGMENT_ABSENT_FAULT:u8=11;
	pub const STACK_FAULT:u8=12;
	pub const GENERAL_PROTECTION_FAULT:u8=13;
	pub const PAGE_FAULT:u8=14;
	pub const X87_FP_EXCEPTION_FAULT:u8=16;
	pub const ALIGNMENT_CHECK_FAULT:u8=17;
	pub const MACHINE_CHECK_ABORT:u8=18;
	pub const SIMD_FP_EXCEPTION_FAULT:u8=19;
	pub const CONTROL_PROTECTION_FAULT:u8=21;

	#[repr(u8)] pub enum EventType
	{
		ExternalInterrupt=0,
		ReservedEvent=1,
		NonMaskableInterrupt=2,
		HardwareException=3,
		SoftwareInterrupt=4,
		PrivilegedSoftwareException=5,
		SoftwareException=6,
		OtherEvent=7
	}

	#[repr(C)] pub struct InterruptStackFrame
	{
		pub return_rip:u64,
		pub return_cs:u16,
		pub return_rflags:u64,
		pub return_rsp:u64,
		pub return_ss:u16
	}

	impl Display for InterruptStackFrame
	{
		fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
		{
			writeln!(f,"Return cs:rip={:04X}:{:016X}, ",self.return_cs,self.return_rip)?;
			writeln!(f,"Return ss:rsp={:04X}:{:016X}, ",self.return_ss,self.return_rsp)?;
			write!(f,"Return rflags=0x{:016X}",self.return_rflags)
		}
	}

	#[repr(C)] pub struct InterruptStackFrameWithErrorCode
	{
		pub error_code:u32,
		pub return_rip:u64,
		pub return_cs:u16,
		pub return_rflags:u64,
		pub return_rsp:u64,
		pub return_ss:u16
	}

	impl Display for InterruptStackFrameWithErrorCode
	{
		fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
		{
			writeln!(f,"Return cs:rip={:04X}:{:016X}, ",self.return_cs,self.return_rip)?;
			writeln!(f,"Return ss:rsp={:04X}:{:016X}, ",self.return_ss,self.return_rsp)?;
			writeln!(f,"Return rflags=0x{:016X}",self.return_rflags)?;
			write!(f,"Error-Code=0x{:08X}",self.error_code)
		}
	}
}

pub mod msr
{
	pub const MSR_TSC:u32=0x10;
	pub const MSR_APIC_BASE:u32=0x1B;
	pub const MSR_SPEC_CTRL:u32=0x48;
	pub const MSR_PRED_CMD:u32=0x49;
	pub const MSR_MTRR_CAP:u32=0xFE;
	pub const MSR_SYSENTER_CS:u32=0x174;
	pub const MSR_SYSENTER_ESP:u32=0x175;
	pub const MSR_SYSENTER_EIP:u32=0x176;
	pub const MSR_DEBUG_CONTROL:u32=0x1D9;
	pub const MSR_SMRR_PHYS_BASE:u32=0x1F2;
	pub const MSR_SMRR_PHYS_MASK:u32=0x1F3;
	pub const MSR_MTRR_PHYS_BASE0:u32=0x200;
	pub const MSR_MTRR_PHYS_MASK0:u32=0x201;
	pub const MSR_MTRR_PHYS_BASE1:u32=0x202;
	pub const MSR_MTRR_PHYS_MASK1:u32=0x203;
	pub const MSR_MTRR_PHYS_BASE2:u32=0x204;
	pub const MSR_MTRR_PHYS_MASK2:u32=0x205;
	pub const MSR_MTRR_PHYS_BASE3:u32=0x206;
	pub const MSR_MTRR_PHYS_MASK3:u32=0x207;
	pub const MSR_MTRR_PHYS_BASE4:u32=0x208;
	pub const MSR_MTRR_PHYS_MASK4:u32=0x209;
	pub const MSR_MTRR_PHYS_BASE5:u32=0x20A;
	pub const MSR_MTRR_PHYS_MASK5:u32=0x20B;
	pub const MSR_MTRR_PHYS_BASE6:u32=0x20C;
	pub const MSR_MTRR_PHYS_MASK6:u32=0x20D;
	pub const MSR_MTRR_PHYS_BASE7:u32=0x20E;
	pub const MSR_MTRR_PHYS_MASK7:u32=0x20F;
	pub const MSR_MTRR_FIX64K_00000:u32=0x250;
	pub const MSR_MTRR_FIX16K_80000:u32=0x258;
	pub const MSR_MTRR_FIX16K_A0000:u32=0x259;
	pub const MSR_MTRR_FIX4K_C0000:u32=0x268;
	pub const MSR_MTRR_FIX4K_C8000:u32=0x269;
	pub const MSR_MTRR_FIX4K_D0000:u32=0x26A;
	pub const MSR_MTRR_FIX4K_D8000:u32=0x26B;
	pub const MSR_MTRR_FIX4K_E0000:u32=0x26C;
	pub const MSR_MTRR_FIX4K_E8000:u32=0x26D;
	pub const MSR_MTRR_FIX4K_F0000:u32=0x26E;
	pub const MSR_MTRR_FIX4K_F8000:u32=0x26F;
	pub const MSR_PAT:u32=0x277;
	pub const MSR_MTRR_DEF_TYPE:u32=0x2FF;
	pub const MSR_U_CET:u32=0x6A0;
	pub const MSR_S_CET:u32=0x6A2;
	pub const MSR_PL0_SSP:u32=0x6A4;
	pub const MSR_PL1_SSP:u32=0x6A5;
	pub const MSR_PL2_SSP:u32=0x6A6;
	pub const MSR_PL3_SSP:u32=0x6A7;
	pub const MSR_ISST_ADDR:u32=0x6A8;
	pub const MSR_X2APIC_MSR_START:u32=0x800;
	pub const MSR_X2APIC_ID:u32=0x802;
	pub const MSR_X2APIC_VERSION:u32=0x803;
	pub const MSR_X2APIC_TPR:u32=0x808;
	pub const MSR_X2APIC_APR:u32=0x809;
	pub const MSR_X2APIC_PPR:u32=0x80A;
	pub const MSR_X2APIC_EOI:u32=0x80B;
	pub const MSR_X2APIC_LDR:u32=0x80D;
	pub const MSR_X2APIC_SPUR_INT_VECTOR:u32=0x80F;
	pub const MSR_X2APIC_ISR:u32=0x810;
	pub const MSR_X2APIC_TMR:u32=0x818;
	pub const MSR_X2APIC_IRR:u32=0x820;
	pub const MSR_X2APIC_ESR:u32=0x828;
	pub const MSR_X2APIC_ICR:u32=0x830;
	pub const MSR_X2APIC_TIMER_LVT:u32=0x832;
	pub const MSR_X2APIC_THERMAL_LVT:u32=0x833;
	pub const MSR_X2APIC_PERFCNT_LVT:u32=0x834;
	pub const MSR_X2APIC_LINT0_LVT:u32=0x835;
	pub const MSR_X2APIC_LINT1_LVT:u32=0x836;
	pub const MSR_X2APIC_EVT:u32=0x837;
	pub const MSR_X2APIC_TIMER_INIT_COUNT:u32=0x838;
	pub const MSR_X2APIC_TIMER_CUR_COUNT:u32=0x839;
	pub const MSR_X2APIC_TIMER_DIV_CONF:u32=0x83E;
	pub const MSR_X2APIC_SELF_IPI:u32=0x83F;
	pub const MSR_X2APIC_EXT_FEAT:u32=0x840;
	pub const MSR_X2APIC_EXT_CTRL:u32=0x841;
	pub const MSR_X2APIC_SEOI:u32=0x842;
	pub const MSR_X2APIC_IER:u32=0x848;
	pub const MSR_X2APIC_EXTINT_LVT:u32=0x850;
	pub const MSR_X2APIC_MSR_END:u32=0x8FF;
	pub const MSR_XSS:u32=0xDA0;
	pub const MSR_EFER:u32=0xC0000080;
	pub const MSR_STAR:u32=0xC0000081;
	pub const MSR_LSTAR:u32=0xC0000082;
	pub const MSR_CSTAR:u32=0xC0000083;
	pub const MSR_SFMASK:u32=0xC0000084;
	pub const MSR_FS_BASE:u32=0xC0000100;
	pub const MSR_GS_BASE:u32=0xC0000101;
	pub const MSR_KERNEL_GS_BASE:u32=0xC0000102;
}