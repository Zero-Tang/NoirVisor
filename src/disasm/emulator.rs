/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file decodes and emulates instructions via disassemblers.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).
*/

use core::{fmt, mem::MaybeUninit};

use static_collections::string::StaticString;
use yaxpeax_arch::{display::DisplaySink, LengthedInstruction};
use yaxpeax_x86::{long_mode, protected_mode, real_mode};
use log::{error, warn};
use paste::paste;

#[derive(Default)]
pub enum DecodeResult
{
	Real(real_mode::Instruction),
	Protected(protected_mode::Instruction),
	Long(long_mode::Instruction),
	#[default]
	Undecoded
}

#[derive(Default)]
pub struct Instruction
{
	pub instruction_bytes:[u8;15],
	decode_result:DecodeResult
}

macro_rules! build_decoder_fn
{
	($bitness:tt,$name:tt)=>
	{
		paste!
		{
			pub fn [<decode $bitness>](&mut self)
			{
				let decoder=[<$name:lower _mode>]::InstDecoder::default();
				self.decode_result=match decoder.decode_slice(&self.instruction_bytes)
				{
					Ok(ins)=>DecodeResult::[<$name>](ins),
					Err(e)=>
					{
						error!("Failed to decode real-mode instruction: {:02X?}! Reason: {e}",self.instruction_bytes);
						DecodeResult::Undecoded
					}
				}
			}
		}
	};
}

impl Instruction
{
	build_decoder_fn!(16,Real);
	build_decoder_fn!(32,Protected);
	build_decoder_fn!(64,Long);

	#[inline(always)] pub fn decode(&mut self,bitness:u32)
	{
		match bitness
		{
			16=>self.decode16(),
			32=>self.decode32(),
			64=>self.decode64(),
			_=>error!("Unexpected decode request for {bitness}-bit!")
		}
	}

	#[inline(always)] pub const fn new(instruction_bytes:[u8;15])->Self
	{
		Self
		{
			instruction_bytes,
			decode_result:DecodeResult::Undecoded
		}
	}

	#[inline(always)] pub fn len(&self)->usize
	{
		match self.decode_result
		{
			DecodeResult::Real(ins)=>ins.len().to_const() as usize,
			DecodeResult::Protected(ins)=>ins.len().to_const() as usize,
			DecodeResult::Long(ins)=>ins.len().to_const() as usize,
			DecodeResult::Undecoded=>0
		}
	}

	#[inline(always)] pub fn copy_from_slice(&mut self,instruction_bytes:&[u8])
	{
		let len=if instruction_bytes.len()>15 {15} else {instruction_bytes.len()};
		self.instruction_bytes[..len].copy_from_slice(instruction_bytes);
	}

	/// Decode Control Register Access.
	/// Returns `None` if this is not an access to control register.
	pub fn decode_cr_access(&self)->Option<MovCrInfo>
	{
		match self.decode_result
		{
			DecodeResult::Long(ins)=>ins.decode_cr(),
			DecodeResult::Protected(ins)=>ins.decode_cr(),
			DecodeResult::Real(ins)=>ins.decode_cr(),
			DecodeResult::Undecoded=>None
		}
	}

	/// Decode Debug Register Access.
	/// Returns `None` if this is not an access to debug register.
	pub fn decode_dr_access(&self)->Option<MovDrInfo>
	{
		match self.decode_result
		{
			DecodeResult::Long(ins)=>ins.decode_dr(),
			DecodeResult::Protected(ins)=>ins.decode_dr(),
			DecodeResult::Real(ins)=>ins.decode_dr(),
			DecodeResult::Undecoded=>None
		}
	}

	/// Decode Software Interrupt Instruction.
	/// Returns `None` if this is not a software interrupt instruction.
	pub fn decode_swint(&self)->Option<u8>
	{
		match self.decode_result
		{
			DecodeResult::Long(ins)=>ins.decode_int(),
			DecodeResult::Protected(ins)=>ins.decode_int(),
			DecodeResult::Real(ins)=>ins.decode_int(),
			DecodeResult::Undecoded=>None
		}
	}

