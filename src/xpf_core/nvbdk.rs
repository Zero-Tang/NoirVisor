/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines Basic Development Kits for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{arch::x86_64::_bittest64, convert::From, ffi::c_void, fmt::{self, Display}, ops::*, ptr::null_mut, slice};
use super::{asm::{crdr::*, msr::rdmsr, seg::*}, x86::{descriptors::{DescriptorTable, SegmentFlags}, msr::*}};
use alloc::vec::Vec;
use bitfield_struct::bitfield;
use paste::paste;
use spin::Lazy;

#[cfg(windows)] use crate::mshv_core::forwarder::MshvForwardStack;
use crate::xpf_core::allocator::{ContiguousAllocator, InternalPageAllocator};

/// The `MemoryDescriptor<N,T>` is a descriptor type which describes a
/// contiguous range of memory with type `T` and `N` pages.
pub struct MemoryDescriptor<const N:usize,T:Sized,A:ContiguousAllocator=InternalPageAllocator>
{
	pub virt:*mut T,
	pub phys:u64,
	allocator:A
}

unsafe impl<const N:usize,T:Sized,A:ContiguousAllocator> Send for MemoryDescriptor<N,T,A> {}

impl<const N:usize,T:Sized> MemoryDescriptor<N,T>
{
	/// Constructs a memory descriptor from raw pointer.
	/// 
	/// ## Safety
	/// You must either ensure the constructed descriptor can be dropped properly,
	/// or wrap the descriptor with `ManuallyDrop` generic type.
	pub unsafe fn new(virt:*mut T,phys:u64)->Self
	{
		Self
		{
			virt,
			phys,
			allocator:InternalPageAllocator
		}
	}

	/// Constructs a null memory descriptor. The `Drop` trait ignores null memory descriptors.
	pub const fn null()->Self
	{
		Self
		{
			virt:null_mut(),
			phys:0,
			allocator:InternalPageAllocator
		}
	}
}

impl<const N:usize,T:Sized,A:ContiguousAllocator> MemoryDescriptor<N,T,A>
{
	/// Constructs a memory descriptor from raw pointer with a specific allocator.
	/// 
	/// ## Safety
	/// You must either ensure the constructed descriptor can be dropped properly,
	/// or wrap the descriptor with `ManuallyDrop` generic type.
	pub unsafe fn new_in(virt:*mut T,phys:u64,allocator:A)->Self
	{
		Self
		{
			virt,
			phys,
			allocator
		}
	}
}

impl<const N:usize,T:Sized,A:ContiguousAllocator> Drop for MemoryDescriptor<N,T,A>
{
	fn drop(&mut self)
	{
		if self.virt.is_null()
		{
			return;
		}
		unsafe
		{
			self.allocator.free(self.virt.cast(),N);
		}
	}
}

pub struct MmioDescriptor<const N:usize,T:Sized>
{
	pub virt:*mut T,
	pub phys:u64
}

impl<const N:usize,T:Sized> MmioDescriptor<N,T>
{
	pub const fn null()->Self
	{
		Self
		{
			virt:null_mut(),
			phys:0
		}
	}

	pub fn map(phys:u64)->Self
	{
		Self
		{
			virt:unsafe{noir_map_uncached_memory(phys,page_4kb_mult(N)).cast()},
			phys
		}
	}
}

#[repr(C)] #[derive(Default,Debug,Clone,Copy)] pub struct SegmentRegister
{
	pub selector:u16,
	pub attrib:u16,
	pub limit:u32,
	pub base:u64
}

impl SegmentRegister
{
	pub fn from_sel_gdt(gdt:&DescriptorTable,selector:u16)->Option<Self>
	{
		if selector<gdt.limit
		{
			let p=(gdt.base+(selector&0xfff8) as u64) as *const u8;
			let a=unsafe{p.add(5).cast::<SegmentFlags>().read_unaligned()};
			Some
			(
				Self
				{
					selector,
					attrib:a.into_bits(),
					limit:lsl(selector),
					base:if a.present()
					{
						if a.system_segment()
						{
							0
						}
						else
						{
							let lo=unsafe{p.add(2).cast::<u16>().read()} as u64;
							let mid1=unsafe{p.add(4).read()} as u64;
							let mid2=unsafe{p.add(7).read()} as u64;
							let hi=unsafe{p.add(8).cast::<u32>().read()} as u64;
							lo|(mid1<<16)|(mid2<<24)|(hi<<32)
						}
					}
					else
					{
						0
					}
				}
			)
		}
		else
		{
			None
		}
	}

