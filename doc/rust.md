# NoirVisor by Rust
This document describes the plan of remastering NoirVisor with the [Rust](https://www.rust-lang.org) Programming Language. \
By virtue of the formally-verified safety guaranteed by the compiler, core parts of the NoirVisor will be remastered with Rust.

## Code Architecture
Not all codes are going be remastered with the Rust Programming Language, in that it involves too many works to get started with Rust. For example, there is no official support from Microsoft to build Windows Driver with Rust.

Codes that will be remastered with Rust:
- Hypervisor Abstraction
- Intel VT-x Core
- AMD-V Core
- Basic Development Kits

Codes that will still be written in C:
- Windows Driver Framework
- UEFI Application & Driver Framework
- Cross-Platform Abstraction

Rust codes will be managed with the Cargo package manager. They will be compiled into static libraries and linked via linker. In other words, we will be calling the `cargo` command from the compiling script.