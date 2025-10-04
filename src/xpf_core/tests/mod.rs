/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file mocks C/ASM APIs for testing NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void, ptr::null_mut};
use paste::paste;
use crate::xpf_core::nvbdk::GprState;

extern crate std;

mod cpu;
mod memory;
pub mod svm_hv;

#[unsafe(no_mangle)] extern "C" fn nvc_store_image_info(_base:*mut *mut c_void,_size:*mut u32)
{

}

#[unsafe(no_mangle)] extern "C" fn nvc_svm_return(_stack:*const GprState)->!
{
	loop{}
}

#[unsafe(no_mangle)] extern "C" fn nvc_svm_guest_start()
{

}

#[unsafe(no_mangle)] extern "C" fn nvc_svm_subvert_processor_a(_vcpu:*mut c_void)
{

}

#[unsafe(no_mangle)] extern "C" fn nvc_vt_resume_without_entry(_gpr_state:*const GprState)->!
{
	loop{}
}

#[unsafe(no_mangle)] extern "C" fn nvc_vt_exit_handler_a()
{

}

#[unsafe(no_mangle)] extern "C" fn nvc_vt_guest_start()
{

}

#[unsafe(no_mangle)] extern "C" fn nvc_vt_subvert_processor_a(_vcpu:*mut c_void)
{

}

#[unsafe(no_mangle)] extern "C" fn noir_locate_acpi_rsdt(_length:*mut usize)->*mut c_void
{
	null_mut()
}

#[unsafe(no_mangle)] extern "C" fn noir_query_enabled_features_in_system()->i64
{
	0
}

#[unsafe(no_mangle)] extern "C" fn noir_system_debugger_write(_string:*const u8,_maximum_length:usize)
{

}

macro_rules! generate_handler
{
	($name:tt) =>
	{
		paste!
		{
			#[unsafe(no_mangle)] extern "C" fn [<noir_ $name:lower _handler_a>]()->!
			{
				loop{}
			}
		}
	};
}

generate_handler!(divide_error_fault);
generate_handler!(debug_fault_trap);
generate_handler!(breakpoint_trap);
generate_handler!(overflow_trap);
generate_handler!(bound_range_fault);
generate_handler!(invalid_opcode_fault);
generate_handler!(device_not_available_fault);
generate_handler!(double_fault_abort);
generate_handler!(invalid_tss_fault);
generate_handler!(segment_not_present_fault);
generate_handler!(stack_fault);
generate_handler!(general_protection_fault);
generate_handler!(page_fault);
generate_handler!(x87_floating_point_fault);
generate_handler!(alignment_check_fault);
generate_handler!(machine_check_abort);
generate_handler!(simd_floating_point_fault);
generate_handler!(control_protection_fault);