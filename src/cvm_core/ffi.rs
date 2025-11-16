/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file implements C-FFI for CVM API of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void,ptr::null_mut};

use nvcvm::{interface::{CvmHandle, ExitContext}, status::Status};

use crate::cvm_core::CUSTOMIZABLE_HYPERVISOR;

#[allow(non_upper_case_globals)] 
#[unsafe(no_mangle)] static noir_cvm_exit_context_size:usize=size_of::<ExitContext>();

unsafe extern "C"
{
	pub(super) static noir_maximum_memslot_shift:u8;
}

#[unsafe(no_mangle)] extern "C" fn nvc_query_hypervisor_status(_status_type:u64,_result:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_set_guest_vcpu_options(_vcpu:*mut c_void,_option_type:u32,_data:u32)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_edit_vcpu_registers(_vcpu:*mut c_void,_register_names:*const c_void,_register_count:u32,_register_size:u32,_buffer:*const c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_view_vcpu_registers(_vcpu:*mut c_void,_register_names:*const c_void,_register_count:u32,_register_size:u32,_buffer:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_set_event_injection(_vcpu:*mut c_void,_event:u64)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_query_vcpu_statistics(_vcpu:*mut c_void,_buffer:*mut c_void,_buffer_size:u32)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_run_vcpu(_vcpu:*mut c_void,_exit_context:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_rescind_vcpu(_vcpu:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_release_vcpu(_vcpu:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_create_vcpu(vm_handle:CvmHandle,vcpu_id:u32)->Status
{
	let mut lk=CUSTOMIZABLE_HYPERVISOR.write();
	match &mut *lk
	{
		Some(hv)=>
		{
			match hv.create_vcpu(vm_handle,vcpu_id as usize)
			{
				Ok(_)=>Status::SUCCESS,
				Err(e)=>e
			}
		}
		None=>Status::HYPERVISION_ABSENT
	}
}

#[unsafe(no_mangle)] extern "C" fn nvc_ref_vcpu(_vcpu:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_deref_vcpu(_vcpu:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_reference_vcpu(_vm:*mut c_void,_vcpu_id:u32)->*mut c_void
{
	null_mut()
}

#[unsafe(no_mangle)] extern "C" fn nvc_set_mapping(_vm:*mut c_void,_mapping_info:*const c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_query_gpa_accessing_bitmap(_vm:*mut c_void,_gpa_start:u64,_page_count:u32,_bitmap:*mut c_void,_bitmap_size:u32)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_clear_gpa_accessing_bits(_vm:*mut c_void,_gpa_start:u64,_page_count:u32)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_release_vm(vm_handle:CvmHandle)->Status
{
	let mut lk=CUSTOMIZABLE_HYPERVISOR.write();
	match &mut *lk
	{
		Some(hv)=>
		{
			hv.release_vm(vm_handle);
			Status::SUCCESS
		}
		None=>Status::HYPERVISION_ABSENT
	}
}

#[unsafe(no_mangle)] extern "C" fn nvc_create_vm_ex(_vm:*mut c_void,_process_id:u32,_properties:u64)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_create_vm(vm_handle:*mut CvmHandle,process_id:u32)->Status
{
	if process_id==0 || vm_handle.is_null()
	{
		return Status::INVALID_PARAMETER;
	}
	let mut lk=CUSTOMIZABLE_HYPERVISOR.write();
	match &mut *lk
	{
		Some(hv)=>
		{
			match hv.create_vm(process_id)
			{
				Ok(h)=>
				{
					unsafe
					{
						*vm_handle=h;
					}
					Status::SUCCESS
				}
				Err(e)=>e
			}
		}
		None=>Status::HYPERVISION_ABSENT
	}
}

#[unsafe(no_mangle)] extern "C" fn nvc_deref_vm(_vm:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_ref_vm(_vm:*mut c_void)->Status
{
	Status::NOT_IMPLEMENTED
}

#[unsafe(no_mangle)] extern "C" fn nvc_get_vm_pid(_vm:*mut c_void)->u32
{
	0
}