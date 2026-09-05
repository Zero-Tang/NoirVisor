// NoirVisor CVM Hypercall Interface

use crate::interface::{CvmMappingFlags, CvmRequestedEvent};

// CVM Hypervisor Interfaces
pub const CVM_HYPERCALL_GET_CAPABILITY:u32=0x10000;
pub const CVM_HYPERCALL_CREATE_VM:u32=0x10001;
pub const CVM_HYPERCALL_DELETE_VM:u32=0x10002;

#[repr(C)] pub struct CvmHypercallGetCapabilityContext
{
	pub code:u32,
	pub raw:[u32;8]
}

#[repr(C)] pub struct CvmHypercallCreateVmContext
{
	pub handle:u32
}

#[repr(C)] pub struct CvmHypercallDeleteVmContext
{
	pub handle:u32
}

// CVM Virtual Machine Interfaces
pub const CVM_HYPERCALL_CREATE_VCPU:u32=0x10010;
pub const CVM_HYPERCALL_DELETE_VCPU:u32=0x10011;
pub const CVM_HYPERCALL_SET_MAPPING:u32=0x10012;

#[repr(C)] pub struct CvmHypercallCreateVcpuContext
{
	pub handle:u32,
	pub vcpu_id:u32,
	pub vpcb_hpa:u64
}

#[repr(C)] pub struct CvmHypercallDeleteVcpuContext
{
	pub handle:u32,
	pub vcpu_id:u32
}

#[repr(C)] pub struct CvmHypercallSetMappingContext
{
	pub handle:u32,
	pub flags:CvmMappingFlags,
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
	pub handle:u32,
	pub vcpu_id:u32
}

#[repr(C)] pub struct CvmHypercallRequestEventContext
{
	pub handle:u32,
	pub vcpu_id:u32,
	pub event:CvmRequestedEvent
}