	pub fn from_sel_gdt_with_base(gdt:&DescriptorTable,selector:u16,base:u64)->Option<Self>
	{
		Self::from_sel_gdt(gdt,selector).map(|seg| Self
		{
			selector:seg.selector,
			attrib:seg.attrib,
			limit:seg.limit,
			base
		})
	}

	pub fn from_dt(descriptor:&DescriptorTable)->Self
	{
		Self
		{
			selector:0,
			attrib:0,
			limit:descriptor.limit as u32,
			base:descriptor.base
		}
	}

	pub const fn reset_code()->Self
	{
		Self
		{
			selector:0xF000,
			attrib:0x9B,
			limit:0xFFFF,
			base:0xFFFF0000
		}
	}

	pub const fn reset_data()->Self
	{
		Self
		{
			selector:0,
			attrib:0x92,
			limit:0xFFFF,
			base:0
		}
	}

	pub const fn reset_tss()->Self
	{
		Self
		{
			selector:0,
			attrib:0x83,
			limit:0xFFFF,
			base:0
		}
	}

	pub const fn reset_ldt()->Self
	{
		Self
		{
			selector:0,
			attrib:0x82,
			limit:0xFFFF,
			base:0
		}
	}

	pub const fn reset_dt()->Self
	{
		Self
		{
			selector:0,
			attrib:0,
			limit:0xFFFF,
			base:0
		}
	}
}

#[derive(Default)] pub struct ProcessorState
{
	pub cs:SegmentRegister,
	pub ds:SegmentRegister,
	pub es:SegmentRegister,
	pub fs:SegmentRegister,
	pub gs:SegmentRegister,
	pub ss:SegmentRegister,
	pub tr:SegmentRegister,
	pub gdtr:SegmentRegister,
	pub idtr:SegmentRegister,
	pub ldtr:SegmentRegister,
	pub cr0:usize,
	pub cr2:usize,
	pub cr3:usize,
	pub cr4:usize,
	pub cr8:u64,
	pub dr0:usize,
	pub dr1:usize,
	pub dr2:usize,
	pub dr3:usize,
	pub dr6:usize,
	pub dr7:usize,
	pub sysenter_cs:u64,
	pub sysenter_esp:u64,
	pub sysenter_eip:u64,
	pub debug_ctrl:u64,
	pub pat:u64,
	pub efer:u64,
	pub star:u64,
	pub lstar:u64,
	pub cstar:u64,
	pub sfmask:u64,
	pub fsbase:u64,
	pub gsbase:u64,
	pub gsswap:u64
}

impl ProcessorState
{
	pub fn new()->Self
	{
		let gdtr=read_gdtr();
		let idtr=read_idtr();
		Self
		{
			cs:SegmentRegister::from_sel_gdt(&gdtr,read_cs()).unwrap(),
			ds:SegmentRegister::from_sel_gdt(&gdtr,read_ds()).unwrap(),
			es:SegmentRegister::from_sel_gdt(&gdtr,read_es()).unwrap(),
			fs:SegmentRegister::from_sel_gdt_with_base(&gdtr,read_fs(),rdmsr(MSR_FS_BASE)).unwrap(),
			gs:SegmentRegister::from_sel_gdt_with_base(&gdtr,read_gs(),rdmsr(MSR_GS_BASE)).unwrap(),
			ss:SegmentRegister::from_sel_gdt(&gdtr,read_ss()).unwrap(),
			tr:SegmentRegister::from_sel_gdt(&gdtr,read_tr()).unwrap(),
			ldtr:SegmentRegister::from_sel_gdt(&gdtr,read_ldt()).unwrap(),
			gdtr:SegmentRegister::from_dt(&gdtr),
			idtr:SegmentRegister::from_dt(&idtr),
			cr0:read_cr0() as usize,
			cr2:read_cr2() as usize,
			cr3:read_cr3() as usize,
			cr4:read_cr4() as usize,
			cr8:read_cr8(),
			dr0:read_dr0() as usize,
			dr1:read_dr1() as usize,
			dr2:read_dr2() as usize,
			dr3:read_dr3() as usize,
			dr6:read_dr6() as usize,
			dr7:read_dr7() as usize,
			sysenter_cs:rdmsr(MSR_SYSENTER_CS),
			sysenter_esp:rdmsr(MSR_SYSENTER_ESP),
			sysenter_eip:rdmsr(MSR_SYSENTER_EIP),
			debug_ctrl:rdmsr(MSR_DEBUG_CONTROL),
			pat:rdmsr(MSR_PAT),
			efer:rdmsr(MSR_EFER),
			star:rdmsr(MSR_STAR),
			lstar:rdmsr(MSR_LSTAR),
			cstar:rdmsr(MSR_CSTAR),
			sfmask:rdmsr(MSR_SFMASK),
			fsbase:rdmsr(MSR_FS_BASE),
			gsbase:rdmsr(MSR_GS_BASE),
			gsswap:rdmsr(MSR_KERNEL_GS_BASE)
		}
	}
}

