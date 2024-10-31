# NoirVisor by Rust
This document describes the plan of remastering NoirVisor with the [Rust](https://www.rust-lang.org) Programming Language. \
By virtue of the formally-verified safety guaranteed by the compiler, core parts of the NoirVisor will be remastered with Rust.

A new branch called `rust-dev` has already been created to [refactor NoirVisor in Rust](https://github.com/Zero-Tang/NoirVisor/tree/rust-dev). It will be merged into the `master` branch after all existing core features (system subversion and customizable virtual machines) are implemented in Rust. Once merged, the retired C codes will be moved to the archive branch.

## Code Architecture
Not all codes are going be remastered with the Rust Programming Language, in that it involves too many works to get started with Rust. This will also drastically slow down the performance of rust-analyzer plugin in VSCode.

Codes that will be remastered with Rust:
- Hypervisor Abstraction
- Intel VT-x Core
- AMD-V Core
- Customizable VM Core
- Basic Development Kits

Codes that will still be written in C:
- Windows Driver Framework
- UEFI Application & Driver Framework
- Cross-Platform Abstraction

Rust codes will be managed with the Cargo package manager. They will be compiled into static libraries and linked via linker. In other words, we will be calling the `cargo` command from the compiling script.