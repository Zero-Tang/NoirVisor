/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file configures the NoirVisor Core build.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

fn main()
{
	// It seems Rust compiler cannot track header files for global_asm.
	println!("cargo:rerun-if-changed=src/xpf_core/hv_host/asm_helper_x86.inc");
}
