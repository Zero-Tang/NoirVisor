// NoirVisor CVM Hypercall Interface Definitions.

#[cfg(feature="scheduler")]
use core::{arch::{naked_asm, x86_64::__cpuid}, ffi::c_void};

use bitfield_struct::bitfield;
use nvcvm::interface::{CvmHandle, CvmRequestedEvent};

#[cfg(feature="scheduler")]
use nvcvm::status::Status;

#[cfg(feature="scheduler")]
#[unsafe(naked)] unsafe extern "win64" fn vmcall(fn_index:u32,context:*mut c_void)->Status
{
	naked_asm!
	(
		"vmcall",
		"ret"
	)
}

#[cfg(feature="scheduler")]
#[unsafe(naked)] unsafe extern "win64" fn vmmcall(fn_index:u32,context:*mut c_void)->Status
{
	naked_asm!
	(
		"vmmcall",
		"ret"
	)
}

#[cfg(feature="scheduler")]
unsafe extern "win64" fn unknown_hypercall(_fn_index:u32,_context:*mut c_void)->Status
{
	Status::HYPERVISION_ABSENT
}

#[cfg(feature="scheduler")]
static mut HYPERCALL_FN:unsafe extern "win64" fn(fn_index:u32,context:*mut c_void)->Status=unknown_hypercall;

#[cfg(feature="scheduler")]
pub(crate) fn init()
{
	let vendor_id=__cpuid(0);
	let mut vendor_str_raw:[u8;12]=[0;12];
	vendor_str_raw[..4].copy_from_slice(&vendor_id.ebx.to_le_bytes());
	vendor_str_raw[4..8].copy_from_slice(&vendor_id.edx.to_le_bytes());
	vendor_str_raw[8..].copy_from_slice(&vendor_id.ecx.to_le_bytes());
	let vendor_str_raw=unsafe{str::from_utf8_unchecked(&vendor_str_raw)};
	unsafe
	{
		HYPERCALL_FN=match vendor_str_raw
		{
			"GenuineIntel"|"VIA VIA VIA "|"CentaurHauls"|"  Shanghai  "=>vmcall,
			"AuthenticAMD"|"AMDisbetter!"|"HygonGenuine"=>vmmcall,
			_=>unknown_hypercall
		}
	}
}

/// Perform a hypercall on the current system.
/// 
/// ## Safety
/// If NoirVisor is not loaded, the behavior is undefined.
#[cfg(feature="scheduler")]
#[inline(always)] pub(crate) unsafe fn hypercall<T:Sized>(fn_index:u32,context:*mut T)->Status
{
	unsafe
	{
		HYPERCALL_FN(fn_index,context.cast())
	}
}

// CVM Hypervisor Interfaces
pub const CVM_HYPERCALL_GET_CAPABILITY:u32=0x10000;
pub const CVM_HYPERCALL_CREATE_VM:u32=0x10001;
pub const CVM_HYPERCALL_DELETE_VM:u32=0x10002;

#[repr(C)] pub struct CvmHypercallGetCapabilityContext
{
	pub code:u32,
	pub raw:[u32;7]
}

#[repr(C)] pub struct CvmHypercallCreateVmContext
{
	pub handle:CvmHandle
}

#[repr(C)] pub struct CvmHypercallDeleteVmContext
{
	pub handle:CvmHandle
}

// CVM Virtual Machine Interfaces
pub const CVM_HYPERCALL_CREATE_VCPU:u32=0x10010;
pub const CVM_HYPERCALL_DELETE_VCPU:u32=0x10011;
pub const CVM_HYPERCALL_SET_MAPPING:u32=0x10012;

#[repr(C)] pub struct CvmHypercallCreateVcpuContext
{
	pub handle:CvmHandle,
	pub vcpu_id:u32
}

#[repr(C)] pub struct CvmHypercallDeleteVcpuContext
{
	pub handle:CvmHandle,
	pub vcpu_id:u32
}

#[bitfield(u32)] pub struct CvmHypercallSetMappingAccessInfo
{
	pub read:bool,
	pub write:bool,
	pub execute:bool,
	#[bits(2)] pub page_size:u8,
	#[bits(27)] rsvd:u32
}

#[repr(C)] pub struct CvmHypercallSetMappingContext
{
	pub handle:CvmHandle,
	pub access_info:CvmHypercallSetMappingAccessInfo,
	pub as_id:u32,
	pub pages:u32,
	pub gpa:u64,
	pub hpa:[u64;0]
}

// CVM Virtual Processor Interfaces
pub const CVM_HYPERCALL_RUN_VCPU:u32=0x10020;
pub const CVM_HYPERCALL_REQUEST_EVENT:u32=0x10021;

#[repr(C)] pub struct CvmHypercallRunVcpuContext
{
	pub handle:CvmHandle,
	pub vcpu_id:u32
}

#[repr(C)] pub struct CvmHypercallRequestEventContext
{
	pub handle:CvmHandle,
	pub vcpu_id:u32,
	pub event:CvmRequestedEvent
}