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
use iced_x86::{Code, Decoder, DecoderOptions, Instruction, Mnemonic};
use paste::paste;
use npt::NptFaultCode;

use super::{xpf_core::x86::paging::*,svm_core::*};

macro_rules! build_get_reg_helper
{
	($name:tt) =>
	{
		paste!
		{
			#[inline] fn [<get_ $name:lower>](&self)->u64
			{
				unsafe
				{
					vmread(self.vmcb.virt,[<GUEST_ $name:upper>])
				}
			}
		}
	};
}

impl PageTranslationHelper for SvmVcpu
{
	build_get_reg_helper!(cr0);
	build_get_reg_helper!(cr3);
	build_get_reg_helper!(cr4);
	build_get_reg_helper!(efer);

	fn is_user_mode(&self)->bool
	{
		let cpl:u8=unsafe{vmread(self.vmcb.virt,GUEST_CPL)};
		cpl==3
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

	fn fetch_instruction(&mut self)
	{
		let rip:u64=unsafe{vmread(self.vmcb.virt,GUEST_RIP)};
		let buff=unsafe{slice::from_raw_parts_mut(self.vmcb.virt.cast::<u8>().add(GUEST_INSTRUCTION_BYTES),15)};
		let mut fault_pa:Option<u64>=None;
		if let Err(e)=read_virtual_address(rip,self,buff,&mut fault_pa)
		{
			panic!("Page-Fault is triggered by software while fetching instruction! Code: 0x{:08X}",e.0);
		}
	}

	fn decode_instruction_internal(&mut self)->Option<Instruction>
	{
		let rip:u64=unsafe{vmread(self.vmcb.virt,GUEST_RIP)};
		let buff=unsafe{slice::from_raw_parts_mut(self.vmcb.virt.cast::<u8>().add(GUEST_INSTRUCTION_BYTES),15)};
		// Fetch instructions.
		self.fetch_instruction();
		// Check bitness.
		let bitness:u32=unsafe
		{
			if vmcb_bt32(self.vmcb.virt,GUEST_CS_ATTRIB,9)	// The CS.L bit.
			{
				64
			}
			else if vmcb_bt32(self.vmcb.virt,GUEST_CS_ATTRIB,10)	// The CS.D bit.
			{
				32
			}
			else
			{
				16
			}
		};
		// Call disassembler.
		let mut decoder=Decoder::with_ip(bitness,buff,rip,DecoderOptions::AMD);
		if decoder.can_decode()
		{
			let ins=decoder.decode();
			let mut nrip:u64=rip+ins.len() as u64;
			// If the vCPU is in compatibility mode, advancing rip should drop the higher 32 bits.
			unsafe
			{
				if !vmcb_bt32(self.vmcb.virt,GUEST_CS_ATTRIB,9)
				{
					nrip&=0xFFFFFFFF;
				}
				vmwrite(self.vmcb.virt,NEXT_RIP,nrip);
			}
			return Some(ins);
		}
		None
	}

	fn decode_instruction(&mut self)
	{
		// This interception does not involve assisting decodings.
		// However, it still helps if Next-RIP Saving is unsupported by the processor.
		if !self.nrip_saving
		{
			self.decode_instruction_internal();
		}
	}

	fn decode_event(&mut self)
	{
		// This interception does not involve assisting decodings.
		// Event has no instruction length, so just do nothing.
	}

	fn decode_cr(&mut self)
	{
		if !self.decode_assists
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			if let Some(ins)=self.decode_instruction_internal()
			{
				// Then put the results back.
				let ins_kind=ins.mnemonic();
				match ins_kind
				{
					Mnemonic::Mov=>
					{
						let ins_code=ins.code();
						match ins_code
						{
							// According to AMD64 manual, the highest bit of EXITINFO1 should be set if this is a mov-crx instruction.
							Code::Mov_cr_r32|Code::Mov_cr_r64=>unsafe{vmwrite(self.vmcb.virt,EXIT_INFO1,(ins.op1_register().number() as u64)|0x8000000000000000)},
							Code::Mov_r32_cr|Code::Mov_r64_cr=>unsafe{vmwrite(self.vmcb.virt,EXIT_INFO1,(ins.op0_register().number() as u64)|0x8000000000000000)},
							_=>panic!("Unexpected instruction code {:?}!",ins_code)
						}
					}
					// There are additional instructions which can access control registers!
					// According to AMD64 manual, nothing will be reported if these instructions are lmsw, smsw or clts.
					Mnemonic::Lmsw|Mnemonic::Smsw|Mnemonic::Clts=>unsafe{vmwrite::<u64>(self.vmcb.virt,EXIT_INFO1,0)},
					_=>panic!("Unexpected instruction mnemonic {:?}!",ins_kind)
				}
			}
		}
	}

