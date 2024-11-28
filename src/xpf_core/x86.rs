/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file lists universal x86 definitions for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

pub mod paging
{
	use core::ffi::c_void;	
	use paste::paste;
	use crate::{println,print,dbg_print, svm_core::amd64::msr::MSR_EFER_LMA, xpf_core::{nvbdk::*, x86::crdr::*}};

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

	/// ## `build_paging_bit_impl!` macro
	/// This macro is intended to reduce the effort of implementing common fields.
	#[macro_export] macro_rules! build_paging_bit_impl
	{
		($prefix:tt,$def:tt) =>
		{
			paste!
			{
				#[inline] fn [<get_ $def:lower>](&self)->bool
				{
					self.0&[<$prefix:upper _PAGING_ $def:upper>]==[<$prefix:upper _PAGING_ $def:upper>]
				}
				
				#[inline] fn [<set_ $def:lower>](&mut self,v:bool)
				{
					self.0|=(v as u64)<<[<$prefix:upper _PAGING_ $def:upper _BIT>];
				}
			}
		};
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

	/// ## `build_regular_paging_impl` macro
	/// This macro implements regular x86 system page table entries. \
	/// Just use this macro to quickly implement all sorts of entries like PML4E, PDPTE, etc.
	/// - `$type_name`: Case-sensitive text. This parameter is the new type name.
	/// - `$page_size`: Case-insensitive text. This paramater defines the size of the level.
	/// - `$last_level`: Literal. This parameter defines if this is the last entry on traversal.
	/// - `$ps_bit`: Literal. This parameter defines if constructor should set the page-size bit.
	macro_rules! build_regular_paging_impl
	{
		($type_name:tt,$page_size:tt,$last_level:literal,$ps_bit:literal) =>
		{
			paste!
			{
				pub struct $type_name(pub u64);

				impl $type_name
				{
					pub fn new(present:bool,write:bool,user:bool,global:bool,no_execute:bool,next_phys:u64)->Self
					{
						let p:u64=(present as u64)<<X86_PAGING_PRESENT_BIT;
						let w:u64=(write as u64)<<X86_PAGING_WRITE_BIT;
						let u:u64=(user as u64)<<X86_PAGING_USER_BIT;
						let g:u64=(global as u64)<<X86_PAGING_GLOBAL_BIT;
						let b:u64=phys_page_4kb_base(next_phys as usize) as u64;
						let ps:u64=($ps_bit as u64)<<X86_PAGING_PAGE_SIZE_BIT;
						let nx:u64=(no_execute as u64)<<X86_PAGING_NX_BIT;
						Self(p|w|u|ps|g|b|nx)
					}
				}

				impl X86PageTableEntryOps for $type_name
				{
					build_paging_bit_impl!(x86,present);
					build_paging_bit_impl!(x86,write);
					build_paging_bit_impl!(x86,user);
					build_paging_bit_impl!(x86,accessed);
					build_paging_bit_impl!(x86,nx);

					#[inline] fn get_next_level_base(&self)->u64
					{
						phys_page_4kb_base(self.0 as usize) as u64
					}

					#[inline] fn set_next_level_base(&mut self,v:u64)
					{
						self.0&=[<PHYS_PAGE_ $page_size:upper _MASK>];
						self.0|=[<phys_page_ $page_size:lower _base>](v as usize) as u64;
					}

					#[inline] fn is_last_level(&self)->bool
					{
						$last_level
					}
				}
			}
		};
	}

	build_regular_paging_impl!(Pml4e,4kb,false,false);
	build_regular_paging_impl!(HugePdpte,1gb,true,true);
	build_regular_paging_impl!(Pdpte,4kb,false,false);
	build_regular_paging_impl!(LargePde,2mb,true,true);
	build_regular_paging_impl!(Pde,4kb,false,false);
	build_regular_paging_impl!(Pte,4kb,true,false);

	/// ## `X86PageTableOps`
	/// This trait should share among System MMU, EPT, NPT and IOMMU in x86 systems. \
	/// Implement the trait members as defined in the manual. If certain fields are missing, do not implement the corresponding methods.
	pub trait X86PageTableEntryOps
	{
		fn get_present(&self)->bool;
		fn get_write(&self)->bool;
		fn get_user(&self)->bool;
		fn get_accessed(&self)->bool;
		fn get_next_level_base(&self)->u64;
		fn get_nx(&self)->bool;

		fn set_present(&mut self,v:bool);
		fn set_write(&mut self,v:bool);
		fn set_user(&mut self,v:bool);
		fn set_accessed(&mut self,v:bool);
		fn set_next_level_base(&mut self,v:u64);
		fn set_nx(&mut self,v:bool);

		fn is_last_level(&self)->bool;

		#[inline] fn get_caching(&self)->u8 {0}
		#[inline] fn set_caching(&mut self,_v:u8) {}
		#[inline] fn get_dirty(&self)->bool {false}
		#[inline] fn set_dirty(&mut self,_v:bool) {}
	}

	pub struct PageFaultErrorCode(pub u32);

	impl PageFaultErrorCode
	{
		fn new(p:bool,w:bool,u:bool,r:bool,x:bool,pk:bool,ss:bool)->Self
		{
			Self
			(
				(p as u32)|
				(w as u32)<<1|
				(u as u32)<<2|
				(r as u32)<<3|
				(x as u32)<<4|
				(pk as u32)<<5|
				(ss as u32)<<6
			)
		}
	}

	/// ## `PageTranslator` trait
	/// This trait is intended to help translating the virtual addresses to physical addresses on vCPU.
	/// Implement this trait on vCPU objects.
	pub trait PageTranslationHelper
	{
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
			return Err(PageFaultErrorCode::new(false,w,u,false,x,false,false));
		}
		if !pml_w && w
		{
			return Err(PageFaultErrorCode::new(pml_p,w,u,false,x,false,false));
		}
		if !pml_u && u
		{
			return Err(PageFaultErrorCode::new(pml_p,w,u,false,x,false,false));
		}
		if pml_nx && !x
		{
			return Err(PageFaultErrorCode::new(pml_p,w,u,false,x,false,false));
		}
		if pml_ps || level==1
		{
			// This is the last level!
			// Check Shadow-Stack: R/W bit is cleared while D bit is set means a shadow-stack page.
			let pml_d=(pml_e&X86_PAGING_DIRTY)==X86_PAGING_DIRTY;
			let pml_ss=pml_d&!pml_w;
			if pml_ss && !ss
			{
				Err(PageFaultErrorCode::new(pml_p,w,u,false,x,false,true))
			}
			else
			{
				let offset_mask:u64=(1<<shift_amount)-1;
				let base:u64=(pml_e>>shift_amount)<<shift_amount;
				println!("Offset Mask: 0x{:016X}, Base: 0x{:016X}, Shift-Amount: {}",offset_mask,base,shift_amount);
				let pa=(va&offset_mask)|base;
				println!("Final Physical Address: 0x{:016X} in level {}!",pa,level);
				Ok(pa)
			}
		}
		else
		{
			// This is the intermediate level!
			let next_pt_base=phys_page_4kb_base(pml_e as usize) as u64;
			println!("Translating 0x{:016X} in level {}! Page-Table: 0x{:016X}",va,level,next_pt_base);
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
				let level=if (efer&MSR_EFER_LMA)==MSR_EFER_LMA
				{
					// Long-Mode is activated. Virtual-Address is 64-bit!
					if (cr4&CR4_LA57)==CR4_LA57
					{
						// 5-level 57-bit Linear-Address.
						5
					}
					else
					{
						// 4-level 48-bit Linear-Address.
						4
					}
				}
				else
				{
					// 3-level 32-bit PAE paging.
					va&=0xFFFFFFFF;
					3
				};
				println!("Translating 0x{:016X} with {} levels of paging structure! CR3=0x{:X}",va,level,cr3);
				translate_64bit_va_routine(va,vcpu,cr3,level,w,x,ss)
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
				noir_copy_memory(buffer.cast(),pa as *const c_void,copy_size);
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
			let end_len=(PAGE_SIZE-page_offset(va as usize)) as u64;
			let rem_len=end_va-cur_va;
			let copy_size=if end_len<rem_len {end_len} else {rem_len};
			println!("Copying {} bytes at VA 0x{:016X}!",copy_size,cur_va);
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
}

pub mod descriptors
{
	use core::fmt::{self,Display};
	use crate::xpf_core::{hv_host::x86::AsmInterruptHandler, nvbdk::PAGE_SHIFT};

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

	pub const GATE_DESCRIPTOR_LDT:u16=0x2;
	pub const GATE_DESCRIPTOR_AVAILABLE_TSS:u16=0x9;
	pub const GATE_DESCRIPTOR_BUSY_TSS:u16=0xB;
	pub const GATE_DESCRIPTOR_CALL_GATE:u16=0xC;
	pub const GATE_DESCRIPTOR_INTERRUPT_GATE:u16=0xE;
	pub const GATE_DESCRIPTOR_TRAP_GATE:u16=0xF;

	pub const GATE_DESCRIPTOR_TYPE_BIT:u16=8;
	pub const GATE_DESCRIPTOR_DPL_BIT:u16=13;
	pub const GATE_DESCRIPTOR_PRESENT_BIT:u16=15;

	#[derive(Default,Clone,Copy)]
	#[repr(C,packed)] pub struct GateDescriptor
	{
		pub offset_lo:u16,
		pub selector:u16,
		pub flags:u16,
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
			let flags=ist|(GATE_DESCRIPTOR_INTERRUPT_GATE<<GATE_DESCRIPTOR_TYPE_BIT)|(dpl<<GATE_DESCRIPTOR_DPL_BIT)|(1<<GATE_DESCRIPTOR_PRESENT_BIT);
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
	define_bit!(DR6_BD,13);
	define_bit!(DR6_BS,14);
	define_bit!(DR6_BT,15);
}

pub mod cpuid
{
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
	pub const CPUID_UNDER_HYPERVISOR:u32=0x80000000;
	/// Use this flag for CPUID[EAX=0x80000001].EDX
	pub const CPUID_1GB_PAGE:u32=0x4000000;
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

	pub enum EventType
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
			writeln!(f,"Return rflags=0x{:016X}",self.return_rflags)
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
			writeln!(f,"Error-Code=0x{:08X}",self.error_code)
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