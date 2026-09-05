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

use core::slice;

use crate::{disasm::emulator::EmulatorOps, vt_core::{VtVcpu, vmcs::*}, xpf_core::{asm::vt::{vmread16, vmread32, vmread64, vmreadptr, vmwrite16}, x86::{crdr::{Cr0, Cr4}, msr::Efer}}};

impl EmulatorOps for VtVcpu
{
	fn get_gpr(&self,gpr_index:usize)->u64
	{
		self.get_stack_top().gpr_state.read(gpr_index).unwrap()
	}

	fn set_gpr(&mut self,gpr_index:usize,value:u64)
	{
		self.get_stack_top_mut().gpr_state.write(gpr_index,value);
	}

	fn get_rip(&self)->u64
	{
		self.cached_ctxt.rip
	}

	fn get_seg_selector(&self,seg_index:usize)->u16
	{
		vmread16(GUEST_ES_SELECTOR+(seg_index<<1)).unwrap()
	}

	fn set_seg_selector(&mut self,seg_index:usize,value:u16)
	{
		vmwrite16(GUEST_ES_SELECTOR+(seg_index<<1),value);
	}

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
	
	fn read_gpa(&mut self,gpa:u64,value:&mut [u8])->usize
	{
		// TODO: dispatch MMIO inputs.
		let src=unsafe{slice::from_raw_parts(gpa as *const u8,value.len())};
		value.copy_from_slice(src);
		value.len()
	}

	fn write_gpa(&mut self,gpa:u64,value:&[u8])->usize
	{
		// TODO: dispatch MMIO outputs.
		let dest=unsafe{slice::from_raw_parts_mut(gpa as *mut u8,value.len())};
		dest.copy_from_slice(value);
		value.len()
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
		if let Err((e,_))=self.read_virt(rip,&mut instruction_bytes)
		{
			panic!("Page-fault is triggered by software while fetching instruction! Code: 0x{:08X}, rip=0x{rip:016X}",e.into_bits());
		}
		instruction_bytes
	}
}