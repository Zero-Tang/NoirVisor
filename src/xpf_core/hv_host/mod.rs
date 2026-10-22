/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file declares host hypervisor modules and hypercall codes.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

pub mod x86;

pub const NOIR_HYPERCALL_CODE_CALLEXIT: u32 = 0x1;
pub const NOIR_HYPERCALL_CODE_REGISTER_PAGES: u32 = 0x2;
pub const NOIR_HYPERCALL_CODE_EXIT_BOOT_SERVICES: u32 = 0x3;
