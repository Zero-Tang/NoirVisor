#![no_std]
#![feature(allocator_api)]

extern crate alloc;

pub mod hvcall;
pub mod interface;
pub mod ioctl;
pub mod status;

#[cfg(all(target_os="uefi",any(feature="user",feature="kernel"),not(feature="hypervisor")))] pub mod uefi;
#[cfg(all(windows,any(feature="user",feature="kernel"),not(feature="hypervisor")))] pub mod windows;