	/// Decode TLB invalidation instruction.
	/// Returns `None` if this is not an access to debug register.
	pub fn decode_invlpg(&self)->Option<InvlpgInfo>
	{
		match self.decode_result
		{
			DecodeResult::Long(ins)=>ins.decode_invlpg_instruction(),
			DecodeResult::Protected(ins)=>ins.decode_invlpg_instruction(),
			DecodeResult::Real(ins)=>ins.decode_invlpg_instruction(),
			DecodeResult::Undecoded=>None
		}
	}

	pub fn decode_mmio(&self)->Option<MmioInstruction>
	{
		match self.decode_result
		{
			DecodeResult::Long(ins)=>ins.decode_mmio(),
			DecodeResult::Protected(ins)=>ins.decode_mmio(),
			DecodeResult::Real(ins)=>ins.decode_mmio(),
			DecodeResult::Undecoded=>None
		}
	}
}

impl fmt::Display for Instruction
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		let mut w=InstructionMnemonic::default();
		match self.decode_result
		{
			DecodeResult::Long(ins)=>ins.display_into(&mut w),
			DecodeResult::Protected(ins)=>ins.display_into(&mut w),
			DecodeResult::Real(ins)=>ins.display_into(&mut w),
			DecodeResult::Undecoded=>write!(f,"undecoded: {:02X?}",self.instruction_bytes)
		}?;
		f.write_str(w.0.as_str())
	}
}

pub enum MmioInstruction
{
	MovWithGpr8(u8),
	MovWithGpr8Hi(u8),
	MovWithGpr16(u8),
	MovWithGpr32(u8),
	MovWithGpr64(u8),
	MovWithSeg(u8),
	MovFromImm8(u8),
	MovFromImm16(u16),
	MovFromImm32(u32),
	MovFromImm64(u64)
}

impl MmioInstruction
{
	pub fn len(&self)->usize
	{
		match self
		{
			Self::MovFromImm8(_)|Self::MovWithGpr8(_)|Self::MovWithGpr8Hi(_)=>1,
			Self::MovFromImm16(_)|Self::MovWithGpr16(_)|Self::MovWithSeg(_)=>2,
			Self::MovFromImm32(_)|Self::MovWithGpr32(_)=>4,
			Self::MovFromImm64(_)|Self::MovWithGpr64(_)=>8
		}
	}
}

trait MmioDecoder
{
	fn decode_mmio(&self)->Option<MmioInstruction>;
}

macro_rules! derive_mmio_decoder
{
	($mode:tt)=>
	{
		paste!
		{
			impl MmioDecoder for [<$mode _mode>]::Instruction
			{
				fn decode_mmio(&self)->Option<MmioInstruction>
				{
					use [<$mode _mode>]::{Opcode,Operand,register_class};
					match self.opcode()
					{
						Opcode::MOV=>
						{
							let dest_is_mem=self.operand(0).is_memory();
							let op_index=if dest_is_mem {1} else {0};
							match self.operand(op_index)
							{
								Operand::ImmediateI8{imm}=>Some(MmioInstruction::MovFromImm8(imm as u8)),
								Operand::ImmediateI16{imm}=>Some(MmioInstruction::MovFromImm16(imm as u16)),
								Operand::ImmediateI32{imm}=>Some(MmioInstruction::MovFromImm32(imm as u32)),
								Operand::Register{reg}=>
								{
									let n=reg.num();
									match reg.class()
									{
										register_class::B=>match n
										{
											0..4=>Some(MmioInstruction::MovWithGpr8(n)),
											4..8=>Some(MmioInstruction::MovWithGpr8Hi(n-4)),
											_=>None
										}
										register_class::W=>Some(MmioInstruction::MovWithGpr16(n)),
										register_class::D=>Some(MmioInstruction::MovWithGpr32(n)),
										register_class::S=>Some(MmioInstruction::MovWithSeg(n)),
										_=>None
									}
								}
								_=>None
							}
						}
						_=>
						{
							error!("Unexpected {}-mode MMIO instruction!",stringify!(long));
							None
						}
					}
				}
			}
		}
	};
}

