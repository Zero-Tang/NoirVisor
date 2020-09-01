# NoirVisor - MSHV-Core
This directory is the functionalities designed in accordance to Microsoft Hypervisor specifications. <br>
All codes in this directory should be cross-platform designed. <br>
Duly note that this is not a virtualization engine that uses Windows Hypervisor Platform (WHP).

# Minimal Hv#1 Requirement
To implement a minimal Hv#1 interface, there are following requirements: <br>
- CPUID Hypervisor Leaf Maximum: `0x40000005`
- Indicate support of `AccessVpIndex` and `AccessHypercallMsrs` in CPUID instruction.
- Three Synthetic MSRs: `0x40000000` to `0x40000002`
- Hypercall Page Implementation

# Features
Minimal Hv#1 interface support.

# Roadmap
Implement full support to Hv#1 interface support.