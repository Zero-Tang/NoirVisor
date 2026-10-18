# NoirVisor Disassembler
This directory is the disassembler engine for NoirVisor.

NoirVisor chooses a disassembler that satisfies following conditions:

- Fast enough, see [disas-bench](https://github.com/athre0z/disas-bench)
- Can decode instructions and provide detailed info
  + Can decode instructions from all operating modes
  + Can decode common MMIO instructions
  + Can decode `mov cr`, `mov dr`, `clts`, `lmsw`, `smsw`, `invlpg` and `int` instruction in order to emulate AMD-V decode-assists feature
  + Can decode instruction length for any instructions that may cause VM-Exits.
- Minimal or no LibC dependency, no OS-specific dependency
- Minimal initial allocation, no runtime allocation
- Minimal build process
