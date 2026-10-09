# NoirVisor Disassembler
This directory is the disassembler engine for NoirVisor.

NoirVisor chooses a disassembler that satisfies following conditions:

- Fast enough, see [disas-bench](https://github.com/athre0z/disas-bench)
- Can decode instructions and provide detailed info
  + Can decode instructions from all operating modes
  + Can decode common MMIO instructions
  + Can decode `mov cr`, `mov dr`, `clts`, `lmsw`, `smsw`, `invlpg` and `int` instruction in order to emulate AMD-V decode-assists feature
- Minimal or no LibC dependency
- Minimal initial allocation, no runtime allocation
- Minimal build process

## Zydis
Zydis is deprecated since NoirVisor is going to adopt the [`iced-x86` crate](https://crates.io/crates/iced-x86) as it is remastering with Rust.

## iced
According to [disas-bench](https://github.com/athre0z/disas-bench), the `iced-x86` disassembler triumphed in the benchmark. This is the other morale of adopting `iced-x86` for NoirVisor's disassembler.

Note that `iced-x86` will cause runtime memory allocations when using formatters. \
Even without formatters, there'd still be more than 2MiB of runtime memory allocation for its internal static variables. \
For this reason, it is retired from NoirVisor in October 2025. NoirVisor would use `yaxpeax-x86` instead.

## yaxpeax
The [yaxpeax-x86](https://github.com/iximeow/yaxpeax-x86) decoder has a similar performance to `iced-x86`, and has a unique advantage against `iced-x86` that it does not have any dynamic memory allocations.