/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file is the NoirVisor CVM Scheduler library entry point.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */
#![no_std]
#![feature(allocator_ext)]

extern crate alloc;

pub mod hvcall;
pub mod ioctl;
pub(crate) mod vmm;
#[cfg(test)]
pub(crate) use cvmock as platform;
#[cfg(all(not(test), target_os = "uefi"))]
pub(crate) use uefibsdrv as platform;
#[cfg(all(not(test), target_os = "windows"))]
pub(crate) use windrv as platform;
