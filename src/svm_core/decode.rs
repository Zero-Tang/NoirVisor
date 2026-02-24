/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file implements Decode-Assists and Next-Rip-Saving in AMD-V of
 * NoirVisor Core in Rust if not supported by the processor.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::slice;
use paste::paste;

use crate::disasm::emulator::{EmulatorOps, Instruction, MovCrInfo, MovDrInfo};
use exit::*;
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

impl EmulatorOps for SvmVcpu
{
	fn read_gpr(&self,gpr_index:usize)->u64
	{
		let stk_top=self.get_stack_top();
		stk_top.gpr_state.read(gpr_index).unwrap()
	}

	fn write_gpr(&mut self,gpr_index:usize,value:u64)
	{
		let stk_top=self.get_stack_top_mut();
		stk_top.gpr_state.write(gpr_index,value);
	}

	fn read_seg_selector(&self,seg_index:usize)->u16
	{
		unsafe
		{
			vmread(self.vmcb.virt,GUEST_ES_SELECTOR+(seg_index<<4))
		}
	}

	fn write_seg_selector(&self,seg_index:usize,value:u16)
	{
		unsafe
		{
			vmwrite(self.vmcb.virt,GUEST_ES_SELECTOR+(seg_index<<4),value);
			if seg_index<4
			{
				vmcb_clean_seg(self.vmcb.virt);
			}
		}
	}

	fn read_rip(&self)->u64
	{
		unsafe
		{
			vmread(self.vmcb.virt,GUEST_RIP)
		}
	}

	fn read_gpa(&mut self,gpa:u64,value:&mut [u8])
	{
		let hv:&mut SvmHypervisor=unsafe{&mut *self.hypervisor.cast()};
		let _=hv.mmio_space.dispatch_input(gpa,value.len() as u64,value.as_mut_ptr().cast(),self as *mut Self as *mut c_void);
	}

	fn write_gpa(&mut self,gpa:u64,value:&[u8])
	{
		let hv:&mut SvmHypervisor=unsafe{&mut *self.hypervisor.cast()};
		let _=hv.mmio_space.dispatch_output(gpa,value.len() as u64,value.as_ptr().cast(),self as *mut Self as *mut c_void);
	}
}

impl SvmVcpu
{
	pub(super) fn get_current_bitness(&self)->u32
	{
		unsafe
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
		}
	}

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
			let cr3=self.get_cr3();
			panic!("Page-Fault is triggered by software while fetching instruction! Code: 0x{:08X}, vmcb=0x{:X} rip=0x{rip:016X}, cr3=0x{cr3:016X}",e.into_bits(),self.vmcb.phys);
		}
	}

	fn decode_instruction_internal(&mut self)->Option<Instruction>
	{
		let rip:u64=unsafe{vmread(self.vmcb.virt,GUEST_RIP)};
		let buff:&mut [u8;15]=unsafe{&mut *self.vmcb.virt.byte_add(GUEST_INSTRUCTION_BYTES).cast()};
		// Fetch instructions.
		self.fetch_instruction();
		// Call disassembler.
		let mut ins=Instruction::new(*buff);
		ins.decode(self.get_current_bitness());
		if ins.len()>0
		{
			let mut nrip:u64=rip+ins.len() as u64;
			// If the vCPU is in compatibility mode, advancing rip should drop the higher 32 bits.
			unsafe
			{
				if !vmcb_bt32(self.vmcb.virt,GUEST_CS_ATTRIB,9)
				{
					nrip&=u32::MAX as u64;
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
		if !self.svm_feats.nrips()
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
		if !self.svm_feats.decode_assists()
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			if let Some(ins)=self.decode_instruction_internal()
			{
				// Then put the results back.
				let v=match ins.decode_cr_access()
				{
					Some(info)=>
					{
						match info
						{
							MovCrInfo::MovToCr(rd,_rs)=>rd as u64|(1<<63),
							MovCrInfo::MovFromCr(_rd,rs)=>rs as u64|(1<<63),
							MovCrInfo::Lmsw|MovCrInfo::Smsw|MovCrInfo::Clts=>0
						}
					}
					None=>0
				};
				unsafe
				{
					vmwrite(self.vmcb.virt,EXIT_INFO1,v);
				}
			}
		}
	}

	fn decode_dr(&mut self)
	{
		if !self.svm_feats.decode_assists()
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			if let Some(ins)=self.decode_instruction_internal()
			{
				// Then put the results back.
				let v=match ins.decode_dr_access()
				{
					Some(info)=>
					{
						match info
						{
							MovDrInfo::MovToDr(rd,_rs)=>rd as u64,
							MovDrInfo::MovFromDr(_rd,rs)=>rs as u64
						}
					}
					None=>0
				};
				unsafe
				{
					vmwrite(self.vmcb.virt,EXIT_INFO1,v);
				}
			}
		}
	}

	fn decode_pf(&mut self)
	{
		if !self.svm_feats.decode_assists()
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			let fault_code=PageFaultErrorCode::from_bits(unsafe{vmread(self.vmcb.virt,EXIT_INFO1)});
			if fault_code.execute()
			{
				// Fetching instruction is only needed if the operation is not instruction fetch!
				self.fetch_instruction();
			}
		}
	}

	fn decode_int(&mut self)
	{
		if !self.svm_feats.decode_assists()
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			if let Some(ins)=self.decode_instruction_internal()
			{
				// Then put the results back.
				let v=ins.decode_swint().unwrap_or(0) as u64;
				unsafe
				{
					vmwrite(self.vmcb.virt,EXIT_INFO1,v);
				}
			}
		}
	}

	fn decode_invlpg(&mut self)
	{
		if !self.svm_feats.decode_assists()
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			if let Some(ins)=self.decode_instruction_internal() && let Some(info)=ins.decode_invlpg()
			{
				let p=info.calc_addr(self);
				// Then put the results back.
				unsafe
				{
					vmwrite(self.vmcb.virt,EXIT_INFO1,p);
				}
			}
		}
	}

	fn decode_io(&mut self)
	{
		// I/O instruction is a special case, since the next rip is saved in the EXIT_INFO2
		// field, even if the next-rip-saving feature is not supported by the processor.
		// Therefore, there is no need to fetch-and-decode the intercepted I/O instructions!
		if !self.svm_feats.nrips()
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
		if !self.svm_feats.decode_assists()
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			let fault_code=NptFaultCode::from_bits(unsafe{vmread(self.vmcb.virt,EXIT_INFO1)});
			if !fault_code.code_fetch()
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
		match SVM_HOST_DECODE_HANDLER_GROUP_NEGATIVE.get(index)
		{
			Some(&h)=>h,
			None=>SvmVcpu::decode_unknown
		}
	}
	else
	{
		let group:usize=(intercept_code as usize)>>10;
		match SVM_HOST_DECODE_HANDLER_GROUPS.get(group)
		{
			Some(&g)=>
			{
				let index:usize=(intercept_code as usize)&0x3ff;
				match g.get(index)
				{
					Some(h)=>*h,
					None=>SvmVcpu::decode_unknown
				}
			}
			None=>SvmVcpu::decode_unknown
		}
	}
}