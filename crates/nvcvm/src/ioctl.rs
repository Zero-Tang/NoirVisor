// NoirVisor CVM IOCTL Codes and Structures

use crate::{interface::{CvmHandle, CvmMapping, CvmRequestedEvent, Vpcb}, status::Status};

pub const IOCTL_CURRENT_VERSION:u32=1;

pub const IOCTL_CODE_HV_GET_CAPABILITY:usize=0x00;
pub const IOCTL_CODE_HV_CREATE_VM:usize=0x01;
pub const IOCTL_CODE_HV_DELETE_VM:usize=0x02;

pub const IOCTL_CODE_VM_CREATE_VCPU:usize=0x10;
pub const IOCTL_CODE_VM_DELETE_VCPU:usize=0x11;
pub const IOCTL_CODE_VM_SET_MEMORY_REGION:usize=0x12;

pub const IOCTL_CODE_VCPU_RUN:usize=0x20;
pub const IOCTL_CODE_VCPU_REQUEST_EVENT:usize=0x21;

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmGetCapabilityInputBuffer
{
	pub version:u32,
	pub capability_code:u32
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmGetCapabilityOutputBuffer
{
	pub version:u32,
	pub status:u32,
	pub capability:[u32;8]
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmCreateVmInputBuffer
{
	pub version:u32
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmCreateVmOutputBuffer
{
	pub version:u32,
	pub status:Status,
	pub vm_handle:CvmHandle
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmDeleteVmInputBuffer
{
	pub version:u32,
	pub vm_handle:CvmHandle
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmDeleteVmOutputBuffer
{
	pub version:u32,
	pub status:Status,
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmCreateVcpuInputBuffer
{
	pub version:u32,
	pub vm_handle:CvmHandle,
	pub vcpu_id:u32
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmCreateVcpuOutputBuffer
{
	pub version:u32,
	pub status:Status,
	pub vpcb_uva:*mut Vpcb
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmDeleteVcpuInputBuffer
{
	pub version:u32,
	pub vm_handle:CvmHandle,
	pub vcpu_id:u32
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmDeleteVcpuOutputBuffer
{
	pub version:u32,
	pub status:Status
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmSetMappingInputBuffer
{
	pub version:u32,
	pub vm_handle:CvmHandle,
	pub hva:u64,
	pub info:CvmMapping
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmSetMappingOutputBuffer
{
	pub version:u32,
	pub status:Status
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmRunVcpuInputBuffer
{
	pub version:u32,
	pub vm_handle:CvmHandle,
	pub vcpu_id:u32
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmRunVcpuOutputBuffer
{
	pub version:u32,
	pub status:Status
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmRequestEventInputBuffer
{
	pub version:u32,
	pub vm_handle:CvmHandle,
	pub vcpu_id:u32,
	pub info:CvmRequestedEvent
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmRequestEventOutputBuffer
{
	pub version:u32,
	pub status:Status
}