// NoirVisor CVM Scheduler IOCTL Handlers

use core::{ffi::c_void, hint::cold_path, mem::MaybeUninit};

use alloc::sync::Arc;
use nvcvm::{hvcall::*, interface::CvmHandle, ioctl::*, status::{Status, unwrap_status}};
use paste::paste;
use log::*;

use crate::{hvcall::hypercall, vmm::{VM_LIST, VirtualMachine}};
use crate::platform::{sync::RwLock, kmap::Kmap};

/// The `init` method initializes the whole crate.
/// 
/// ## Safety
/// This method is unsafe when it's called more than once.
#[unsafe(no_mangle)] unsafe extern "system" fn noir_cvsched_init()->bool
{
	crate::hvcall::init();
	unsafe
	{
		VM_LIST.init()
	}
}

/// The `deinit` method finalizes the whole crate.
/// 
/// ## Safety
/// This method is unsafe when this crate is still used after calling this method.
#[unsafe(no_mangle)] unsafe extern "system" fn noir_cvsched_deinit()
{
	unsafe
	{
		VM_LIST.deinit();
	}
}

// A macro that reduces effort for checking buffer sizes.
macro_rules! check_buffer_args
{
	($in_size:expr,$out_size:expr,$name:tt)=>
	{
		paste!
		{
			if $in_size<size_of::<[<Cvm $name InputBuffer>]>() || $out_size<size_of::<[<Cvm $name OutputBuffer>]>()
			{
				return Status::BUFFER_TOO_SMALL;
			}
		}
	};
}

/// The `dispatch_get_capability` function queries the capability of the NoirVisor exposed in the current system.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_get_capability(in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,GetCapability);
	let mut ctxt:MaybeUninit<CvmHypercallGetCapabilityContext>=MaybeUninit::uninit();
	unsafe
	{
		let ctxt=ctxt.assume_init_mut();
		let in_buff:&CvmGetCapabilityInputBuffer=&*in_buff.cast();
		let out_buff:&mut CvmGetCapabilityOutputBuffer=&mut *out_buff.cast();
		ctxt.code=in_buff.capability_code;
		let st=hypercall(CVM_HYPERCALL_GET_CAPABILITY,ctxt,size_of::<CvmHypercallGetCapabilityContext>());
		out_buff.capability.copy_from_slice(&ctxt.raw);
		unwrap_status(st)
	}
}

/// The `dispatch_create_vm` function creates a virtual machine.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_create_vm(_in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,CreateVm);
	let mut ctxt:MaybeUninit<CvmHypercallCreateVmContext>=MaybeUninit::uninit();
	let r=unsafe
	{
		let ctxt=ctxt.assume_init_mut();
		let st=hypercall(CVM_HYPERCALL_CREATE_VM,ctxt,size_of::<CvmHypercallCreateVmContext>());
		st.map(|_| ctxt.handle)
	};
	match r
	{
		Ok(handle)=>
		{
			let r=match Arc::<RwLock<VirtualMachine>>::try_new_uninit()
			{
				Ok(arc_vm)=>unsafe
				{
					let vm=arc_vm.assume_init_ref();
					if vm.init()
					{
						vm.write().init(handle,0);
						Ok(arc_vm.assume_init())
					}
					else
					{
						cold_path();
						error!("Failed to create RwLock<VirtualMachine>!");
						Err(Status::UNSUCCESSFUL)
					}
				}
				Err(_)=>
				{
					cold_path();
					error!("Failed to create Arc<RwLock<VirtualMachine>>!");
					Err(Status::INSUFFICIENT_RESOURCES)
				}
			};
			match r
			{
				Ok(arc_vm)=>
				{
					match VM_LIST.write().create_handle(arc_vm)
					{
						Ok(handle)=>
						{
							let out_buff:&mut CvmCreateVmOutputBuffer=unsafe{&mut *out_buff.cast()};
							out_buff.vm_handle=CvmHandle(handle as u32);
							out_buff.status=Status::SUCCESS;
							Status::SUCCESS
						}
						Err(_)=>Status::INSUFFICIENT_RESOURCES
					}
				}
				Err(st)=>st
			}
		}
		Err(st)=>st
	}
}

/// The `dispatch_delete_vm` function deletes a virtual machine.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_delete_vm(in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,DeleteVm);
	let in_buff:&CvmDeleteVmInputBuffer=unsafe{&*in_buff.cast()};
	let st=match VM_LIST.write().delete_handle(in_buff.vm_handle.0 as usize)
	{
		Some(vm)=>
		{
			let mut ctxt:MaybeUninit<CvmHypercallDeleteVmContext>=MaybeUninit::uninit();
			let st=unsafe
			{
				let ctxt=ctxt.assume_init_mut();
				let lk=vm.read();
				ctxt.handle=lk.handle;
				hypercall(CVM_HYPERCALL_DELETE_VM,ctxt,size_of::<CvmHypercallDeleteVmContext>())
			};
			drop(vm);
			unwrap_status(st)
		}
		None=>Status::INVALID_PARAMETER
	};
	let out_buff:&mut CvmDeleteVmOutputBuffer=unsafe{&mut *out_buff.cast()};
	out_buff.status=st;
	st
}