derive_mmio_decoder!(protected);
derive_mmio_decoder!(real);

impl MmioDecoder for long_mode::Instruction
{
	fn decode_mmio(&self)->Option<MmioInstruction>
	{
		use long_mode::{Opcode,Operand,register_class};
		match self.opcode()
		{
			Opcode::MOV=>
			{
				let op_index=if self.operand(0).is_memory() {1} else {0};
				match self.operand(op_index)
				{
					Operand::ImmediateI8{imm}=>Some(MmioInstruction::MovFromImm8(imm as u8)),
					Operand::ImmediateI16{imm}=>Some(MmioInstruction::MovFromImm16(imm as u16)),
					Operand::ImmediateI32{imm}=>Some(MmioInstruction::MovFromImm32(imm as u32)),
					Operand::ImmediateI64{imm}=>Some(MmioInstruction::MovFromImm64(imm as u64)),
					Operand::Register{reg}=>
					{
						let n=reg.num();
						match reg.class()
						{
							register_class::B=>match n
							{
								0..4=>Some(MmioInstruction::MovWithGpr8(n)),
								4..8=>Some(MmioInstruction::MovWithGpr8Hi(n-4)),
								_=>None
							}
							register_class::RB=>Some(MmioInstruction::MovWithGpr8(n)),
							register_class::W=>Some(MmioInstruction::MovWithGpr16(n)),
							register_class::D=>Some(MmioInstruction::MovWithGpr32(n)),
							register_class::Q=>Some(MmioInstruction::MovWithGpr64(n)),
							register_class::S=>Some(MmioInstruction::MovWithSeg(n)),
							_=>None
						}
					}
					_=>None
				}
			}
			_=>
			{
				error!("Unexpected {}-mode MMIO instruction!",stringify!(long));
				None
			}
		}
	}
}

trait MovCrDrDecoder
{
	fn decode_cr(&self)->Option<MovCrInfo>;
	fn decode_dr(&self)->Option<MovDrInfo>;
}

macro_rules! derive_crdr_decoder
{
	($mode:tt) =>
	{
		paste!
		{
			impl MovCrDrDecoder for [<$mode _mode>]::Instruction
			{
				fn decode_cr(&self)->Option<MovCrInfo>
				{
					use [<$mode _mode>]::{Opcode,Operand,register_class};
					match self.opcode()
					{
						Opcode::MOV=>
						{
							let rd=match self.operand(0)
							{
								Operand::Register{reg}=>Some(reg),
								_=>None
							}?;
							let rs=match self.operand(1)
							{
								Operand::Register{reg}=>Some(reg),
								_=>None
							}?;
							if rd.class()==register_class::CR
							{
								Some(MovCrInfo::MovToCr(rd.num(),rs.num()))
							}
							else
							{
								Some(MovCrInfo::MovFromCr(rd.num(),rs.num()))
							}
						}
						Opcode::SMSW=>Some(MovCrInfo::Smsw),
						Opcode::LMSW=>Some(MovCrInfo::Lmsw),
						Opcode::CLTS=>Some(MovCrInfo::Clts),
						_=>None
					}
				}
				
				fn decode_dr(&self)->Option<MovDrInfo>
				{
					use [<$mode _mode>]::{Opcode,Operand,register_class};
					if self.opcode()==Opcode::MOV
					{
						let rd=match self.operand(0)
						{
							Operand::Register{reg}=>Some(reg),
							_=>None
						}?;
						let rs=match self.operand(1)
						{
							Operand::Register{reg}=>Some(reg),
							_=>None
						}?;
						if rd.class()==register_class::DR
						{
							Some(MovDrInfo::MovToDr(rd.num(),rs.num()))
						}
						else
						{
							Some(MovDrInfo::MovFromDr(rd.num(),rs.num()))
						}
					}
					else
					{
						None
					}
				}
			}
		}
	};
}

