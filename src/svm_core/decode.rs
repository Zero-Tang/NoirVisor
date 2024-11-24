/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file implements Decode-Assists and Next-Rip-Saving in AMD-V of
 * NoirVisor Core in Rust if not supported by the processor.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::slice;
use exit::*;

use super::{xpf_core::x86::paging::*,svm_core::*};

impl PageTranslationHelper for SvmVcpu
{
	fn get_cr0(&self)->u64
	{
		unsafe 
		{
			vmread(self.vmcb.virt,GUEST_CR0)
		}
	}

	fn get_cr3(&self)->u64
	{
		unsafe
		{
			vmread(self.vmcb.virt,GUEST_CR3)
		}
	}

	fn get_cr4(&self)->u64
	{
		unsafe
		{
			vmread(self.vmcb.virt,GUEST_CR4)
		}
	}

	fn get_efer(&self)->u64
	{
		unsafe
		{
			vmread(self.vmcb.virt,GUEST_EFER)
		}
	}

	fn is_user_mode(&self)->bool
	{
		false
	}

	fn read_phys_mem(&self,pa:u64,buffer:&mut [u8])->usize
	{
		unsafe
		{
			noir_copy_memory(buffer.as_mut_ptr().cast(),pa as *const c_void,buffer.len());
		}
		buffer.len()
	}

	fn write_phys_mem(&self,pa:u64,buffer:&[u8])->usize
	{
		unsafe
		{
			noir_copy_memory(pa as *mut c_void,buffer.as_ptr().cast(),buffer.len());
		}
		buffer.len()
	}
}

impl SvmVcpu
{
	fn decode_unknown(&mut self)
	{
		// This interception means NoirVisor does not know such interception at all. So panic on interception.
		let exit_reason:i64=unsafe{vmread(self.vmcb.virt,EXIT_CODE)};
		panic!("Unknown interception decode request! Intercept Code: 0x{:016X}",exit_reason);
	}

	fn decode_instruction(&mut self)
	{
		// This interception does not involve assisting decodings.
		// However, it still helps if Next-RIP Saving is unsupported by the processor.
	}

	fn decode_event(&mut self)
	{
		// This interception does not involve assisting decodings.
		// Event has no instruction length, so just do nothing.
	}

	fn decode_npf(&mut self)
	{
		let rip:u64=unsafe{vmread(self.vmcb.virt,GUEST_RIP)};
		println!("Fetching instruction for #NPF! rip=0x{:016X}",rip);
		let buff=unsafe{slice::from_raw_parts_mut(self.vmcb.virt.cast::<u8>().add(GUEST_INSTRUCTION_BYTES),15)};
		let mut fault_pa:Option<u64>=None;
		let r=read_virtual_address(rip,self,buff,&mut fault_pa);
		if let Err(e)=r
		{
			panic!("Page-Fault is triggered by software! Reason: 0x{:08X}",e.0);
		}
	}
}

type SvmHostDecodeHandler=fn (&mut SvmVcpu);

const SVM_HOST_DECODE_HANDLER_GROUP1:[SvmHostDecodeHandler;SVM_MAXIMUM_CODE1]=
{
	let mut array:[SvmHostDecodeHandler;SVM_MAXIMUM_CODE1]=[SvmVcpu::decode_instruction;SVM_MAXIMUM_CODE1];
	let mut i:usize=INTERCEPTED_EXCEPTIONS as usize;
	while i<INTERCEPTED_EXCEPTIONS as usize+32
	{
		array[i]=SvmVcpu::decode_event;
		i+=1;
	}
	array[INTERCEPTED_INTERRUPT as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_NMI as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_SMI as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_INIT as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_VINTR as usize]=SvmVcpu::decode_event;
	array
};

const SVM_HOST_DECODE_HANDLER_GROUP2:[SvmHostDecodeHandler;SVM_MAXIMUM_CODE2]=
{
	let mut array:[SvmHostDecodeHandler;SVM_MAXIMUM_CODE2]=[SvmVcpu::decode_event;SVM_MAXIMUM_CODE2];
	array[(NESTED_PAGE_FAULT-0x400) as usize]=SvmVcpu::decode_npf;
	array
};

const SVM_HOST_DECODE_HANDLER_GROUP_NEGATIVE:[SvmHostDecodeHandler;SVM_MAXIMUM_NEGATIVE]=[SvmVcpu::decode_event;SVM_MAXIMUM_NEGATIVE];

const SVM_HOST_DECODE_HANDLER_GROUPS:[&[SvmHostDecodeHandler];SVM_MAXIMUM_GROUPS]=[&SVM_HOST_DECODE_HANDLER_GROUP1,&SVM_HOST_DECODE_HANDLER_GROUP2];

// This function is supposed to be time-sensitive!
// The O(1) method of dispatching handler.
// If we use match-expression, it could be O(n) if optimizer went dumb!
#[inline] pub fn dispatch_decoder(intercept_code:i64)->SvmHostDecodeHandler
{
	if intercept_code<0
	{
		let index:usize=!intercept_code as usize;
		if index<SVM_MAXIMUM_NEGATIVE
		{
			SVM_HOST_DECODE_HANDLER_GROUP_NEGATIVE[index]
		}
		else
		{
			SvmVcpu::decode_unknown
		}
	}
	else
	{
		let group:usize=(intercept_code as usize)>>10;
		if group<SVM_MAXIMUM_GROUPS
		{
			let index:usize=(intercept_code as usize)&0x3ff;
			if index<SVM_EXIT_HANDLER_GROUP_LIMITS[group]
			{
				SVM_HOST_DECODE_HANDLER_GROUPS[group][index]
			}
			else
			{
				SvmVcpu::decode_unknown
			}
		}
		else
		{
			SvmVcpu::decode_unknown
		}
	}
}