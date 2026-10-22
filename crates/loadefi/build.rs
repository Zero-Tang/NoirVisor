/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file configures the NoirVisor UEFI loader build.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

fn main()
{
	println!("cargo:rustc-link-arg=/ENTRY:uefi_entry");
	println!("cargo:rustc-link-arg=/SUBSYSTEM:EFI_APPLICATION");
}