derive_crdr_decoder!(long);
derive_crdr_decoder!(protected);
derive_crdr_decoder!(real);

trait SwIntDecoder
{
	fn decode_int(&self)->Option<u8>;
}

macro_rules! derive_swint_decoder
{
	($mode:tt)=>
	{
		paste!
		{
			impl SwIntDecoder for [<$mode _mode>]::Instruction
			{
				fn decode_int(&self)->Option<u8>
				{
					use [<$mode _mode>]::{Opcode,Operand};
					if self.opcode()==Opcode::INT
					{
						match self.operand(0)
						{
							Operand::ImmediateU8{imm}=>Some(imm),
							_=>None
						}
					}
					else
					{
						None
					}
				}
			}
		}
	};
}

derive_swint_decoder!(long);
derive_swint_decoder!(protected);
derive_swint_decoder!(real);

trait InvlpgDecoder
{
	fn decode_invlpg_instruction(&self)->Option<InvlpgInfo>;
}

impl InvlpgDecoder for long_mode::Instruction
{
	fn decode_invlpg_instruction(&self)->Option<InvlpgInfo>
	{
		use long_mode::{Opcode,Operand};
		if self.opcode()==Opcode::INVLPG
		{
			match self.operand(0)
			{
				Operand::MemDeref{base}=>Some(InvlpgInfo::new_long(Some(base),None,0,base.width())),
				Operand::Disp{base,disp}=>Some(InvlpgInfo::new_long(Some(base),None,disp,base.width())),
				Operand::MemBaseIndexScale{base,index,scale}=>Some(InvlpgInfo::new_long(Some(base),Some((index,scale)),0,base.width())),
				Operand::MemBaseIndexScaleDisp{base,index,scale,disp}=>Some(InvlpgInfo::new_long(Some(base),Some((index,scale)),disp,base.width())),
				Operand::MemIndexScale{index,scale}=>Some(InvlpgInfo::new_long(None,Some((index,scale)),0,index.width())),
				Operand::MemIndexScaleDisp{index,scale,disp}=>Some(InvlpgInfo::new_long(None,Some((index,scale)),disp,index.width())),
				Operand::AbsoluteU32{addr}=>Some(InvlpgInfo::new_long(None,None,addr.cast_signed(),8)),
				_=>None
			}
		}
		else
		{
			None
		}
	}
}

impl InvlpgDecoder for protected_mode::Instruction
{
	fn decode_invlpg_instruction(&self)->Option<InvlpgInfo>
	{
		use protected_mode::{Opcode,Operand};
		if self.opcode()==Opcode::INVLPG
		{
			match self.operand(0)
			{
				Operand::MemDeref{base}=>Some(InvlpgInfo::new_protected(Some(base),None,0,base.width())),
				Operand::Disp{base,disp}=>Some(InvlpgInfo::new_protected(Some(base),None,disp,base.width())),
				Operand::MemBaseIndexScale{base,index,scale}=>Some(InvlpgInfo::new_protected(Some(base),Some((index,scale)),0,base.width())),
				Operand::MemBaseIndexScaleDisp{base,index,scale,disp}=>Some(InvlpgInfo::new_protected(Some(base),Some((index,scale)),disp,base.width())),
				Operand::MemIndexScale{index,scale}=>Some(InvlpgInfo::new_protected(None,Some((index,scale)),0,index.width())),
				Operand::MemIndexScaleDisp{index,scale,disp}=>Some(InvlpgInfo::new_protected(None,Some((index,scale)),disp,index.width())),
				Operand::AbsoluteU32{addr}=>Some(InvlpgInfo::new_long(None,None,addr.cast_signed(),4)),
				_=>None
			}
		}
		else
		{
			None
		}
	}
}

