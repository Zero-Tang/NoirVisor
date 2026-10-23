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

use std::{env, path::Path, process::Command};

fn main()
{
	// It seems Rust compiler cannot track header files for global_asm.
	println!("cargo:rerun-if-changed=src/xpf_core/hv_host/asm_helper_x86.inc");
	// Get compiler version.
	let out = Command::new("rustc").arg("--version").output().unwrap();
	let s = String::from_utf8(out.stdout).unwrap();
	println!("cargo:rustc-env=RUSTC_VERSION={s}");
	// Get git version.
	if Path::new(".git").exists()
	{
		// Get the hash of latest commit.
		let out = Command::new("git").args(["rev-parse", "HEAD"]).output().unwrap();
		let hash = String::from_utf8(out.stdout).unwrap();
		// Check if there're uncommitted changes.
		let out = Command::new("git").args(["status", "--porcelain"]).output().unwrap();
		let clean = out.stdout.is_empty();
		// Get the current branch name.
		let out = Command::new("git").args(["branch", "--show-current"]).output().unwrap();
		let br_name = String::from_utf8(out.stdout).unwrap();
		// Format the git version string.
		let s = format!(
			"{} ({}{} @ {})",
			env::var("CARGO_PKG_VERSION").unwrap(),
			&hash[..7],
			if clean { "" } else { "-dirty" },
			br_name.trim()
		);
		println!("cargo:rustc-env=GIT_VERSION={s}");
		// Force a rebuild if git environment changes.
		println!("cargo:rerun-if-changed=.git/HEAD");
		println!("cargo:rerun-if-changed=.git/index");
	}
}
