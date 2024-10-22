/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file handles VM-Exits in AMD-V of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use super::*;

#[no_mangle] pub extern "C" fn nvc_svm_exit_handler(gpr_state:*mut GprState,_vcpu:*mut SvmVcpu)
{
	let cur_vmcb=unsafe{*gpr_state}.rax as *mut c_void;
	let intercept_code:i64=unsafe{vmread(cur_vmcb,EXIT_CODE)};
	println!("Intercept Code: 0x{:016X}",intercept_code);
	loop{}
}