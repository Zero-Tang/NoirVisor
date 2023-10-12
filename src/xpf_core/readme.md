# NoirVisor XPF-Core
This directory is not a virtualization engine for NoirVisor. Instead, this file implement basic utilities for NoirVisor virtualization engines. \
Codes in this directory should be platform-specific designed. However, function names should have the same style to virtualization engines. \

# Folders
There should be multiple folders in this directory. Each folder indicates the platform it supports. Currently, NoirVisor supports the Windows platform, and the support to UEFI platform is in progress. Thus there are a "windows" folder and a "uefi" folder in XPF-Core directory.

# Features
In XPF-Core, one folder should have following features for its specific platform: \
Allocate and free memory, with types of: NonPaged, Paged and Contiguous. Allocated memory should be initialized with zeros. \
Print formatted debug message to debugger. \
Save processor state, including:
- Segment Registers.
- Control Registers.
- Debug Registers.
- Model Specific Registers.
- Perform generic call to all processors.
- Query processor count.
- Partial virtualization engine code that necessarily written in assembly.

# CI (Code Integrity)
Code Integrity is a component that ensures codes in NoirVisor is not tampered by malicious software. \
It works like PatchGuard in 64-bit Windows. In NoirVisor, checksum of CI is implemented by CRC32 Castagnoli Algorithm. \
Real-Time Code Integrity will work like HyperGuard in Windows. The key point is that NoirVisor will not crash the system.

# Debugger
NoirVisor integrates an internal debugger for debugging hypervisor from remote. Currently, NoirVisor supports debugging over serial connection.

# Roadmap
Implement support to other platforms (e.g Linux, MacOS, etc...).