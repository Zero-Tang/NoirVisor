/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file is the entry point for the NoirVisor CVM interface library.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![no_std]

extern crate alloc;

pub mod hvcall;
pub mod interface;
pub mod ioctl;
pub mod status;

#[cfg(all(any(feature = "user", feature = "kernel"), not(feature = "hypervisor")))]
pub mod uefi;
#[cfg(all(windows, any(feature = "user", feature = "kernel"), not(feature = "hypervisor")))]
pub mod windows;