#[repr(C)] pub struct GprState
{
	pub rax:u64,
	pub rcx:u64,
	pub rdx:u64,
	pub rbx:u64,
	pub rsp:u64,
	pub rbp:u64,
	pub rsi:u64,
	pub rdi:u64,
	pub r8:u64,
	pub r9:u64,
	pub r10:u64,
	pub r11:u64,
	pub r12:u64,
	pub r13:u64,
	pub r14:u64,
	pub r15:u64
}

impl Display for GprState
{
	fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result
	{
		writeln!(f,"rax=0x{:016X} rcx=0x{:016X} rdx=0x{:016X} rbx=0x{:016X}",self.rax,self.rcx,self.rdx,self.rbx)?;
		writeln!(f,"rsp=0x{:016X} rbp=0x{:016X} rsi=0x{:016X} rdi=0x{:016X}",self.rsp,self.rbp,self.rsi,self.rdi)?;
		writeln!(f,"r8 =0x{:016X} r9 =0x{:016X} r10=0x{:016X} r11=0x{:016X}",self.r8,self.r9,self.r10,self.r11)?;
		write!(f,"r12=0x{:016X} r13=0x{:016X} r14=0x{:016X} r15=0x{:016X}",self.r12,self.r13,self.r14,self.r15)
	}
}

impl GprState
{
	pub fn read<T:Into<usize>>(&self,index:T)->Option<u64>
	{
		let i:usize=index.into();
		if i>=16
		{
			None
		}
		else
		{
			let array=self as *const Self as *const u64;
			Some(unsafe{array.add(i).read()})
		}
	}

	pub fn write<T:Into<usize>>(&mut self,index:T,value:u64)
	{
		let i:usize=index.into();
		if i<16
		{
			let array=self as *mut Self as *mut u64;
			unsafe{array.add(i).write(value);}
		}
	}
}

/// Note: This structure only defines volatile state for MSVC ABI (i.e.: Windows, UEFI).
/// Volatile state for other ABIs are not implemented yet.
#[repr(C,align(16))] pub struct VolatileXmmState
{
	pub xmm0:[u8;16],
	pub xmm1:[u8;16],
	pub xmm2:[u8;16],
	pub xmm3:[u8;16],
	pub xmm4:[u8;16],
	pub xmm5:[u8;16],
	pub mxcsr:u32,
	pub flags:u32,
	pub xsave_ptr:*mut c_void
}

#[repr(C)] pub struct SegmentState
{
	pub es:SegmentRegister,
	pub cs:SegmentRegister,
	pub ss:SegmentRegister,
	pub ds:SegmentRegister,
	pub fs:SegmentRegister,
	pub gs:SegmentRegister,
	pub tr:SegmentRegister,
	pub gdtr:SegmentRegister,
	pub idtr:SegmentRegister,
	pub ldtr:SegmentRegister
}

#[repr(C)] pub struct CrState
{
	pub cr0:u64,
	pub cr2:u64,
	pub cr3:u64,
	pub cr4:u64,
	pub cr8:u64
}

#[repr(C)] pub struct DrState
{
	pub dr0:u64,
	pub dr1:u64,
	pub dr2:u64,
	pub dr3:u64,
	pub dr6:u64,
	pub dr7:u64
}

#[repr(C)] pub struct MsrState
{
	pub sysenter_cs:u64,
	pub sysenter_esp:u64,
	pub sysenter_eip:u64,
	pub pat:u64,
	pub efer:u64,
	pub star:u64,
	pub lstar:u64,
	pub cstar:u64,
	pub sfmask:u64,
	pub ststar:u64,
	pub gsswap:u64,
	pub debug_ctrl:u64
}

