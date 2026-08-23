#![no_std]

extern crate alloc;

use core::{ffi::c_void, ptr::{null, null_mut}};

use cvsched::ioctl::*;
use nvcvm::{ioctl::*, status::Status};
use utf16_lit::utf16_null;
use windows_sys::{Wdk::{Foundation::{DEVICE_OBJECT, DRIVER_OBJECT, IO_STACK_LOCATION, IRP}, Storage::FileSystem::IO_NO_INCREMENT, System::SystemServices::{FILE_DEVICE_SECURE_OPEN, HighPagePriority, IRP_MJ_CLOSE, IRP_MJ_CREATE, IRP_MJ_DEVICE_CONTROL, IoCreateDevice, IoCreateSymbolicLink, IoDeleteDevice, IoDeleteSymbolicLink, IofCompleteRequest, KernelMode, MmCached, MmMapLockedPagesSpecifyCache}}, Win32::{Foundation::{NTSTATUS, STATUS_INVALID_DEVICE_REQUEST, STATUS_SUCCESS, STATUS_UNSUCCESSFUL, UNICODE_STRING}, System::Ioctl::{FILE_DEVICE_UNKNOWN, METHOD_BUFFERED, METHOD_NEITHER, METHOD_OUT_DIRECT}}};

use crate::misc::init_logger;

mod misc;

unsafe fn dispatch_io_unknown(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::DISPATCH_FAILURE
}

type IoDispatchFn=unsafe fn(*const c_void,usize,*mut c_void,usize)->Status;

const IOCTL_CODE_MAX:usize=0x30;

static DISPATCHER_GROUP:[IoDispatchFn;IOCTL_CODE_MAX]=
{
	let mut x:[IoDispatchFn;IOCTL_CODE_MAX]=[dispatch_io_unknown;IOCTL_CODE_MAX];
	x[IOCTL_CODE_HV_GET_CAPABILITY]=dispatch_get_capability;
	x[IOCTL_CODE_HV_CREATE_VM]=dispatch_create_vm;
	x[IOCTL_CODE_HV_DELETE_VM]=dispatch_delete_vm;
	x[IOCTL_CODE_VM_CREATE_VCPU]=dispatch_create_vcpu;
	x[IOCTL_CODE_VM_DELETE_VCPU]=dispatch_delete_vcpu;
	x[IOCTL_CODE_VM_SET_MEMORY_REGION]=dispatch_set_memory_region;
	x[IOCTL_CODE_VCPU_RUN]=dispatch_run_vcpu;
	x[IOCTL_CODE_VCPU_REQUEST_EVENT]=dispatch_request_event;
	x
};

static DEVICE_NAME:&[u16]=&utf16_null!("\\Device\\NoirVisor");
static LINK_NAME:&[u16]=&utf16_null!("\\DosDevices\\NoirVisor");

const unsafe fn unistr_from_slice(string:&[u16])->UNICODE_STRING
{
	UNICODE_STRING
	{
		Length:(string.len()<<1) as u16,
		MaximumLength:(string.len()<<1) as u16,
		Buffer:string.as_ptr() as *mut u16
	}
}

// Equivalent of IoGetCurrentIrpStackLocation.
#[inline(always)] unsafe fn get_current_irpsp(irp:*const IRP)->*mut IO_STACK_LOCATION
{
	unsafe
	{
		(*irp).Tail.Overlay.Anonymous2.Anonymous.CurrentStackLocation
	}
}

#[inline(always)] const fn method_from_ctl_code(ctl_code:u32)->u32
{
	ctl_code&3
}

#[inline(always)] const fn is_ctl_code_custom(ctl_code:u32)->bool
{
	(ctl_code&(1<<13))!=0
}

#[inline(always)] const fn function_from_ctl_code(ctl_code:u32)->usize
{
	((ctl_code>>2)&0x7FF) as usize
}

unsafe fn get_input_buffer(irp:*const IRP)->*mut c_void
{
	unsafe
	{
		let irpsp=get_current_irpsp(irp);
		if (*irpsp).MajorFunction==IRP_MJ_DEVICE_CONTROL as u8
		{
			let method=method_from_ctl_code((*irpsp).Parameters.DeviceIoControl.IoControlCode);
			if method==METHOD_NEITHER
			{
				(*irpsp).Parameters.DeviceIoControl.Type3InputBuffer
			}
			else
			{
				(*irp).AssociatedIrp.SystemBuffer
			}
		}
		else
		{
			null_mut()
		}
	}
}

