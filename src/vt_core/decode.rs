/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file helps fetching and decoding instructions in Intel VT-x of
 * NoirVisor Core in Rust since it's not supported in the processor.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

use crate::{vt_core::{VtVcpu, vmcs::*}, xpf_core::{asm::vt::{vmread32, vmread64, vmreadptr}, nvbdk::memcpy, x86::{crdr::{Cr0, Cr4}, msr::Efer, paging::{PageTranslationHelper, read_virtual_address}}}};

impl PageTranslationHelper for VtVcpu
{
	fn get_cr0(&self)->Cr0
	{
		Cr0::from_bits(vmreadptr(GUEST_CR0).unwrap() as u64)
	}

	fn get_cr3(&self)->u64
	{
		vmreadptr(GUEST_CR3).unwrap() as u64
	}

	fn get_cr4(&self)->Cr4
	{
		Cr4::from_bits(vmreadptr(GUEST_CR4).unwrap() as u64)
	}
	
	fn get_efer(&self)->Efer
	{
		Efer::from_bits(vmread64(GUEST_MSR_IA32_EFER).unwrap())
	}

	fn is_user_mode(&self)->bool
	{
		let ss_ar=SegmentAccessRights::from_bits(vmread32(GUEST_SS_ACCESS_RIGHTS).unwrap());
		ss_ar.dpl()==3
	}

	fn read_phys_mem(&self,pa:u64,buffer:&mut [u8])->usize
	{
		unsafe
		{
			memcpy(buffer.as_mut_ptr().cast(),pa as *const c_void,buffer.len());
		}
		buffer.len()
	}

	fn write_phys_mem(&self,pa:u64,buffer:&[u8])->usize
	{
		unsafe
		{
			memcpy(pa as *mut c_void,buffer.as_ptr().cast(),buffer.len());
		}
		buffer.len()
	}
}

impl VtVcpu
{
	pub(super) fn get_current_bitness(&self)->u32
	{
		let cs_ar=SegmentAccessRights::from_bits(vmread32(GUEST_CS_ACCESS_RIGHTS).unwrap());
		let efer=self.get_efer();
		if efer.lma()
		{
			// Long-Mode is active. It could be either 32-bit or 64-bit.
			if cs_ar.long_mode() {64} else {32}
		}
		else
		{
			// Long-Mode is inactive. It could be either 16-bit or 32-bit.
			if cs_ar.default_size() {32} else {16}
		}
	}

	pub(super) fn fetch_instruction(&mut self)->[u8;15]
	{
		let mut instruction_bytes:[u8;15]=[0;15];
		let rip=vmread64(GUEST_RIP).unwrap();
		let mut fault_pa:Option<u64>=None;
		if let Err(e)=read_virtual_address(rip,self,&mut instruction_bytes,&mut fault_pa)
		{
			panic!("Page-fault is triggered by software while fetching instruction! Code: 0x{:08X}, rip=0x{rip:016X}",e.into_bits());
		}
		instruction_bytes
	}
}