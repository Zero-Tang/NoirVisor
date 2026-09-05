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

use core::{hint::cold_path, sync::atomic::Ordering};

use super::*;
use exit::*;
use custom::SvmCustomVcpu;
use npt::NptFaultCode;
use disasm::emulator::{EmulatorOps, Instruction, MovCrInfo, MovDrInfo};
use xpf_core::x86::{crdr::*,paging::*};

impl EmulatorOps for SvmVcpu
{
	fn get_gpr(&self,gpr_index:usize)->u64
	{
		let stk_top=self.get_stack_top();
		stk_top.gpr_state.read(gpr_index).unwrap()
	}

	fn set_gpr(&mut self,gpr_index:usize,value:u64)
	{
		let stk_top=self.get_stack_top_mut();
		stk_top.gpr_state.write(gpr_index,value);
	}

	fn get_seg_selector(&self,seg_index:usize)->u16
	{
		unsafe
		{
			self.vmread(GUEST_ES_SELECTOR+(seg_index<<4))
		}
	}

	fn set_seg_selector(&mut self,seg_index:usize,value:u16)
	{
		unsafe
		{
			self.vmwrite(GUEST_ES_SELECTOR+(seg_index<<4),value);
			if seg_index<4
			{
				self.ref_clean_field_mut().set_seg(false);
			}
		}
	}

	fn get_rip(&self)->u64
	{
		self.read_rip()
	}

	fn get_cr0(&self)->Cr0
	{
		self.read_cr0()
	}

	fn get_cr3(&self)->u64
	{
		self.read_cr3()
	}

	fn get_cr4(&self)->Cr4
	{
		self.read_cr4()
	}

	fn get_efer(&self)->Efer
	{
		self.read_efer()
	}

	fn is_user_mode(&self)->bool
	{
		self.read_cpl()==3
	}

	fn read_gpa(&mut self,gpa:u64,value:&mut [u8])->usize
	{
		let hv:&mut SvmHypervisor=unsafe{&mut *self.hypervisor.cast()};
		if hv.mmio_space.dispatch_input(gpa,value,self as *mut Self as *mut c_void).is_err()
		{
			// Cannot dispatch MMIO input handler. Then simply pass-thru to system memory.
			let src=unsafe{slice::from_raw_parts(gpa as *const u8,value.len())};
			value.copy_from_slice(src);
		}
		value.len()
	}

	fn write_gpa(&mut self,gpa:u64,value:&[u8])->usize
	{
		let hv:&mut SvmHypervisor=unsafe{&mut *self.hypervisor.cast()};
		if hv.mmio_space.dispatch_output(gpa,value,self as *mut Self as *mut c_void).is_err()
		{
			// Cannot dispatch MMIO output handler. Then simply pass-thru to system memory.
			let dest=unsafe{slice::from_raw_parts_mut(gpa as *mut u8,value.len())};
			dest.copy_from_slice(value);
		}
		value.len()
	}
}

impl EmulatorOps for SvmCustomVcpu
{
	fn get_gpr(&self,gpr_index:usize)->u64
	{
		self.state.gpr.read(gpr_index).unwrap()
	}

	fn set_gpr(&mut self,gpr_index:usize,value:u64)
	{
		self.state.gpr.write(gpr_index,value);
	}

	fn get_seg_selector(&self,seg_index:usize)->u16
	{
		unsafe
		{
			self.vmread(GUEST_ES_SELECTOR+(seg_index<<4))
		}
	}

	fn set_seg_selector(&mut self,seg_index:usize,value:u16)
	{
		unsafe
		{
			self.vmwrite(GUEST_ES_SELECTOR+(seg_index<<4),value);
		}
	}

	fn get_rip(&self)->u64
	{
		self.read_rip()
	}

	fn get_cr0(&self)->Cr0
	{
		self.read_cr0()
	}

	fn get_cr3(&self)->u64
	{
		self.read_cr3()
	}

	fn get_cr4(&self)->Cr4
	{
		self.read_cr4()
	}

	fn get_efer(&self)->Efer
	{
		self.read_efer()
	}

	fn is_user_mode(&self)->bool
	{
		self.read_cpl()==3
	}

	fn read_gpa(&mut self,gpa:u64,value:&mut [u8])->usize
	{
		let mut offset:usize=0;
		while offset<value.len()
		{
			let len=self.read_gpa_single_page(gpa+offset as u64,&mut value[offset..]);
			if len==0
			{
				break;
			}
			offset+=len;
		}
		offset
	}

