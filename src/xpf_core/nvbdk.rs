/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file defines Basic Development Kits for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, ptr::null_mut, usize};
use paste::paste;

#[derive(Copy,Clone)] #[repr(C)] pub struct MemoryDescriptor
{
	pub virt:*mut c_void,
	pub phys:u64
}

impl MemoryDescriptor
{
	pub fn new(virt:*mut c_void,phys:u64)->Self
	{
		Self
		{
			virt:virt,
			phys:phys
		}
	}

	pub fn null()->Self
	{
		Self
		{
			virt:null_mut(),
			phys:0
		}
	}
}

#[repr(C)] #[derive(Default)] pub struct SegmentRegister
{
	pub selector:u16,
	pub attrib:u16,
	pub limit:u32,
	pub base:u64
}

#[repr(C)] #[derive(Default)] pub struct ProcessorState
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

#[repr(C)] #[derive(Clone, Copy)] pub struct GprState
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

impl GprState
{
	pub fn read(&self,index:u64)->Option<u64>
	{
		if index>=16
		{
			None
		}
		else
		{
			let array=self as *const Self as *const u64;
			Some(unsafe{array.add(index as usize).read()})
		}
	}

	pub fn write(&mut self,index:u64,value:u64)
	{
		if index<16
		{
			let array=self as *mut Self as *mut u64;
			unsafe{array.add(index as usize).write(value);}
		}
	}
}

type BroadcastWorker=extern "C" fn(context:*mut c_void,processor_id:u32)->();
type PhysicalRangeCallback=extern "C" fn(start:u64,length:u64,context:*mut c_void)->();

extern "C"
{
	// Processor State Facility
	pub fn noir_get_processor_count()->u32;
	pub fn noir_get_current_processor()->u32;
	pub fn noir_save_processor_state(state:*mut ProcessorState)->();
	pub fn noir_generic_call(worker:BroadcastWorker,context:*mut c_void)->();
	// Memory Facility
	pub fn noir_alloc_contd_memory(length:usize)->*mut c_void;
	pub fn noir_free_contd_memory(virtual_address:*mut c_void,length:usize)->();
	pub fn noir_enum_physical_memory_ranges(callback_rt:PhysicalRangeCallback,context:*mut c_void)->();
	pub fn noir_get_physical_address(virtual_address:*mut c_void)->u64;
}

/**
  # `alloc_contd_pages`
  Allocate some contiguous pages in memory with `length` bytes.
  Return Value: Option<MemoryDescriptor>
  - Some(MemoryDescriptor)=> Both virtual/physical addresses of the start of contiguous pages.
  - None=> Insufficient resource!
 */
pub fn alloc_contd_pages(length:usize)->Option<MemoryDescriptor>
{
	let p=unsafe{noir_alloc_contd_memory(length)};
	if p==null_mut()
	{
		None
	}
	else
	{
		Some
		(
			MemoryDescriptor
			{
				virt:p,
				phys:unsafe{noir_get_physical_address(p)}
			}
		)
	}
}

pub fn free_contd_pages(ptr:*mut c_void,length:usize)->()
{
	unsafe{noir_free_contd_memory(ptr,length)}
}

// Page-related definitions
// Use macro to reduce effort and make sure correctness.
macro_rules! build_page_shift
{
	($size:tt,$shift:literal) =>
	{
		paste!
		{
			pub const [<PAGE $size:upper SHIFT>]:usize=$shift;
		}
	};
}

build_page_shift!(_,12);
build_page_shift!(_4KB_,12);
build_page_shift!(_2MB_,21);
build_page_shift!(_4MB_,22);
build_page_shift!(_1GB_,30);
build_page_shift!(_512GB_,39);
build_page_shift!(_256TB_,48);

macro_rules! build_page_size
{
	($size:tt) =>
	{
		paste!
		{
			pub const [<PAGE $size:upper SIZE>]:usize=1<<[<PAGE $size:upper SHIFT>];
		}
	};
}

build_page_size!(_);
build_page_size!(_4KB_);
build_page_size!(_2MB_);
build_page_size!(_4MB_);
build_page_size!(_1GB_);
build_page_size!(_512GB_);
build_page_size!(_256TB_);

pub const PAGE_SHIFT_DIFF:usize=if cfg!(target_arch="x86_64") {10} else {9};
pub const PAGE_TABLE_ENTRIES:usize=if cfg!(target_arch="x86_64") {512} else {1024};

#[inline] pub fn page_entry_index(addr:usize)->usize
{
	addr&(PAGE_TABLE_ENTRIES-1)
}

macro_rules! build_page_offset
{
	($size:tt) =>
	{
		paste!
		{
			// Use function in order to check type.
			#[inline] pub fn [<page $size:lower offset>](addr:usize)->usize
			{
				addr&([<PAGE $size:upper SIZE>]-1)
			}
		}
	};
}

build_page_offset!(_);
build_page_offset!(_4kb_);
build_page_offset!(_2mb_);
build_page_offset!(_4mb_);
build_page_offset!(_1gb_);
build_page_offset!(_512gb_);
build_page_offset!(_256tb_);

macro_rules! build_page_count
{
	($size:tt) =>
	{
		paste!
		{
			// Use function in order to check type.
			#[inline] pub fn [<page $size:lower count>](addr:usize)->usize
			{
				addr>>[<PAGE $size:upper SHIFT>]
			}
		}
	};
}

build_page_count!(_);
build_page_count!(_4kb_);
build_page_count!(_2mb_);
build_page_count!(_4mb_);
build_page_count!(_1gb_);
build_page_count!(_512gb_);
build_page_count!(_256tb_);

macro_rules! build_page_mult
{
	($size:tt) =>
	{
		paste!
		{
			// Use function in order to check type.
			#[inline] pub fn [<page $size:lower mult>](addr:usize)->usize
			{
				addr<<[<PAGE $size:upper SHIFT>]
			}
		}
	};
}

build_page_mult!(_);
build_page_mult!(_4kb_);
build_page_mult!(_2mb_);
build_page_mult!(_4mb_);
build_page_mult!(_1gb_);
build_page_mult!(_512gb_);
build_page_mult!(_256tb_);

macro_rules! build_bytes_to_pages
{
	($size:tt) =>
	{
		paste!
		{
			// Use function in order to check type.
			#[inline] pub fn [<bytes_to $size:lower pages>](len:usize)->usize
			{
				[<page $size:lower count>](len)+(if [<page $size:lower offset>](len)!=0 {1} else {0})
			}
		}
	};
}

build_bytes_to_pages!(_);
build_bytes_to_pages!(_4kb_);
build_bytes_to_pages!(_2mb_);
build_bytes_to_pages!(_4mb_);
build_bytes_to_pages!(_1gb_);
build_bytes_to_pages!(_512gb_);
build_bytes_to_pages!(_256tb_);

macro_rules! build_page_base
{
	($size:tt) =>
	{
		paste!
		{
			// Use function in order to check type.
			#[inline] pub fn [<page $size:lower base>](addr:usize)->usize
			{
				addr&(!([<PAGE $size:upper SIZE>]-1))
			}
		}
	};
}

build_page_base!(_);
build_page_base!(_4kb_);
build_page_base!(_2mb_);
build_page_base!(_4mb_);
build_page_base!(_1gb_);
build_page_base!(_512gb_);
build_page_base!(_256tb_);