#![no_std]

extern crate alloc;

pub mod hvcall;
pub mod interface;
pub mod ioctl;
pub mod status;

#[cfg(all(any(feature="user",feature="kernel"),not(feature="hypervisor")))] pub mod uefi;
#[cfg(all(windows,any(feature="user",feature="kernel"),not(feature="hypervisor")))] pub mod windows;