	fn write_gpa(&mut self,gpa:u64,value:&[u8])->usize
	{
		let mut offset:usize=0;
		while offset<value.len()
		{
			let len=self.write_gpa_single_page(gpa+offset as u64,&value[offset..]);
			if len==0
			{
				break;
			}
			offset+=len;
		}
		offset
	}
}

impl SvmCustomVcpu
{
	fn read_gpa_single_page(&mut self,gpa:u64,value:&mut [u8])->usize
	{
		let Some((hpa,_,_))=self.translate(gpa) else
		{
			return 0;
		};
		let copy_size=core::cmp::min(value.len(),PAGE_SIZE-page_offset(gpa) as usize);
		let src=unsafe{slice::from_raw_parts(hpa as *const u8,copy_size)};
		value[..copy_size].copy_from_slice(src);
		copy_size
	}

	fn write_gpa_single_page(&mut self,gpa:u64,value:&[u8])->usize
	{
		let Some((hpa,true,_))=self.translate(gpa) else
		{
			return 0;
		};
		let copy_size=core::cmp::min(value.len(),PAGE_SIZE-page_offset(gpa) as usize);
		let dest=unsafe{slice::from_raw_parts_mut(hpa as *mut u8,copy_size)};
		dest.copy_from_slice(&value[..copy_size]);
		copy_size
	}
}

pub(super) static DECODE_ASSIST_SUPPORT:AtomicBool=AtomicBool::new(false);
pub(super) static NEXT_RIP_SAVING_SUPPORT:AtomicBool=AtomicBool::new(false);

pub(super) trait SoftwareDecodeAssistOps:VmcbOps+EmulatorOps
{
	fn get_current_bitness(&self)->u32
	{
		if self.ref_efer().lma() && self.ref_cs().attrib.long_mode()	// The CS.L bit.
		{
			64
		}
		else if self.ref_cs().attrib.default_big()	// The CS.D bit.
		{
			32
		}
		else
		{
			16
		}
	}

	fn ins_mut(&mut self)->&mut Instruction;

	fn decode_unknown(&mut self)
	{
		// This interception means NoirVisor does not know such interception at all. So panic on interception.
		let exit_reason:i64=unsafe{self.vmread(EXIT_CODE)};
		panic!("Unknown interception decode request! Intercept Code: 0x{:016X}",exit_reason);
	}

	fn fetch_instruction(&mut self) where Self:Sized
	{
		let cs_base=self.ref_cs().base;
		let rip=self.read_rip();
		let buff=self.instruction_bytes_mut();
		if let Err((e,_))=self.read_virt(cs_base+rip,buff)
		{
			let cr3=self.get_cr3();
			panic!("Page-Fault is triggered by software while fetching instruction! Code: 0x{:X}, vmcb={:p} rip=0x{rip:016X}, cr3=0x{cr3:016X}",e.into_bits(),self.get_vmcb());
		}
		self.ins_mut().copy_from_slice(buff);
	}

	fn decode_instruction_internal(&mut self) where Self:Sized
	{
		let rip:u64=self.read_rip();
		// Fetch instructions.
		self.fetch_instruction();
		// Call disassembler.
		let bitness=self.get_current_bitness();
		self.ins_mut().decode(bitness);
		let len=self.ins_mut().len();
		if len>0
		{
			let mut nrip:u64=rip+len as u64;
			// If the vCPU is in compatibility mode, advancing rip should drop the higher 32 bits.
			if !(self.ref_efer().lma() && self.ref_cs().attrib.long_mode())
			{
				nrip&=u32::MAX as u64;
			}
			self.write_next_rip(nrip);
		}
	}

	fn decode_event(&mut self)
	{
		// This interception does not involve assisting decodings.
		// Event has no instruction length, so just do nothing.
	}

	fn decode_instruction(&mut self) where Self:Sized
	{
		// This interception does not involve assisting decodings.
		// However, it still helps if Next-RIP Saving is unsupported by the processor.
		if !NEXT_RIP_SAVING_SUPPORT.load(Ordering::Relaxed)
		{
			self.decode_instruction_internal();
		}
	}

