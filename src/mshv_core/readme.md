# NoirVisor - MSHV-Core
This directory is the functionalities designed in accordance to Microsoft Hypervisor specifications. \
All codes in this directory should be cross-platform designed. \
Duly note that this is not a virtualization engine that uses Windows Hypervisor Platform (WHP).

# Minimal Hv#1 Requirement
To implement a minimal Hv#1 interface, there are following requirements: \
- CPUID Hypervisor Leaf Maximum: `0x40000005`.
- Indicate support of `AccessVpIndex` and `AccessHypercallMsrs` in CPUID instruction.
- Three Synthetic MSRs: `0x40000000` to `0x40000002`.
- Hypercall Page Implementation.

Reference: [Requirements for Implementing the Microsoft Hypervisor Interface](https://raw.githubusercontent.com/MicrosoftDocs/Virtualization-Documentation/master/tlfs/Requirements%20for%20Implementing%20the%20Microsoft%20Hypervisor%20Interface.pdf)

# Features
NoirVisor support a few features of `Hv#1` interface. Any additional `Hv#1` interface features supported by NoirVisor will be listed below.

## Non-Privileged Instruction Execution Prevention
This feature prevents the `sgdt`, `sidt`, `sldt` and `str` instructions to be executed in user mode. \
You may enable NPIEP by configuring `MSR[0x40000040]`. (The `HV_X64_MSR_NPIEP_CONFIG` Synthetic MSR) \
The bit fields of this MSR is defined by Microsoft as the following:
```C
union
{
    UINT64 AsUINT64;
    struct
    {
        // These bits enable instruction execution prevention for specific
        // instructions.

        UINT64 PreventSgdt:1;
        UINT64 PreventSidt:1;
        UINT64 PreventSldt:1;
        UINT64 PreventStr:1;
        UINT64 Reserved:60;
    };
} HV_X64_MSR_NPIEP_CONFIG_CONTENTS;
```
In that NPIEP is a feature of Microsoft Hypervisor, you must indicate `CPUID Presence` of NoirVisor.

### Interception Logic
Considering that there is also a processor feature called `UMIP` (User-Mode Instruction Prevention), a feature that prevents these instructions to be executed in user mode: executions of these instructions in user would trigger `#GP(0)` exceptions. In this regard, it becomes unnecessary for NoirVisor to intercept these instructions: intercepted instructions are always in the kernel mode. \
Here, in the following, lists some points of the interception logics of the NPIEP feature.

- If the processor supports the `UMIP` feature, monitor the `CR4.UMIP` bit. Otherwise, do not monitor the `CR4.UMIP` bit.
- If `CR4.UMIP` is to be set, stop the NPIEP functionality. For Intel VT-x, cancel `descriptor-table exiting` in VMCS. For AMD-V, cancel `sidt`, `sgdt`, `sldt` and `str` interceptions in VMCB.
- If `CR4.UMIP` is to be cleared, resume the NPIEP if it was previously running. Reconfigure the interception according to the content in the MSR `HV_X64_MSR_NPIEP_CONFIG`.
- If writes to the `HV_X64_MSR_NPIEP_CONFIG` are intercepted, save the written value to vCPU. However, `CR4.UMIP` bit must be checked before reconfiguring the interceptions.

### Toggle NPIEP Manually
The idea to toggle NPIEP is actually very simple: write to the `HV_X64_MSR_NPIEP_CONFIG` Synthetic MSR (`MSR[0x40000000]`). If your system is being debug with WinDbg, type the following command to enable NPIEP:
```
wrmsr 40000040 f
```
Similarly, type the following command to disable NPIEP:
```
wrmsr 40000040 0
```
You may write your own kernel-mode program to toggle them by executing the `wrmsr` instruction.

# Roadmap
Implement full support to `Hv#1` interface.