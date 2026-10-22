/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file is the invoker of the yaxpeax Disassembler Crate.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

pub(crate) mod emulator;

#[allow(non_snake_case)]
#[unsafe(no_mangle)]
extern "C" fn NoirInitializeDisassembler()
{
	// For yaxpeax, there is basically nothing to do to initialize it.
}