impl InvlpgDecoder for real_mode::Instruction
{
	fn decode_invlpg_instruction(&self)->Option<InvlpgInfo>
	{
		use real_mode::{Opcode,Operand};
		if self.opcode()==Opcode::INVLPG
		{
			match self.operand(0)
			{
				Operand::MemDeref{base}=>Some(InvlpgInfo::new_real(Some(base),None,0,base.width())),
				Operand::MemBaseIndexScale{base,index,scale}=>Some(InvlpgInfo::new_real(Some(base),Some((index,scale)),0,base.width())),
				Operand::MemBaseIndexScaleDisp{base,index,scale,disp}=>Some(InvlpgInfo::new_real(Some(base),Some((index,scale)),disp,base.width())),
				Operand::MemIndexScale{index,scale}=>Some(InvlpgInfo::new_real(None,Some((index,scale)),0,index.width())),
				Operand::MemIndexScaleDisp{index,scale,disp}=>Some(InvlpgInfo::new_real(None,Some((index,scale)),disp,index.width())),
				_=>None
			}
		}
		else
		{
			None
		}
	}
}

pub enum MovCrInfo
{
	MovToCr(u8,u8),
	MovFromCr(u8,u8),
	Smsw,
	Lmsw,
	Clts,
}

pub enum MovDrInfo
{
	MovToDr(u8,u8),
	MovFromDr(u8,u8)
}

pub struct InvlpgInfo
{
	base:Option<u8>,
	index_scale:Option<(u8,u8)>,
	addr_size:u8,
	disp:i32
}

macro_rules! derive_invlpg_info_new
{
	($mode:tt) =>
	{
		paste!
		{
			fn [<new_ $mode>](base:Option<[<$mode _mode>]::RegSpec>,index_scale:Option<([<$mode _mode>]::RegSpec,u8)>,disp:i32,addr_size:u8)->Self
			{
				Self
				{
					base:base.map(|reg| reg.num()),
					index_scale:index_scale.map(|is| (is.0.num(),is.1)),
					addr_size,
					disp
				}
			}
		}
	};
}

impl InvlpgInfo
{
	derive_invlpg_info_new!(real);
	derive_invlpg_info_new!(protected);

	fn new_long(base:Option<long_mode::RegSpec>,index_scale:Option<(long_mode::RegSpec,u8)>,disp:i32,addr_size:u8)->Self
	{
		use long_mode::register_class;
		Self
		{
			base:base.map(|reg| if reg.class()==register_class::EIP || reg.class()==register_class::RIP {16} else {reg.num()}),
			index_scale:index_scale.map(|is| (is.0.num(),is.1)),
			addr_size,
			disp
		}
	}

	pub fn calc_addr(&self,vcpu:&impl EmulatorOps)->u64
	{
		let mut p:u64=0;
		let mask=8u64.wrapping_shl(self.addr_size as u32).wrapping_sub(1);
		if let Some(base)=self.base
		{
			p+=if base==16
			{
				vcpu.read_rip()
			}
			else
			{
				vcpu.read_gpr(base as usize)
			};
		}
		if let Some((index,scale))=self.index_scale
		{
			p+=vcpu.read_gpr(index as usize)*scale as u64;
		}
		p+=p.wrapping_add_signed(self.disp as i64);
		p&=mask;
		p
	}
}

#[derive(Default)]
pub struct InstructionMnemonic(StaticString<64>);

impl fmt::Write for InstructionMnemonic
{
	fn write_str(&mut self, s: &str) -> fmt::Result
	{
		self.0.write_str(s)
	}
}

impl DisplaySink for InstructionMnemonic {}

pub trait EmulatorOps
{
	fn read_gpr(&self,gpr_index:usize)->u64;
	fn write_gpr(&mut self,gpr_index:usize,value:u64);
	fn read_seg_selector(&self,seg_index:usize)->u16;
	fn write_seg_selector(&self,seg_index:usize,value:u16);
	fn read_rip(&self)->u64;
	fn read_gpa(&mut self,gpa:u64,value:&mut [u8]);
	fn write_gpa(&mut self,gpa:u64,value:&[u8]);

