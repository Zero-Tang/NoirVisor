# NoirVisor Disassembler
This directory is the disassembler engine for NoirVisor.

## Zydis
NoirVisor chooses Zydis as disassembler engine for Project NoirVisor. \
Zydis is licensed under the MIT license. \

## Build
Compilation of Zydis is not included in NoirVisor's compilation script. Hence, you should build Zydis before compiling NoirVisor. Since Zydis is included in Project NoirVisor via git submodule, you should make sure that you have cloned NoirVisor's repository recursively. \
Execute following command to pull Zydis source code:
```
git pull --recurse-submodules
```