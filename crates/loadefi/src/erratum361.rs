/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file contains a test for AMD Erratum 361.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{
	arch::{asm, naked_asm},
	sync::atomic::{AtomicU16, Ordering},
};

use efi_helpers::println;

static CS_STORAGE: AtomicU16 = AtomicU16::new(0);
static SS_STORAGE: AtomicU16 = AtomicU16::new(0);
static mut IDT_BUFFER: [u128; 256] = [0; 256];

unsafe extern "win64" fn handle_db(stack: *mut u64)
{
	println!("#DB rip=0x{:X}", unsafe { *stack });
}

unsafe extern "win64" fn handle_bp(stack: *mut u64)
{
	println!("#BP rip=0x{:X}", unsafe { *stack });
}

#[unsafe(naked)]
#[unsafe(no_mangle)]
unsafe extern "win64" fn asm_handle_db()
{
	naked_asm!(
		"push rax",
		"push rcx",
		"push rdx",
		"push r8",
		"push r9",
		"push r10",
		"push r11",
		"lea rcx,[rsp+0x38]",
		"call {}",
		"pop r11",
		"pop r10",
		"pop r9",
		"pop r8",
		"pop rdx",
		"pop rcx",
		"pop rax",
		"iretq",
		sym handle_db
	);
}

#[unsafe(naked)]
#[unsafe(no_mangle)]
unsafe extern "win64" fn asm_handle_bp()
{
	naked_asm!(
		"push rax",
		"push rcx",
		"push rdx",
		"push r8",
		"push r9",
		"push r10",
		"push r11",
		"lea rcx,[rsp+0x38]",
		"call {}",
		"pop r11",
		"pop r10",
		"pop r9",
		"pop r8",
		"pop rdx",
		"pop rcx",
		"pop rax",
		"iretq",
		sym handle_bp
	);
}

#[unsafe(naked)]
unsafe extern "win64" fn test_erratum(_ptr: *mut u16)
{
	naked_asm!("int 3", "mov ss,word ptr [rcx]", "cpuid", "ret");
}

fn set_idt(vector: u8, handler: unsafe extern "win64" fn())
{
	let h = handler as usize as u128;
	let mut v = h & 0xffff;
	v |= (CS_STORAGE.load(Ordering::Relaxed) as u128) << 16;
	v |= 0x8E00 << 32;
	v |= (h & 0xFFFF0000) << 32;
	v |= (h & 0xFFFFFFFF00000000) << 64;
	unsafe {
		IDT_BUFFER[vector as usize] = v;
	}
}

pub fn test_amd_erratum_361()
{
	let ss: u16;
	let cs: u16;
	unsafe {
		asm!(
			"cli",
			"mov {ss:x},ss",
			"mov {cs:x},cs",
			ss = out(reg) ss,
			cs = out(reg) cs
		);
	}
	SS_STORAGE.store(ss, Ordering::Relaxed);
	CS_STORAGE.store(cs, Ordering::Relaxed);
	println!("SS: 0x{ss:X} is stored at {:p}!", SS_STORAGE.as_ptr());
	set_idt(1, asm_handle_db);
	set_idt(3, asm_handle_bp);

	unsafe {
		let mut old_idtr: [u16; 5] = [0; 5];
		let mut new_idtr: [u16; 5] = [0; 5];
		new_idtr[0] = 0xFFF;
		new_idtr.as_mut_ptr().add(1).cast::<u64>().write_unaligned(&raw mut IDT_BUFFER as u64);
		asm!(
			"sidt [{}]",
			"lidt [{}]",
			in(reg) old_idtr.as_mut_ptr(),
			in(reg) new_idtr.as_mut_ptr()
		);
		asm!(
			"mov dr0,{}",
			"mov dr7,{}",
			in(reg) SS_STORAGE.as_ptr(),
			in(reg) 0x70402u64
		);
		test_erratum(SS_STORAGE.as_ptr());
		asm!(
			"lidt [{}]",
			"mov dr7,{}",
			in(reg) old_idtr.as_mut_ptr(),
			in(reg) 0x400u64
		);
	}
}