/// The `dispatch_create_vcpu` function creates a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_create_vcpu(in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,CreateVcpu);
	let in_buff:&CvmCreateVcpuInputBuffer=unsafe{&*in_buff.cast()};
	match VM_LIST.read().get(in_buff.vm_handle.0 as usize)
	{
		Some(arc_vm)=>
		{
			let mut vm=arc_vm.write();
			match vm.create_vcpu(in_buff.vcpu_id)
			{
				Ok(vpcb)=>
				{
					let out_buff:&mut CvmCreateVcpuOutputBuffer=unsafe{&mut *out_buff.cast()};
					out_buff.vpcb_uva=vpcb;
					out_buff.status=Status::SUCCESS;
					Status::SUCCESS
				}
				Err(st)=>st
			}
		}
		None=>Status::INVALID_PARAMETER
	}
}

/// The `dispatch_delete_vcpu` function deletes a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_delete_vcpu(in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,DeleteVcpu);
	let in_buff:&CvmDeleteVcpuInputBuffer=unsafe{&*in_buff.cast()};
	match VM_LIST.read().get(in_buff.vm_handle.0 as usize)
	{
		Some(arc_vm)=>
		{
			let mut vm=arc_vm.write();
			let st=vm.delete_vcpu(in_buff.vcpu_id);
			let out_buff:&mut CvmDeleteVcpuOutputBuffer=unsafe{&mut *out_buff.cast()};
			out_buff.status=st;
			st
		}
		None=>Status::INVALID_PARAMETER
	}
}

/// The `dispatch_set_memory_region` function maps/unmaps a memory region in the virtual machine.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_set_memory_region(in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,SetMapping);
	let in_buff:&CvmSetMappingInputBuffer=unsafe{&*in_buff.cast()};
	match VM_LIST.read().get(in_buff.vm_handle.0 as usize)
	{
		Some(arc_vm)=>
		{
			let mut vm=arc_vm.write();
			let kmap=match Kmap::new(in_buff.hva,in_buff.info.size as usize)
			{
				Ok(v)=>v,
				Err(st)=>return st
			};
			let st=vm.set_mapping(in_buff.info.as_id,in_buff.info.base_gpa,&mut kmap.iter(),in_buff.info.size as usize,in_buff.info.flags);
			let out_buff:&mut CvmSetMappingOutputBuffer=unsafe{&mut *out_buff.cast()};
			out_buff.status=st;
			st
		}
		None=>Status::INVALID_PARAMETER
	}
}

/// The `dispatch_run_vcpu` function runs a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_run_vcpu(in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,RunVcpu);
	let in_buff:&CvmRunVcpuInputBuffer=unsafe{&*in_buff.cast()};
	match VM_LIST.read().get(in_buff.vm_handle.0 as usize)
	{
		Some(arc_vm)=>
		{
			let vm=arc_vm.read();
			let st=vm.run_vcpu(in_buff.vcpu_id);
			let out_buff:&mut CvmRunVcpuOutputBuffer=unsafe{&mut *out_buff.cast()};
			out_buff.status=st;
			st
		}
		None=>Status::INVALID_PARAMETER
	}
}

/// The `dispatch_request_event` function requests an event into a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_request_event(in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	check_buffer_args!(in_size,out_size,RequestEvent);
	let in_buff:&CvmSetMappingInputBuffer=unsafe{&*in_buff.cast()};
	let km=match Kmap::new(in_buff.hva,in_buff.info.size as usize)
	{
		Ok(km)=>km,
		Err(st)=>return st
	};
	match VM_LIST.read().get(in_buff.vm_handle.0 as usize)
	{
		Some(arc_vm)=>
		{
			let mut vm=arc_vm.write();
			let out_buff:&mut CvmSetMappingOutputBuffer=unsafe{&mut *out_buff.cast()};
			let st=vm.set_mapping(in_buff.info.as_id,in_buff.info.base_gpa,&mut km.iter(),(in_buff.info.size>>12) as usize,in_buff.info.flags);
			out_buff.status=st;
			st
		}
		None=>Status::INVALID_PARAMETER
	}
}

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

#[unsafe(no_mangle)] unsafe extern "system" fn noir_dispatch_ioctl(ioctl_code:usize,in_buff:*const c_void,in_size:usize,out_buff:*mut c_void,out_size:usize)->Status
{
	match DISPATCHER_GROUP.get(ioctl_code)
	{
		Some(f)=>unsafe
		{
			f(in_buff,in_size,out_buff,out_size)
		}
		None=>Status::DISPATCH_FAILURE
	}
}