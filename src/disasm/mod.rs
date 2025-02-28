/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file is the invoker of the iced Disassembler Crate.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).
*/

use core::slice;

use iced_x86::*;
use paste::paste;

use crate::FormatBuffer;

static EXAMPLE_CODE: &[u8] = &[
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
    0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
    0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
    0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
];

// This routine is intended for eagerly initializing the iced-x86 crate
// so that all `lazy_static` items are initialized.
#[allow(non_snake_case)]
#[unsafe(no_mangle)] extern "C" fn NoirInitializeDisassembler()
{
	let mut decoder=Decoder::with_ip(64,EXAMPLE_CODE,0x2000,0);
	let mut fmter=MasmFormatter::new();
	while decoder.can_decode()
	{
		let mut mnemonic=FormatBuffer::default();
		let ins_info=decoder.decode();
		let _=ins_info.op_code();
		fmter.format(&ins_info,&mut mnemonic);
	}
}

macro_rules! build_disasm_fn
{
	($bitness:tt) =>
	{
		paste!
		{
			#[allow(non_snake_case)]
			#[unsafe(no_mangle)] unsafe extern "C" fn [<NoirGetInstructionLength $bitness>](Code:*const u8,CodeLength:usize)->u8
			{
				let code_slice=unsafe{slice::from_raw_parts(Code,if CodeLength==0 {15} else {CodeLength})};
				let mut decoder=Decoder::new(16,code_slice,DecoderOptions::NONE);
				let ins_info=decoder.decode();
				ins_info.len() as u8
			}
		}
	};
}

build_disasm_fn!(16);
build_disasm_fn!(32);
build_disasm_fn!(64);