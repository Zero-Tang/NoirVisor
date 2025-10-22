/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file is the invoker of the yaxpeax Disassembler Crate.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).
*/

use core::slice;

use yaxpeax_arch::LengthedInstruction;
use paste::paste;

pub(crate) mod emulator;

#[allow(non_snake_case)]
#[unsafe(no_mangle)] extern "C" fn NoirInitializeDisassembler()
{
	// For yaxpeax, there is basically nothing to do to initialize it.
}

macro_rules! build_disasm_fn
{
	($bitness:tt,$name:tt) =>
	{
		paste!
		{
			#[allow(non_snake_case)]
			#[unsafe(no_mangle)] unsafe extern "C" fn [<NoirGetInstructionLength $bitness>](Code:*const u8,CodeLength:usize)->u8
			{
				use yaxpeax_x86::[<$name:lower _mode>]::InstDecoder;
				let code_slice=unsafe{slice::from_raw_parts(Code,if CodeLength==0 {15} else {CodeLength})};
				let decoder=InstDecoder::default();
				match decoder.decode_slice(code_slice)
				{
					Ok(ins)=>ins.len().to_const() as u8,
					Err(_)=>0
				}
			}
		}
	};
}

build_disasm_fn!(16,real);
build_disasm_fn!(32,protected);
build_disasm_fn!(64,long);