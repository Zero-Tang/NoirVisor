// NoirVisor CVM Scheduler
#![no_std]
#![allow(unused_features)]
#![feature(allocator_api)]

extern crate alloc;

pub mod hvcall;
#[cfg(feature="scheduler")]
pub mod ioctl;
#[cfg(feature="scheduler")]
#[allow(dead_code)]
pub(crate) mod sync;
#[cfg(feature="scheduler")]
#[allow(dead_code)]
pub(crate) mod vmm;