#[bitfield(u64)] pub struct EnabledFeatures
{
	pub stealthy_msr_hook:bool,
	pub stealthy_inline_hook:bool,
	pub cpuid_hv_presence:bool,
	pub disable_patchguard:bool,
	pub nested_virtualization:bool,
	pub kva_shadow_presence:bool,
	pub tlfs_passthrough:bool,
	pub hide_from_pt:bool,
	pub enable_nsv:bool,
	pub enable_iommu:bool,
	#[bits(54)] rsvd:u64
}

impl EnabledFeatures
{
	pub fn get()->Self
	{
		Self(unsafe{noir_query_enabled_features_in_system()} as u64)
	}

	const FEATURE_NAMES:[&'static str;10]=
	[
		"Stealthy MSR-Hook",
		"Stealthy Inline-Hook",
		"CPUID Presence",
		"Disable PatchGuard",
		"Nested Virtualization",
		"KVA Shadow Compatibility",
		"TLFS Passthrough",
		"Hide PT Events",
		"Secure Virtualization",
		"DMA Protection"
	];
}

impl Display for EnabledFeatures
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		write!(f,"(")?;
		let mut count:usize=0;
		for (i,&s) in Self::FEATURE_NAMES.iter().enumerate()
		{
			if unsafe{_bittest64((&raw const self.0).cast(),i as i64)!=0}
			{
				if count!=0 {write!(f,", ")?;}
				write!(f,"{s}")?;
				count+=1;
			}
		}
		write!(f,")")
	}
}

#[repr(C)] pub struct XcrState
{
	pub xcr0:u64
}

pub type BroadcastWorker=extern "C" fn(context:*mut c_void,processor_id:u32);
pub type PhysicalRangeCallback=extern "C" fn(start:u64,length:u64,context:*mut c_void);

pub struct PhysicalRange
{
	pub start:u64,
	pub length:u64
}

extern "C" fn phys_mem_range_enum_rt(start:u64,length:u64,context:*mut c_void)
{
	let v:&mut Vec<PhysicalRange>=unsafe{&mut *context.cast()};
	v.push(PhysicalRange{start,length});
}

pub static SYSTEM_PHYSICAL_MEMORY_RANGES:Lazy<Vec<PhysicalRange>>=Lazy::new(||
{
	let mut v=Vec::new();
	unsafe{noir_enum_physical_memory_ranges(phys_mem_range_enum_rt,(&raw mut v).cast())};
	v
});

unsafe extern "C"
{
	// Processor State Facility
	pub fn noir_get_processor_count()->u32;
	pub fn noir_get_current_processor()->u32;
	pub fn noir_generic_call(worker:BroadcastWorker,context:*mut c_void);
	// Memory Facility
	pub fn noir_get_physical_address(virtual_address:*mut c_void)->u64;
	pub fn noir_alloc_2mb_page()->*mut c_void;
	pub fn noir_free_2mb_page(virtual_address:*mut c_void);
	pub fn noir_find_virt_by_phys(physical_address:u64)->*mut c_void;
	pub fn noir_map_physical_memory(physical_address:u64,length:usize)->*mut c_void;
	pub fn noir_map_uncached_memory(physical_address:u64,length:usize)->*mut c_void;
	pub fn noir_unmap_physical_memory(virtual_address:*mut c_void,length:usize);
	pub fn noir_enum_physical_memory_ranges(callback_routine:PhysicalRangeCallback,context:*mut c_void);
	pub fn memcpy(dest:*mut c_void,src:*const c_void,cch:usize)->*mut c_void;
	pub fn memset(dest:*mut c_void,val:u8,cch:usize)->*mut c_void;
	// Image Facility
	pub fn nvc_store_image_info(base:*mut *mut c_void,size:*mut u32);
	// Configuration Facility
	pub fn noir_query_enabled_features_in_system()->i64;
	// String Facility
	pub fn strlen(ptr:*const u8)->usize;
	// Synchronization Facility
	pub fn noir_acquire_pushlock_exclusive(push_lock:*mut usize);
	pub fn noir_acquire_pushlock_shared(push_lock:*mut usize);
	pub fn noir_release_pushlock_exclusive(push_lock:*mut usize);
	pub fn noir_release_pushlock_shared(push_lock:*mut usize);
	// TLFS Forwarder Facility
	#[cfg(windows)] pub fn nvc_forward_fast_hypercall(forward_stack:*mut MshvForwardStack);
	#[cfg(windows)] pub fn nvc_forward_memory_mapped_hypercall(code:u64,input_gpa:u64,output_gpa:u64,source_rax:u64)->u64;
}

