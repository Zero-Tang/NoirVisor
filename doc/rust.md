# NoirVisor by Rust
This document describes the plan of remastering NoirVisor with the [Rust](https://www.rust-lang.org) Programming Language. \
By virtue of the formally-verified memory-safety guaranteed by the compiler, core parts of the NoirVisor will be remastered with Rust.

## Getting Started
Install [Visual Studio Code](https://code.visualstudio.com) and [rust-analyzer](https://marketplace.visualstudio.com/items?itemName=rust-lang.rust-analyzer) extension.

## Code Architecture
Not all codes are going be remastered with the Rust Programming Language, in that it involves too many works to get started with Rust.

Codes that will be remastered with Rust:
- Hypervisor Abstract Layer
- Intel VT-x Core
- AMD-V Core
- Basic Development Kits

Codes that will still be written in C:
- Platform-specific Driver Framework
- Cross-Platform Abstract Layer

Rust codes will be managed with the Cargo package manager. They will be compiled into static library (`nvcore.lib`) and linked via linker. In other words, we will be calling the `cargo` command from the compiling script. \
Note: it may be counter-intuitive that NoirVisor for UEFI also links to `x86_64-pc-windows-msvc` target, but it just works.

## Allocator
NoirVisor uses [portable-dlmalloc](https://github.com/Zero-Tang/portable-dlmalloc) as the global allocator.

## Coding Style
The coding style for NoirVisor in Rust is probably drastically different than most projects you have seen:

- Use Tab (i.e.: `'\t'`) to indent. Each Tab is considered 4 spaces. \
	When you share codes in NoirVisor via GitHub, make sure to append `?ts=4` suffix on the URL.
- There is no specific limit of line length (e.g.: 9999 characters per line could be tolerable) under the following conditions:
	- Each line should have at most one statement. This means when you typed a semicolon to end the statement, only comments are allowed to the follow on this line. \
		This means short statements are not allowed to merge into the same line, even if they are super short. For example, the following is not allowed:
		```Rust
		a=b;c=d;e=f;g=h;
		```
		You must separate it into different lines:
		```Rust
		a=b;
		c=d;
		e=f;
		g=h;
		```
	- Brace marks (i.e.:`{}`) should go to the new line unless the block has at most one line. For example:
		```Rust
		let c=if a>b {a} else {b};
		let d=unsafe{unsafe_fn()};
		```
- Brace marks should go to the new line. Comments and codes are prohibited on lines with half-braces. For example:
	```Rust
	// Commments can go here.
	if(a>b)	// Comments can go here.
	{
		// Commments can go here.
		fn_true();		// Commments can go here.
		// Commments can go here.
	}
	else	// Comments can go here.
	{
		// Comments can go here.
		fn_false();		// Commments can go here.
		// Commments can go here.
	}
	// Commments can go here.
	```
- Single-line comments must begin with `//`. Do not use `/**/` in most circumstances. This is drastically different from Linux kernel and QEMU.
- Do not use `/**/`, unless you are making documentation, or unless this comment will have more than 3 lines. Similar to the half-brace rule, lines with `/*` and `*/` are prohibited to contain comment texts.
	```Rust
	/*
	 * Line 1
	 * Line 2
	 * Line 3
	 * Line 4
	 * Line 5
	 */
	```
- Functions that are callable in C must begin with `#[no_mangle] pub extern "C" (unsafe) fn`.
- Naming convention is the same to the [Rust default](https://doc.rust-lang.org/1.0.0/style/style/naming/README.html), with following additions:
	- Architecture/Hardware-specific names must begin with its name ID as prefix.
	- Method names must be concise, preferably a verb with optional nouns and/or adverbs.
- Modules that handle VM-Exits forbid using the global allocator. An automated allocator checker will be enforced.