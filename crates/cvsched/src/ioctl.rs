// NoirVisor CVM Scheduler IOCTL Handlers

use core::ffi::c_void;

use nvcvm::status::Status;

/// The `init` method initializes the whole crate.
/// 
/// ## Safety
/// This method is unsafe when it's called more than once.
pub unsafe fn init()->bool
{
	crate::hvcall::init();
	true
}

/// The `deinit` method finalizes the whole crate.
/// 
/// ## Safety
/// This method is unsafe when this crate is still used after calling this method.
pub unsafe fn deinit()
{

}

/// The `dispatch_get_capability` function queries the capability of the NoirVisor exposed in the current system.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_get_capability(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::NOT_IMPLEMENTED
}

/// The `dispatch_create_vm` function creates a virtual machine.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_create_vm(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::NOT_IMPLEMENTED
}

/// The `dispatch_delete_vm` function deletes a virtual machine.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_delete_vm(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	
	Status::SUCCESS
}

/// The `dispatch_create_vcpu` function creates a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_create_vcpu(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::NOT_IMPLEMENTED
}

/// The `dispatch_delete_vcpu` function deletes a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_delete_vcpu(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::NOT_IMPLEMENTED
}

/// The `dispatch_set_memory_region` function maps/unmaps a memory region in the virtual machine.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_set_memory_region(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::SUCCESS
}

/// The `dispatch_run_vcpu` function runs a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_run_vcpu(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::NOT_IMPLEMENTED
}

/// The `dispatch_request_event` function requests an event into a virtual processor.
/// 
/// ## Safety
/// This function is safe if and only if all pointers and lengths are valid.
pub unsafe fn dispatch_request_event(_in_buff:*const c_void,_in_size:usize,_out_buff:*mut c_void,_out_size:usize)->Status
{
	Status::NOT_IMPLEMENTED
}