	fn emulate_mmio_output(&mut self,instruction:&Instruction,gpa:u64)
	{
		if let Some(info)=instruction.decode_mmio()
		{
			let mut data:MaybeUninit<[u8;16]>=MaybeUninit::uninit();
			let p=unsafe{data.assume_init_mut()};
			let len=info.len();
			match info
			{
				MmioInstruction::MovFromImm8(v)=>p[0]=v,
				MmioInstruction::MovFromImm16(v)=>p[..2].copy_from_slice(&v.to_le_bytes()),
				MmioInstruction::MovFromImm32(v)=>p[..4].copy_from_slice(&v.to_le_bytes()),
				MmioInstruction::MovFromImm64(v)=>p[..8].copy_from_slice(&v.to_le_bytes()),
				MmioInstruction::MovWithGpr8(i)=>p[0]=self.read_gpr(i as usize) as u8,
				MmioInstruction::MovWithGpr8Hi(i)=>p[0]=(self.read_gpr(i as usize)>>8) as u8,
				MmioInstruction::MovWithGpr16(i)=>p[..2].copy_from_slice(&self.read_gpr(i as usize).to_le_bytes()[..2]),
				MmioInstruction::MovWithGpr32(i)=>p[..4].copy_from_slice(&self.read_gpr(i as usize).to_le_bytes()[..4]),
				MmioInstruction::MovWithGpr64(i)=>p[..8].copy_from_slice(&self.read_gpr(i as usize).to_le_bytes()[..8]),
				MmioInstruction::MovWithSeg(i)=>p[..2].copy_from_slice(&self.read_seg_selector(i as usize).to_le_bytes())
			}
			self.write_gpa(gpa,&p[..len]);
		}
	}

	fn emulate_mmio_input(&mut self,instruction:&Instruction,gpa:u64)
	{
		if let Some(info)=instruction.decode_mmio()
		{
			let mut data:MaybeUninit<[u8;16]>=MaybeUninit::uninit();
			let p=unsafe{data.assume_init_mut()};
			let len=info.len();
			match info
			{
				MmioInstruction::MovWithGpr8(i)=>p[..8].copy_from_slice(&self.read_gpr(i as usize).to_le_bytes()[..8]),
				MmioInstruction::MovWithGpr8Hi(i)=>p[..8].copy_from_slice(&self.read_gpr(i as usize).to_le_bytes()[..8]),
				MmioInstruction::MovWithGpr16(i)=>p[..8].copy_from_slice(&self.read_gpr(i as usize).to_le_bytes()[..8]),
				MmioInstruction::MovWithGpr32(_)=>p[4..8].copy_from_slice(&[0;4]),
				_=>{}
			}
			self.read_gpa(gpa,&mut p[..len]);
			match info
			{
				MmioInstruction::MovWithGpr8(i)=>self.write_gpr(i as usize,u64::from_le_bytes(p[..8].try_into().unwrap())),
				MmioInstruction::MovWithGpr8Hi(i)=>self.write_gpr(i as usize,u64::from_le_bytes(p[..8].try_into().unwrap())),
				MmioInstruction::MovWithGpr16(i)=>self.write_gpr(i as usize,u64::from_le_bytes(p[..8].try_into().unwrap())),
				MmioInstruction::MovWithGpr32(i)=>self.write_gpr(i as usize,u64::from_le_bytes(p[..8].try_into().unwrap())),
				MmioInstruction::MovWithGpr64(i)=>self.write_gpr(i as usize,u64::from_le_bytes(p[..8].try_into().unwrap())),
				MmioInstruction::MovWithSeg(i)=>self.write_seg_selector(i as usize,u16::from_le_bytes(p[..2].try_into().unwrap())),
				_=>warn!("Unexpected MMIO form!")
			}
		}
	}
}