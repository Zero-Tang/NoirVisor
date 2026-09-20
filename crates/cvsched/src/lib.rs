// NoirVisor CVM Scheduler
#![no_std]
#![allow(unused_features)]
#![feature(allocator_api)]

extern crate alloc;

pub mod hvcall;
pub mod ioctl;
pub(crate) mod vmm;
#[cfg(test)]
pub(crate) use cvmock as platform;
#[cfg(all(not(test), target_os="windows"))]
pub(crate) use windrv as platform;
#[cfg(all(not(test), target_os="uefi"))]
pub(crate) use uefibsdrv as platform;