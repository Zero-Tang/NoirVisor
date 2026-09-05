// NoirVisor CVM Scheduler
#![no_std]
#![allow(unused_features)]
#![feature(allocator_api)]

extern crate alloc;

pub mod hvcall;
pub mod ioctl;
pub(crate) mod vmm;
#[cfg(target_os="windows")]
pub(crate) use windrv as platform;
#[cfg(target_os="uefi")]
pub(crate) use uefibsdrv as platform;