unsafe fn get_output_buffer(irp:*const IRP)->*mut c_void
{
	unsafe
	{
		let irpsp=get_current_irpsp(irp);
		if (*irpsp).MajorFunction==IRP_MJ_DEVICE_CONTROL as u8
		{
			let method=method_from_ctl_code((*irpsp).Parameters.DeviceIoControl.IoControlCode);
			if method==METHOD_BUFFERED
			{
				(*irp).AssociatedIrp.SystemBuffer
			}
			else if method==METHOD_OUT_DIRECT
			{
				// Manually implement MmGetSystemAddressForMdlSafe
				let mdl=(*irp).MdlAddress;
				const MDL_MAPPED_TO_SYSTEM_VA:i16=0x0;
				const MDL_SOURCE_IS_NONPAGED_POOL:i16=0x4;
				if ((*mdl).MdlFlags&(MDL_MAPPED_TO_SYSTEM_VA|MDL_SOURCE_IS_NONPAGED_POOL))==0
				{
					MmMapLockedPagesSpecifyCache(mdl,KernelMode as i8,MmCached,null(),0,HighPagePriority as u32)
				}
				else
				{
					(*mdl).MappedSystemVa
				}
			}
			else
			{
				(*irp).UserBuffer
			}
		}
		else
		{
			null_mut()
		}
	}
}

unsafe extern "system" fn driver_unload(driver_object:*const DRIVER_OBJECT)
{
	unsafe
	{
		let link_name=unistr_from_slice(LINK_NAME);
		IoDeleteSymbolicLink(&raw const link_name);
		IoDeleteDevice((*driver_object).DeviceObject);
	}
	dprintln!(0,"Driver is unloaded!");
}

unsafe extern "system" fn dispatch_create_close(_device_object:*const DEVICE_OBJECT,irp:*mut IRP)->NTSTATUS
{
	unsafe
	{
		(*irp).IoStatus.Anonymous.Status=STATUS_SUCCESS;
		(*irp).IoStatus.Information=0;
		IofCompleteRequest(irp,IO_NO_INCREMENT as i8);
	}
	STATUS_SUCCESS
}

unsafe extern "system" fn dispatch_io_control(_device_object:*const DEVICE_OBJECT,irp:*mut IRP)->NTSTATUS
{
	let mut st:NTSTATUS=STATUS_INVALID_DEVICE_REQUEST;
	unsafe
	{
		let irpsp=get_current_irpsp(irp);
		let in_buff=get_input_buffer(irp);
		let out_buff=get_output_buffer(irp);
		let in_size=(*irpsp).Parameters.DeviceIoControl.InputBufferLength as usize;
		let out_size=(*irpsp).Parameters.DeviceIoControl.OutputBufferLength as usize;
		let io_ctrl_code=(*irpsp).Parameters.DeviceIoControl.IoControlCode;
		if is_ctl_code_custom(io_ctrl_code)
		{
			let code_index=function_from_ctl_code(io_ctrl_code);
			if let Some(f)=DISPATCHER_GROUP.get(code_index)
			{
				let cv_st=f(in_buff,in_size,out_buff,out_size);
				// Convert NoirVisor's status code into NT's status code.
				st=match cv_st
				{
					Status::SUCCESS=>STATUS_SUCCESS,
					Status::DISPATCH_FAILURE=>STATUS_INVALID_DEVICE_REQUEST,
					_=>STATUS_UNSUCCESSFUL
				}
			}
		}
		(*irp).IoStatus.Anonymous.Status=st;
		(*irp).IoStatus.Information=out_size;
		IofCompleteRequest(irp,IO_NO_INCREMENT as i8);
	}
	st
}

#[unsafe(no_mangle)] unsafe extern "system" fn NoirDriverEntry(driver_object:*mut DRIVER_OBJECT,_registry_path:*const UNICODE_STRING)->NTSTATUS
{
	let mut st:NTSTATUS;
	init_logger();
	unsafe
	{
		(*driver_object).MajorFunction[IRP_MJ_CREATE as usize]=Some(dispatch_create_close);
		(*driver_object).MajorFunction[IRP_MJ_CLOSE as usize]=Some(dispatch_create_close);
		(*driver_object).MajorFunction[IRP_MJ_DEVICE_CONTROL as usize]=Some(dispatch_io_control);
		(*driver_object).DriverUnload=Some(driver_unload);
		let dev_name=unistr_from_slice(DEVICE_NAME);
		let mut dev_obj=null_mut();
		st=IoCreateDevice(driver_object,0,&raw const dev_name,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,false,&raw mut dev_obj);
		if st==STATUS_SUCCESS
		{
			let link_name=unistr_from_slice(LINK_NAME);
			st=IoCreateSymbolicLink(&raw const link_name,&raw const dev_name);
			if st!=STATUS_SUCCESS
			{
				IoDeleteDevice(dev_obj);
			}
		}
	}
	st
}

#[cfg(not(test))]
mod panicking
{
	use core::panic::PanicInfo;

	use log::error;
	use windows_sys::{Wdk::System::SystemServices::KeBugCheckEx, Win32::System::Diagnostics::Debug::MANUALLY_INITIATED_CRASH};

	#[panic_handler] fn panic(info:&PanicInfo)->!
	{
		error!("NoirVisor {info}");
		unsafe
		{
			// As we're panicking, send the system to crash.
			KeBugCheckEx(MANUALLY_INITIATED_CRASH,0,0,0,0);
		}
		// For some strange reasons, KeBugCheckEx does not return never type.
		loop{}
	}
}
