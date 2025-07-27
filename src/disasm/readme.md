# NoirVisor Disassembler
This directory is the disassembler engine for NoirVisor.

## Zydis
Zydis is deprecated since NoirVisor is going to adopt the [`iced-x86` crate](https://crates.io/crates/iced-x86) as it is remastering with Rust.

## iced
According to [disas-bench](https://github.com/athre0z/disas-bench), the `iced-x86` disassembler triumphed in the benchmark. This is the other morale of adopting `iced-x86` for NoirVisor's disassembler.

Note that `iced` will cause memory allocations when using formatters.