	fn decode_cr(&mut self) where Self:Sized
	{
		if !DECODE_ASSIST_SUPPORT.load(Ordering::Relaxed)
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			self.decode_instruction_internal();
			if self.ins_mut().is_decoded()
			{
				// Then put the results back.
				let v=match self.ins_mut().decode_cr_access()
				{
					Some(info)=>
					{
						match info
						{
							MovCrInfo::MovToCr(_rd,rs)=>rs as u64|(1<<63),
							MovCrInfo::MovFromCr(rd,_rs)=>rd as u64|(1<<63),
							MovCrInfo::Lmsw|MovCrInfo::Smsw|MovCrInfo::Clts=>0
						}
					}
					None=>0
				};
				self.write_exit_info1(v);
			}
		}
	}

	fn decode_dr(&mut self) where Self:Sized
	{
		if !DECODE_ASSIST_SUPPORT.load(Ordering::Relaxed)
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			self.decode_instruction_internal();
			if self.ins_mut().is_decoded()
			{
				// Then put the results back.
				let v=match self.ins_mut().decode_dr_access()
				{
					Some(info)=>
					{
						match info
						{
							MovDrInfo::MovToDr(_rd,rs)=>rs as u64,
							MovDrInfo::MovFromDr(rd,_rs)=>rd as u64
						}
					}
					None=>0
				};
				self.write_exit_info1(v);
			}
		}
	}

	fn decode_pf(&mut self) where Self:Sized
	{
		if !DECODE_ASSIST_SUPPORT.load(Ordering::Relaxed)
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			let fault_code=PageFaultErrorCode::from_bits(unsafe{self.vmread(EXIT_INFO1)});
			if fault_code.execute()
			{
				// Fetching instruction is only needed if the operation is not instruction fetch!
				self.fetch_instruction();
			}
		}
	}

	fn decode_int(&mut self) where Self:Sized
	{
		if !DECODE_ASSIST_SUPPORT.load(Ordering::Relaxed)
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			self.decode_instruction_internal();
			if self.ins_mut().is_decoded()
			{
				// Then put the results back.
				let v=self.ins_mut().decode_swint().unwrap_or(0) as u64;
				self.write_exit_info1(v);
			}
		}
	}

	fn decode_invlpg(&mut self) where Self:Sized
	{
		if !DECODE_ASSIST_SUPPORT.load(Ordering::Relaxed)
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			// First, fetch the instruction.
			self.fetch_instruction();
			// Second, decode the instruction.
			self.decode_instruction_internal();
			if self.ins_mut().is_decoded() && let Some(info)=self.ins_mut().decode_invlpg()
			{
				let p=info.calc_addr(self);
				// Then put the results back.
				self.write_exit_info1(p);
			}
		}
	}

	fn decode_io(&mut self) where Self:Sized
	{
		// I/O instruction is a special case, since the next rip is saved in the EXIT_INFO2
		// field, even if the next-rip-saving feature is not supported by the processor.
		// Therefore, there is no need to fetch-and-decode the intercepted I/O instructions!
		if !NEXT_RIP_SAVING_SUPPORT.load(Ordering::Relaxed)
		{
			unsafe
			{
				let nrip:u64=self.vmread(EXIT_INFO2);
				self.write_next_rip(nrip);
			}
		}
	}

	fn decode_npf(&mut self) where Self:Sized
	{
		if !DECODE_ASSIST_SUPPORT.load(Ordering::Relaxed)
		{
			// In Linux KVM, Decode-Assists is not supported in nested virtualization.
			// We will have to emulate this on our own.
			let fault_code=NptFaultCode::from_bits(unsafe{self.vmread(EXIT_INFO1)});
			if !fault_code.code_fetch()
			{
				// Fetching instruction is only needed if the operation is not instruction fetch!
				self.fetch_instruction();
			}
		}
	}
}

const fn init_group1<T:SoftwareDecodeAssistOps>()->[fn(&mut T);SVM_MAXIMUM_CODE1]
{
	let mut array:[fn(&mut T);SVM_MAXIMUM_CODE1]=[T::decode_instruction;SVM_MAXIMUM_CODE1];
	let mut i:usize=INTERCEPTED_EXCEPTIONS as usize;
	while i<INTERCEPTED_EXCEPTIONS as usize+32
	{
		array[i]=T::decode_event;
		i+=1;
	}
	i=INTERCEPTED_CR0_READ as usize;
	while i<=INTERCEPTED_CR15_WRITE as usize
	{
		array[i]=T::decode_cr;
		i+=1;
	}
	i=INTERCEPTED_DR0_READ as usize;
	while i<=INTERCEPTED_DR15_WRITE as usize
	{
		array[i]=T::decode_dr;
		i+=1;
	}
	array[INTERCEPTED_PF_EXCEPTION as usize]=T::decode_pf;
	array[INTERCEPTED_INTERRUPT as usize]=T::decode_event;
	array[INTERCEPTED_NMI as usize]=T::decode_event;
	array[INTERCEPTED_SMI as usize]=T::decode_event;
	array[INTERCEPTED_INIT as usize]=T::decode_event;
	array[INTERCEPTED_VINTR as usize]=T::decode_event;
	array[INTERCEPTED_INT as usize]=T::decode_int;
	array[INTERCEPTED_INVLPG as usize]=T::decode_invlpg;
	array[INTERCEPTED_IO as usize]=T::decode_io;
	array
}

