// NoirVisor CVM Interface module (C bindings)

use core::{ffi::c_void, mem::MaybeUninit};

use crate::{interface::*, ioctl::*, status::Status};

#[cfg(target_os="uefi")] use crate::uefi as platform;
#[cfg(windows)] use crate::windows as platform;

/// ## `ncv_get_capability`
/// This API gets the hypervisor's capability.
/// 
/// ## Safety
/// This function is unsafe because it's FFI.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_get_capability(capability_code:u32,buffer:*mut u32)->Status
{
	let in_buff=CvmGetCapabilityInputBuffer
	{
		version:IOCTL_CURRENT_VERSION,
		capability_code
	};
	let mut out_buff:MaybeUninit<CvmGetCapabilityOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		let st=platform::do_ioctl(IOCTL_CODE_HV_GET_CAPABILITY,&raw const in_buff,out_buff);
		if st==Status::SUCCESS
		{
			core::ptr::copy_nonoverlapping(out_buff.capability.as_ptr(),buffer,7);
		}
		st
	}
}

/// ## `ncv_create_vm`
/// This API creates a VM.
/// 
/// ## Safety
/// This function is unsafe because it's FFI.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_create_vm(vm_handle:*mut CvmHandle)->Status
{
	let in_buff=CvmCreateVmInputBuffer{version:IOCTL_CURRENT_VERSION};
	let mut out_buff:MaybeUninit<CvmCreateVmOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		let st=platform::do_ioctl(IOCTL_CODE_HV_CREATE_VM,&raw const in_buff,out_buff);
		if st==Status::SUCCESS
		{
			*vm_handle=out_buff.vm_handle;
		}
		st
	}
}

/// ## `ncv_delete_vm`
/// This API deletes the target VM.
/// 
/// ## Safety
/// This function is unsafe because it's FFI.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_delete_vm(vm_handle:CvmHandle)->Status
{
	let in_buff=CvmDeleteVmInputBuffer
	{
		version:IOCTL_CURRENT_VERSION,
		vm_handle
	};
	let mut out_buff:MaybeUninit<CvmDeleteVmOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		platform::do_ioctl(IOCTL_CODE_HV_DELETE_VM,&raw const in_buff,out_buff)
	}
}

/// ## `ncv_create_vcpu`
/// This API creates a vCPU in the target VM.
/// 
/// ## Safety
/// This function is unsafe because it's FFI.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_create_vcpu(vm_handle:CvmHandle,vcpu_id:u32,vpcb:*mut *mut Vpcb)->Status
{
	let in_buff=CvmCreateVcpuInputBuffer
	{
		version:IOCTL_CURRENT_VERSION,
		vm_handle,
		vcpu_id
	};
	let mut out_buff:MaybeUninit<CvmCreateVcpuOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		let st=platform::do_ioctl(IOCTL_CODE_VM_CREATE_VCPU,&raw const in_buff,out_buff);
		if st==Status::SUCCESS
		{
			*vpcb=out_buff.vpcb_uva;
		}
		st
	}
}

/// ## `ncv_delete_vcpu`
/// This API deletes the target vCPU from the target VM.
/// 
/// ## Safety
/// This function is unsafe because it's FFI.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_delete_vcpu(vm_handle:CvmHandle,vcpu_id:u32)->Status
{
	let in_buff=CvmDeleteVcpuInputBuffer
	{
		version:IOCTL_CURRENT_VERSION,
		vm_handle,
		vcpu_id
	};
	let mut out_buff:MaybeUninit<CvmDeleteVcpuOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		platform::do_ioctl(IOCTL_CODE_VM_DELETE_VCPU,&raw const in_buff,out_buff)
	}
}

/// ## `ncv_set_mapping`
/// This API sets mapping for the guest.
/// 
/// `size`: Number of 4KiB pages to map.
/// 
/// ## Safety
/// This function is unsafe because it's FFI. \
/// You must ensure `hva` and `size` specifies the range for the guest.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_set_mapping(vm_handle:CvmHandle,gpa:u64,hva:*mut c_void,size:usize,as_id:u32,flags:CvmMappingFlags)->Status
{
	let in_buff=CvmSetMappingInputBuffer
	{
		version:IOCTL_CURRENT_VERSION,
		vm_handle,
		hva:hva as u64,
		info:CvmMapping
		{
			base_gpa:gpa,
			size:size as u64,
			as_id,
			flags
		}
	};
	let mut out_buff:MaybeUninit<CvmSetMappingOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		platform::do_ioctl(IOCTL_CODE_VM_SET_MEMORY_REGION,&raw const in_buff,out_buff)
	}
}

/// ## `ncv_run_vcpu`
/// This API instructs the hypervisor to run the specified vCPU.
/// 
/// ## Safety
/// This function is unsafe because it's FFI.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_run_vcpu(vm_handle:CvmHandle,vcpu_id:u32)->Status
{
	let in_buff=CvmRunVcpuInputBuffer
	{
		version:IOCTL_CURRENT_VERSION,
		vm_handle,
		vcpu_id
	};
	let mut out_buff:MaybeUninit<CvmRunVcpuOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		platform::do_ioctl(IOCTL_CODE_VCPU_RUN,&raw const in_buff,out_buff)
	}
}

/// ## `ncv_request_event`
/// This API requests an event in the target vCPU.
/// 
/// ## Safety
/// This function is unsafe because it's FFI.
#[unsafe(no_mangle)] pub unsafe extern "C" fn ncv_request_event(vm_handle:CvmHandle,vcpu_id:u32,info:CvmRequestedEventInformation,error_code:u32,payload:u64)->Status
{
	let in_buff=CvmRequestEventInputBuffer
	{
		version:IOCTL_CURRENT_VERSION,
		vm_handle,
		vcpu_id,
		info:CvmRequestedEvent
		{
			info,
			error_code,
			payload
		}
	};
	let mut out_buff:MaybeUninit<CvmRequestEventOutputBuffer>=MaybeUninit::uninit();
	unsafe
	{
		let out_buff=out_buff.assume_init_mut();
		out_buff.version=IOCTL_CURRENT_VERSION;
		platform::do_ioctl(IOCTL_CODE_VCPU_REQUEST_EVENT,&raw const in_buff,out_buff)
	}
}