/// ## `nulstr_from_ptr` function
/// Returns a string slice. The length depends on the position of the null-terminator.
/// 
/// ## Safety
/// The `ptr` must point to a valid UTF-8 string that ends with a null-terminator.
pub unsafe fn nulstr_from_ptr<'a>(ptr:*const u8)->&'a str
{
	unsafe
	{
		let str_slice=slice::from_raw_parts(ptr,strlen(ptr));
		str::from_utf8_unchecked(str_slice)
	}
}

// Page-related definitions
// Use macro to reduce effort and make sure correctness.
macro_rules! build_page_def
{
	($size:tt,$shift:literal) =>
	{
		paste!
		{
			pub const [<PAGE $size:upper SHIFT>]:u8=$shift;
			pub const [<PAGE $size:upper SIZE>]:usize=1<<[<PAGE $size:upper SHIFT>];
			pub const [<PHYS_PAGE $size:upper MASK>]:usize=0xFFF0000000000000|([<PAGE $size:upper SIZE>]-1);
			
			#[inline] pub fn [<page $size:lower offset>]<T:BitAnd<Output=T>+TryFrom<usize>>(addr:T)->T
			{
				match T::try_from([<PAGE $size:upper SIZE>]-1)
				{
					Ok(mask)=>addr&mask,
					Err(_)=>panic!("Cannot calculate page offset into this generic type!")
				}
			}

			#[inline] pub fn [<page $size:lower count>]<T:Shr<Output=T>+From<u8>>(addr:T)->T
			{
				addr>>T::from([<PAGE $size:upper SHIFT>])
			}

			#[inline] pub fn [<page $size:lower mult>]<T:Shl<Output=T>+From<u8>>(addr:T)->T
			{
				addr<<T::from([<PAGE $size:upper SHIFT>])
			}
			
			#[inline] pub fn [<page $size:lower base>]<T:BitAnd<Output=T>+TryFrom<usize>>(addr:T)->T
			{
				match T::try_from(!([<PAGE $size:upper SIZE>]-1))
				{
					Ok(mask)=>addr&mask,
					Err(_)=>panic!("Cannot calculate page base into this generic type!")
				}
			}
			
			#[inline] pub fn [<phys_page $size:lower base>]<T:BitAnd<Output=T>+TryFrom<usize>>(addr:T)->T
			{
				match T::try_from(0xFFFFFFFFFF000)
				{
					Ok(mask)=>[<page $size:lower base>](addr)&mask,
					Err(_)=>panic!("Cannot calculate physical page base into this generic type!")
				}
			}

			#[inline] pub fn [<bytes_to $size:lower pages>]<T:Copy+BitAnd<Output=T>+Shr<Output=T>+Add<Output=T>+TryFrom<usize>+From<u8>+PartialEq>(len:T)->T
			{
				[<page $size:lower count>](len)+T::from(if [<page $size:lower offset>](len)!=T::from(0) {1} else {0})
			}
		}
	};
}

build_page_def!(_,12);
build_page_def!(_4KB_,12);
build_page_def!(_2MB_,21);
build_page_def!(_4MB_,22);
build_page_def!(_1GB_,30);
build_page_def!(_512GB_,39);
build_page_def!(_256TB_,48);

pub const PAGE_SHIFT_DIFF64:usize=9;
pub const PAGE_SHIFT_DIFF32:usize=10;
pub const PAGE_SHIFT_DIFF:usize=if cfg!(target_arch="x86_64") {PAGE_SHIFT_DIFF64} else {PAGE_SHIFT_DIFF32};
pub const PAGE_TABLE_ENTRIES:usize=if cfg!(target_arch="x86_64") {PAGE_TABLE_ENTRIES64} else {PAGE_TABLE_ENTRIES32};
pub const PAGE_TABLE_ENTRIES64:usize=512;
pub const PAGE_TABLE_ENTRIES32:usize=1024;

#[inline] pub const fn phys_addr_mask(addr:u64)->u64
{
	addr&((1<<52)-1)
}

#[inline] pub const fn page_entry_index(addr:usize)->usize
{
	addr&(PAGE_TABLE_ENTRIES-1)
}