const fn init_group2<T:SoftwareDecodeAssistOps>()->[fn(&mut T);SVM_MAXIMUM_CODE2]
{
	let mut array:[fn(&mut T);SVM_MAXIMUM_CODE2]=[T::decode_event;SVM_MAXIMUM_CODE2];
	array[(NESTED_PAGE_FAULT-0x400) as usize]=T::decode_npf;
	array
}

const fn init_group_negative<T:SoftwareDecodeAssistOps>()->[fn(&mut T);SVM_MAXIMUM_NEGATIVE]
{
	[T::decode_event;SVM_MAXIMUM_NEGATIVE]
}

impl SoftwareDecodeAssistOps for SvmVcpu
{
	fn ins_mut(&mut self)->&mut Instruction
	{
		&mut self.decoded_instruction
	}
}

impl SoftwareDecodeAssistOps for SvmCustomVcpu
{
	fn ins_mut(&mut self)->&mut Instruction
	{
		&mut self.decoded_instruction
	}
}

type SvmHostDecodeHandler=fn (&mut SvmVcpu);
type SvmCvGuestDecodeHandler=fn (&mut SvmCustomVcpu);

static SVM_HOST_DECODE_HANDLER_GROUP1:[SvmHostDecodeHandler;SVM_MAXIMUM_CODE1]=init_group1();
static SVM_HOST_DECODE_HANDLER_GROUP2:[SvmHostDecodeHandler;SVM_MAXIMUM_CODE2]=init_group2();
static SVM_HOST_DECODE_HANDLER_GROUP_NEGATIVE:[SvmHostDecodeHandler;SVM_MAXIMUM_NEGATIVE]=init_group_negative();

static SVM_CVGUEST_DECODE_HANDLER_GROUP1:[SvmCvGuestDecodeHandler;SVM_MAXIMUM_CODE1]=init_group1();
static SVM_CVGUEST_DECODE_HANDLER_GROUP2:[SvmCvGuestDecodeHandler;SVM_MAXIMUM_CODE2]=init_group2();
static SVM_CVGUEST_DECODE_HANDLER_GROUP_NEGATIVE:[SvmCvGuestDecodeHandler;SVM_MAXIMUM_NEGATIVE]=init_group_negative();

static SVM_HOST_DECODE_HANDLER_GROUPS:[&[SvmHostDecodeHandler];SVM_MAXIMUM_GROUPS]=[&SVM_HOST_DECODE_HANDLER_GROUP1,&SVM_HOST_DECODE_HANDLER_GROUP2];
static SVM_CVGUEST_DECODE_HANDLER_GROUPS:[&[SvmCvGuestDecodeHandler];SVM_MAXIMUM_GROUPS]=[&SVM_CVGUEST_DECODE_HANDLER_GROUP1,&SVM_CVGUEST_DECODE_HANDLER_GROUP2];

#[inline(always)] fn dispatch_decoder_internal<T:SoftwareDecodeAssistOps>(positive:&[&[fn(&mut T)]],negative:&[fn(&mut T)],intercept_code:i64)->fn(&mut T)
{
	if intercept_code<0
	{
		cold_path();
		let index=!intercept_code as usize;
		negative.get(index).copied().unwrap_or(T::decode_unknown)
	}
	else
	{
		let group=(intercept_code as usize)>>10;
		match positive.get(group)
		{
			Some(&g)=>
			{
				let index=(intercept_code as usize)&0x3FF;
				g.get(index).copied().unwrap_or(T::decode_unknown)
			}
			None=>T::decode_unknown
		}
	}
}

// This function is supposed to be time-sensitive!
// The O(1) method of dispatching handler.
// If we use match-expression, it could be O(n) if optimizer went dumb!
#[inline] pub fn dispatch_decoder(intercept_code:i64)->SvmHostDecodeHandler
{
	dispatch_decoder_internal(&SVM_HOST_DECODE_HANDLER_GROUPS,&SVM_HOST_DECODE_HANDLER_GROUP_NEGATIVE,intercept_code)
}

#[inline] pub fn dispatch_cvexit_decoder(intercept_code:i64)->SvmCvGuestDecodeHandler
{
	dispatch_decoder_internal(&SVM_CVGUEST_DECODE_HANDLER_GROUPS,&SVM_CVGUEST_DECODE_HANDLER_GROUP_NEGATIVE,intercept_code)
}