	fn decode_dr(&mut self)
	{
		if !self.decode_assists
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			if let Some(ins)=self.decode_instruction_internal()
			{
				// Then put the results back.
				let ins_kind=ins.mnemonic();
				match ins_kind
				{
					Mnemonic::Mov=>
					{
						let ins_code=ins.code();
						match ins_code
						{
							Code::Mov_dr_r32|Code::Mov_dr_r64=>unsafe{vmwrite::<u64>(self.vmcb.virt,EXIT_INFO1,ins.op1_register().number() as u64)},
							Code::Mov_r32_dr|Code::Mov_r64_dr=>unsafe{vmwrite::<u64>(self.vmcb.virt,EXIT_INFO1,ins.op0_register().number() as u64)},
							_=>panic!("Unexpected instruction code {:?}!",ins_code)
						}
					}
					_=>panic!("Unexpected instruction mnemonic {:?}!",ins_kind)
				}
			}
		}
	}

	fn decode_pf(&mut self)
	{
		if !self.decode_assists
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			let fault_code=PageFaultErrorCode::from_u32(unsafe{vmread(self.vmcb.virt,EXIT_INFO1)});
			if fault_code.is_execute()
			{
				// Fetching instruction is only needed if the operation is not instruction fetch!
				self.fetch_instruction();
			}
		}
	}

	fn decode_int(&mut self)
	{
		if !self.decode_assists
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			if let Some(ins)=self.decode_instruction_internal()
			{
				// Then put the results back.
				let ins_kind=ins.mnemonic();
				match ins_kind
				{
					Mnemonic::Int=>unsafe{vmwrite(self.vmcb.virt,EXIT_INFO1,ins.immediate8() as u64)},
					_=>panic!("Unexpected instruction mnemonic {:?}!",ins_kind)
				}
			}
		}
	}

	fn decode_invlpg(&mut self)
	{
		if !self.decode_assists
		{
			// FIXME: load registers to obtain the target address.
			todo!("Software-emulated decode-assists for invlpg is not supported yet!");
		}
	}

	fn decode_io(&mut self)
	{
		// I/O instruction is a special case, since the next rip is saved in the EXIT_INFO2
		// field, even if the next-rip-saving feature is not supported by the processor.
		// Therefore, there is no need to fetch-and-decode the intercepted I/O instructions!
		if !self.nrip_saving
		{
			unsafe
			{
				let nrip:u64=vmread(self.vmcb.virt,EXIT_INFO2);
				vmwrite(self.vmcb.virt,NEXT_RIP,nrip);
			}
		}
	}

	fn decode_npf(&mut self)
	{
		if !self.decode_assists
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			let fault_code=NptFaultCode::from_u64(unsafe{vmread(self.vmcb.virt,EXIT_INFO1)});
			if !fault_code.is_code_read()
			{
				// Fetching instruction is only needed if the operation is not instruction fetch!
				self.fetch_instruction();
			}
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
	i=INTERCEPTED_CR0_READ as usize;
	while i<=INTERCEPTED_CR15_WRITE as usize
	{
		array[i]=SvmVcpu::decode_cr;
		i+=1;
	}
	i=INTERCEPTED_DR0_READ as usize;
	while i<=INTERCEPTED_DR15_WRITE as usize
	{
		array[i]=SvmVcpu::decode_dr;
		i+=1;
	}
	array[INTERCEPTED_PF_EXCEPTION as usize]=SvmVcpu::decode_pf;
	array[INTERCEPTED_INTERRUPT as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_NMI as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_SMI as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_INIT as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_VINTR as usize]=SvmVcpu::decode_event;
	array[INTERCEPTED_INT as usize]=SvmVcpu::decode_int;
	array[INTERCEPTED_INVLPG as usize]=SvmVcpu::decode_invlpg;
	array[INTERCEPTED_IO as usize]=SvmVcpu::decode_io;
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