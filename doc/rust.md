# NoirVisor by Rust
This document describes the plan of remastering NoirVisor with the [Rust](https://www.rust-lang.org) Programming Language. \
By virtue of the formally-verified memory-safety guaranteed by the compiler, core parts of the NoirVisor will be remastered with Rust.

## Getting Started
Install [Visual Studio Code](https://code.visualstudio.com) and [rust-analyzer](https://marketplace.visualstudio.com/items?itemName=rust-lang.rust-analyzer) extension.

A new branch called `rust-dev` has already been created to [refactor NoirVisor in Rust](https://github.com/Zero-Tang/NoirVisor/tree/rust-dev). It will be merged into the `master` branch after all existing core features (system subversion and customizable virtual machines) are implemented in Rust. Once merged, the retired C codes will be moved to the archive branch.

## Code Architecture
All C codes are going be remastered with the Rust Programming Language.

System Assembly codes will be remastered with LLVM's module-level inline assembler as they support Microsoft's stack-unwinding directives. \
Trivial assembly codes will be remastered with Rust inline assembly.

Rust codes will be managed with the Cargo package manager. They will be compiled into static library and linked via linker. In other words, we will be calling the `cargo` command from the compiling script.

NoirVisor uses Rust 2024 standard.

## Allocator
NoirVisor uses [portable-dlmalloc](https://github.com/Zero-Tang/portable-dlmalloc) as the global allocator. \
You should avoid using allocators while in VM-Exit handlers! For example, you should use `sort_unstable` instead of `sort` to avoid allocation.

## Automated Testing
NoirVisor uses `cargo test` suite to test NoirVisor. However, system subversion and restoration are not currently included in tests yet. \
To develop test cases, you should toggle `rust-analyzer.cfg.setTest` to true in `rust-analyzer` plugin and then reload VSCode.

## Logging
While we do provide macros like `print!` and `println!`, please use `error!`, `warn!`, `info!`, `debug!` and `trace!` macro provided by [the `log` crate](https://docs.rs/log/latest/log/). These logging macros are only usable in host mode. \
However, if you wish to print logs in codes that would run in guest mode, please use `sysdprint` and `sysdprintln` macro instead. Otherwise, it may cause mutex recursion.

## VSCode Setup
You may add a `.vscode/settings.json` file to configure the behavior of `rust-analyzer` plugin. The `.vscode/` directory is ignored by `git` so feel free to configure however you want. \
You would most likely want to configure the `rust-analyzer.cargo.target` variable in order to switch to either Windows or UEFI.

### UEFI Development
Create a `.vscode/settings.json` file in NoirVisor root source directory, and write the following:
```json
{
	"rust-analyzer.cfg.setTest": false,
	"rust-analyzer.cargo.target": "x86_64-unknown-uefi"
}
```

### Windows Driver Development
Create a `.vscode/settings.json` file in NoirVisor root source directory, and write the following:
```json
{
	"rust-analyzer.cfg.setTest": false,
	"rust-analyzer.cargo.target": "x86_64-pc-windows-msvc"
}
```

### Test Cases
Make sure `rust-analyzer.cfg.setTest` json entry is set to `true`. However, you need to set it to `false` if you are modifying the panic handler.

## Coding Style
The coding style for NoirVisor in Rust is probably drastically different than most projects you have seen:

- Use Tab (i.e.: `'\t'`) to indent. Each Tab is considered 4 spaces. \
	When you share codes in NoirVisor via GitHub, make sure to append `?ts=4` suffix on the URL.
- There is no specific limit of line length (e.g.: 9999 characters per line could be tolerable) under the following conditions:
	- Each line should have at most one statement. This means when you typed a semicolon to end the statement, only comments are allowed to the follow on this line. \
		This means short statements are not allowed to merge into the same line, even if they are super short. For example, the following is not allowed:
		```Rust
		a=b;c=d;e=f;g=h;
		j();k();l();
		```
		You must separate it into different lines:
		```Rust
		a=b;
		c=d;
		e=f;
		g=h;
		j();
		k();
		l();
		```
		Or if viable, merge into tuple assignments:
		```Rust
		(a,c,e,g)=(b,d,f,h);
		j();
		k();
		l();
		```
	- Brace marks (i.e.:`{}`) should go to the new line unless the block has at most one line. For example:
		```Rust
		let c=if a>b {a} else {b};
		let d=unsafe{unsafe_fn()};
		```
- Brace marks should go to the new line. Comments and codes are prohibited on lines with half-braces. For example:
	```Rust
	// Commments can go here.
	if a>b	// Comments can go here.
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
- Do not use `/**/` unless for the source file header.
- For documentation comments, use `///` instead of `/***/`.
- Functions that are callable in C must begin with `#[unsafe(no_mangle)] (pub) (unsafe) extern "C" fn`. The `pub` and `unsafe` keywords are not required.
- Naming convention is the same to the [Rust default](https://doc.rust-lang.org/1.0.0/style/style/naming/README.html), with following additions:
	- Architecture/Hardware-specific names must begin with its name ID as prefix.
	- Method names must be concise, preferably a verb with optional nouns and/or adverbs.
- Modules that handle VM-Exits forbid using the global allocator. An automated allocator checker will be enforced in future.