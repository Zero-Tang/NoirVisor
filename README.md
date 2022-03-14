# NoirVisor
NoirVisor - The Grimoire Hypervisor solution for x86 Processors.

<p align=center>
    <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT">
    <a href="https://discord.gg/5cKn5FdK6U">
        <img src="https://img.shields.io/discord/796222913774354432?color=red&label=Discord&style=flat">
    </a>
</p>

# Introduction
NoirVisor is a hardware-accelerated hypervisor (a.k.a VMM, Virtual Machine Monitor) with support to complex functions and purposes. It is designed to support processors based on x86 architecture with hardware-accelerated virtualization feature. For example, Intel processors supporting Intel VT-x or AMD processors supporting AMD-V meet the requirement. By design, NoirVisor determines the processor manufacturer and selects the function core.

Namesake: NoirVisor is named after the [***Grimoire Noir***](https://nier.fandom.com/wiki/Grimoire_Noir) in NieR:Gestalt/Replicant.

# Processor Requirement
Intel Processors based on Intel 64 and IA-32 Architecture, with support to Intel VT-x. Intel EPT is prefered, but not required. <br>
AMD Processors based on AMD64 Architecture, with support to AMD-V. Nested Paging is prefered, but not required. <br>
Other processors based on x86 architecture may be supported in future. <br>
Currently, it is discovered that x86 processors produced by VIA, Zhaoxin and Hygon supports Hardware-Accelerated Virtualization Technology. In summary, certain facts are observed that:
- Processors produced by Intel Corporation may support Intel VT-x.
- Processors produced by Advanced Micro Devices Inc. may support AMD-V.
- Processors produced by VIA Technologies Inc. may support Intel VT-x.
- Processors produced by Shanghai Zhaoxin Semiconductor Co, Ltd. may support Intel VT-x.
- Processors produced by Tianjin Haiguang Advanced Technology Investment Co, Ltd. may support AMD-V.

Note that early Zhaoxin and VIA use Centaur as vendor.

# Nested Virtualization
NoirVisor is developed in highest focus on nested virtualization. It is not currently supported, but will be developed in future. <br>
Algorithm regarding the Nested Virtualization was stated down in the readme files in both VT-Core and SVM-Core directories. <br>
For Nested Intel VT-x Algorithm, visit [here](src/vt_core/readme.md#vmx-nesting-algorithm-incomplete-version). <br>
For Nested AMD-V Algorithm, visit [here](src/svm_core/readme.md#svm-nesting-algorithm-incomplete-version).

# Announcement to all contributors
NoirVisor is coded in the C programming language and the assembly since it is procedure-oriented designed. <br>
Contributing Guidelines are available in repository. For details, see the markdown file in the root directory of repository. <br>
**DO NOT PROVIDE CODES WITH C++ WHICH INVOLVES THE NoirVisor Core IN YOUR PULL-REQUEST!**

# Build
To build NoirVisor, using batch is essential. <br>
Note that you should execute the `build_prep.bat` to make directories for first-time compilation. <br>
Once NoirVisor is updated, it is recommended to execute `cleanup.bat` script before building.

## Windows Driver
To build a kernel-mode driver on Windows, you should download and mount Enterprise Windows Driver Kit 11 (Visual Studio Build Tools 16.9.2) ISO file to T disk. I recommend using [WinCDEmu](https://wincdemu.sysprogs.org/download/) to mount the ISO on system startup if you are looking for a free virtual ISO Drive. <br>
Then run the provided batch file to build it. You might have to mount the ISO file manually everytime on your machine startup in that I failed to find a script that mount an ISO to a specific drive letter. If you use WinCDEmu, however, you may order the system to mount EWDK10 and specify its drive letter during startup. <br>
You may download the EWDK11 (with VS Build Tools 16.9.2) from Microsoft: https://docs.microsoft.com/en-us/legal/windows/hardware/enterprise-wdk-license-2019-New <br>
Make sure you have downloaded the correct version. NoirVisor would continue updating. If not using correct version, you might fail to compile the latest version of NoirVisor. <br>
Presets for Free/Release build are available. Please note that the compiled binary under Free build does not come along with a digital signature. You might have to sign it yourself.

## EFI Application and Runtime Driver
Due to different EFI firmware implementation, most modern computer firmware does not support booting an EFI Runtime Driver directly. Therefore, it is necessary to build a separate EFI Application. In this way, modern computer firmware will boot, and the application can load runtime driver into memory. <br>
To build a EFI Runtime Driver and Application, you should install LLVM, NASM and TianoCore EDK II. To install TianoCore EDK II, you may download latest release source code and extract to path `C:\UefiDKII`. <br>
You may download NASM from its official website: https://www.nasm.us/pub/nasm/stable/win64/. Make sure you have added the directory to the `PATH` environment variable. <br>
You may download LLVM from GitHub: https://github.com/llvm/llvm-project/releases. Download the Win64 option. <br>
You may download EDK II from GitHub: https://github.com/tianocore/edk2/releases. Download the source code. <br>
NoirVisor also use EDK II Libraries. However, they should be pre-compiled. Visit [EDK-II-Library](https://github.com/Zero-Tang/EDK-II-Library) on GitHub in order to build them.

## Disassembler
Project NoirVisor chooses Zydis as NoirVisor's disassembler engine. You should pre-compile Zydis as a static library. Visit the [documents for disassembler](src/disasm/readme.md) for further details.

# Test

## Windows Driver
There is a .NET Framework 4.0 based GUI loader available on GitHub now: https://github.com/Zero-Tang/NoirVisorLoader <br>
If you are using operating systems older than Windows 8, you are supposed to manually install .NET Framework 4.0 or higher. <br>
If you use the digital signature provided in NoirVisor's repository, then you should enable the test-signing on your machine. <br>
You may enable Stealth SSDT Hook by setting up registry: (If your system is updated with certain patches since 2018, you should, nonetheless, disable Stealth MSR Hook feature. Otherwise, your system could result in #DF failure.) Please note that since hooking is a very dangerous behavior, NoirVisor disables them on default. <br>
**Caveat: The stealth hook functionalities are *deprecated* by virtue of Windows kernel-mode patch dections. They are disabled by default. Future updates of NoirVisor will rarely address issues from them. If you encountered issues from stealth hook features, expect no fixes will be applied. This project has no interest in fixing them.** 
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "StealthMsrHook" /t REG_DWORD /d 1 /f
```
You may enable Stealth Inline Hook by setting up registry:
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "StealthInlineHook" /t REG_DWORD /d 1 /f
```
You may set the values to 0, or remove the value key, in order to disable these features again.

## EFI Application and Runtime Driver
Use a USB flash stick and setup with GUID Partition Table (GPT). Construct a partition and format it info FAT32 file system. After you successfully build the image, you should see two images: `bootx64.efi` and `NoirVisor.efi` <br> 
Those two files are EFI Application and Runtime Driver respectively. <br>
Copy EFI Application to `\EFI\BOOT\bootx64.efi` <br>
Copy EFI Runtime Driver to `\NoirVisor.efi` <br>
As the USB flash stick is ready, enter your firmware settings and set it prior to the operating system. Disable Secure Boot feature unless you can sign the executable. <br>
NoirVisor has defined its own vendor GUID `{2B1F2A1E-DBDF-44AC-DABCC7A130E2E71E}`. Developments regarding Layered Hypervisor would require accessing NoirVisor's UEFI variables.

# Detection of NoirVisor
As specified in AMD64 Architecture Programming Manual, `CPUID.EAX=1.ECX[bit 31]` indicates hypervisor presence. So NoirVisor will set this bit. For CPUID instruction, since AMD defines that function leaves 0x40000000-0x400000FF are reserved for hypervisor use, we will use them. Most hypervisors defines leaf 0x40000000 is used to identify hypervisor vendor. The string constructed by register sequence EBX-ECX-EDX is used to identify vendor of hypervisor. For example, VMware hypervisor vendor string is `VMwareVMware`. In NoirVisor, hypervisor vendor string is defined as `NoirVisor ZT`.

You may disable the detection for NoirVisor in Windows via setting up the registry. <br>
Locate the registry key: `HKLM\Software\Zero-Tang\NoirVisor`. If this key does not exist then create it. <br>
Edit the `CpuidPresence` Key Value to 0. Feel free to execute the following command if you find it less taxing to do: <br>
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "CpuidPresence" /t REG_DWORD /d 0 /f
```

## NoirVisor as a Nested Hypervisor
If NoirVisor is subverting a system under a virtualized environment with exposed detection (e.g: VMware virtual machines with `hypervisor.cpuid.v0 = TRUE` configuration,) as a Type-II hypervisor, the operating system may have already been using functionalities provided by the hypervisor. In this regard, NoirVisor should pass-through the access to hypervisor functionalities (e.g: `cpuid` instructions, accesses to Microsoft Synthetic MSRs, hypercalls, etc.)

## TSC-Omission
Since the end of 2020, NoirVisor implemented a simple Time-Profiler Countermeasure. According to the half-year test, this technique is deemed unstable with multiprocessing systems. For example, TSC-omission may cause external hardwares to trigger drivers resetting themselves. Everything could be messed up: Timer, Graphics Card, NIC, etc. In a nutshell, system may go haywire. <br>
By virtue of this unexpected and unpleasant side-effect, this feature is now obsolete. Codes addressing this feature are now removed.

# Customizable VM
Customizable VM is the true explanation of "complex functions and purposes". As the project creator and director, Zero's true intention to create this project is for studying Hardware-Acclerated Virtualization Technology. Therefore, any features which is related to virtualization and which Zero has ideas to implement will be added in the project. <br>
Customizable VM is the feature that Zero researches about Virtualization: to run an arbitrary guest, instead of to just subvert the host system. In a word, it is aimed to be a competitor of the Windows Hypervisor Platform (WHP). <br>
For CVM Algorithm on AMD-V, visit [here](src/svm_core/readme.md#customizable-vm-scheduler-algorithm).

APIs to invoke Customizable VMs are available in the [NoirCvmApi](https://github.com/Zero-Tang/NoirCvmApi) repository. The documentation of the APIs is available in the [wiki page](https://github.com/Zero-Tang/NoirCvmApi/wiki).

# NPIEP
NPIEP (a.k.a Non-Privileged Instruction Execution Prevention) is an important security feature in Microsoft Virtualization-based Security. As a hypervisor project in conformance to Microsoft `Hv#1` interface, NoirVisor would provide this feature to the guest. This feature is similar to `UMIP` provided by later models of x86 processors. The differences are:

- NPIEP does not raise an exception even if the instruction is executed in user mode.
- NPIEP would prevent the guest from reading the real values of descriptor tables.
- NPIEP does not intercept `smsw` instruction, probably in that Intel VT-x does not support intercepting this instruction.

For further details of NPIEP, visit [here](src/mshv_core/readme.md#non-privileged-instruction-execution-prevention).

# Supported Platforms
NoirVisor is designed to be cross-platform. It can be built to a kernel-mode component of an operating system, or even as a software with bootstrap running on bare-metal. <br>
Currently, NoirVisor supports the Windows Operating System newer than or same as Windows XP, running as a kernel-mode driver. <br>
Porting to Unified Extensible Firmware Interface (UEFI) is in progress. <br>
If there is already a hypervisor running in the system, make sure it supports native virtualization nesting.

# Development Status
Project NoirVisor has seven future development plans: <br>
- Develop Customizable VM engine for complex purposes.
- Develop Nested Virtualization.
- Develop IOMMU Core on Intel VT-d and AMD-Vi.
- Develop NPIEP (Non-Privileged Instruction Execution Prevention) Emulation on Intel VT-x.
- Port NoirVisor to Linux.
- Port NoirVisor to 32-bit Windows platform.
- Port NoirVisor to UEFI and corresponding layered hypervisor.

For more information, check out the [NoirVisor 2020+](https://github.com/Zero-Tang/NoirVisor/projects/2) Project.

# Publications
Here lists some informal publications (blogs) regarding hypervisor development:

- Extending the Tradition Hypervisor's Approach of System Call Hooking in the Post-2018 Windows Operating Systems: https://tangptr.com/?p=149

# Completed Features

- Minimal Microsoft `Hv#1` Hypervisor Functionalities.
- Stealth SSDT Hook (NtOpenProcess Hook) on 64-bit Windows, both Intel VT-x and AMD-V. (**Compatible** with the `KiErrata420Present` mitigation and KVA Shadow mechanism.)
- Stealth Inline Hook (NtSetInformationFile Hook) on 64-bit Windows, both Intel VT-x/EPT and AMD-V/NPT.
- Non-Privileged Instruction Execution Prevention (NPIEP) on AMD-V.
- Customizable VM engine on AMD-V.
- Tagged Translation Lookaside Buffer by ASID/VPID feature.
- Critical Hypervisor Protection.
- Software-Level Code Integrity Enforcement.
- Hardware-Level Code Integrity Enforcement, both Intel EPT and AMD NPT.

# License
This repository is under MIT license. <br>

# Code of Conduct
The Code of Conduct is added to NoirVisor Project since May.5th, 2019. Please follow rule when contributing.