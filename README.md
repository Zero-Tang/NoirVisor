# NoirVisor
NoirVisor - Hardware-Accelerated Hypervisor solution with support to complex functions and purposes and nested virtualization.

# Introduction
NoirVisor is a hardware-accelerated hypervisor (a.k.a VMM, Virtual Machine Monitor) with support to complex functions and purposes. It is designed to support processors based on x86 architecture with hardware-accelerated virtualization feature. For example, Intel processors supporting Intel VT-x or AMD processors supporting AMD-V meet the requirement. By designation, NoirVisor determines the processor manufacturer and selects the function core.

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
Algorithm regarding the Nested Virtualization was stated down in the readme files in both VT-Core and SVM-Core directories.

# Announcement to all contributors
NoirVisor is coded in the C programming language and the assembly since it is procedure-oriented designed. <br>
Contributing Guidelines are available in repository. For details, see the markdown file in the root directory of repository. <br>
DO NOT PROVIDE CODES WITH C++ WHICH INVOLVES THE NoirVisor Core IN YOUR PULL-REQUEST!

# Build
To build NoirVisor, using batch is essential. <br>
Note that you should execute the `build_prep.bat` to make directories for first-time compilation. 

## Windows Driver
To build a kernel-mode driver on Windows, you should download and mount Enterprise Windows Driver Kit 10 (version 2004) ISO file to T disk. I recommend using [WinCDEmu](https://wincdemu.sysprogs.org/download/) to mount the ISO on system startup if you are not using Windows 10. <br>
Then run the provided batch file to build it. You might have to mount the ISO file manually everytime on your machine startup in that I failed to find a script that mount an ISO to a specific drive letter. If you use WinCDEmu, however, you may order the system to mount EWDK10 and specify its drive letter during startup. <br>
You may download the EWDK10-2004 (with VS Build Tools 16.7) from Microsoft: https://docs.microsoft.com/en-us/legal/windows/hardware/enterprise-wdk-license-2019 <br>
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
If you use the digital signature provided in NoirVisor's repository, then you should enable the test-signing on your machine.

## EFI Application and Runtime Driver
Use a USB flash stick and setup with GUID Partition Table (GPT). Construct a partition and format it info FAT32 file system. After you successfully build the image, you should see two images: `bootx64.efi` and `NoirVisor.efi` <br> 
Those two files are EFI Application and Runtime Driver respectively. <br>
Copy EFI Application to `\EFI\BOOT\bootx64.efi` <br>
Copy EFI Runtime Driver to `\NoirVisor.efi` <br>
As the USB flash stick is ready, enter your firmware settings and set it prior to the operating system. Disable Secure Boot feature unless you can sign the executable.

# Detection of NoirVisor
As specified in AMD64 Architecture Programming Manual, `CPUID.EAX=1.ECX[bit 31]` indicates hypervisor presence. So NoirVisor will set this bit. For CPUID instruction, since AMD defines that function leaves 0x40000000-0x400000FF are reserved for hypervisor use, we will use them. Most hypervisors defines leaf 0x40000000 is used to identify hypervisor vendor. The string constructed by register sequence EBX-ECX-EDX is used to identify vendor of hypervisor. For example, VMware hypervisor vendor string is `VMwareVMware`. In NoirVisor, hypervisor vendor string is defined as `NoirVisor ZT`.

You may disable the detection for NoirVisor in Windows via setting up the registry. <br>
Locate the registry key: `HKLM\Software\Zero-Tang\NoirVisor`. If this key does not exist then create it. <br>
Edit the `CpuidPresence` Key Value to 0. If not exist then create it using following command: <br>
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "CpuidPresence" /t REG_DWORD /d 1 /f
```
The TSC due to VM-Exit is always omitted in Exit Handler. This feature can not be disabled. Please note that omitted TSC is approximate and thereby cannot counter precise time-profiler.

# Supported Platforms
NoirVisor is designed to be cross-platform. It can be built to a kernel-mode component of an operating system, or even as a software with bootstrap running on bare-metal. <br>
Currently, NoirVisor supports the Windows Operating System newer than or same as Windows XP, running as a kernel-mode driver. <br>
Porting to Unified Extensible Firmware Interface (UEFI) is in progress. <br>
If there is already a hypervisor running in the system, make sure it supports native virtualization nesting.

# Development Status
Project NoirVisor has four future development plans: <br>
- Develop Nested Virtualization.
- Develop IOMMU Core.
- Port NoirVisor to 32-bit Windows platform.
- Port NoirVisor to UEFI and corresponding layered hypervisor.

For more information, check out the [NoirVisor 2020+](https://github.com/Zero-Tang/NoirVisor/projects/2) Project.

# Completed Features
- Minimal Microsoft `Hv#1` Hypervisor Functionalities.
- Stealth SSDT Hook (NtOpenProcess Hook) on 64-bit Windows, both Intel VT-x and AMD-V.
- Stealth Inline Hook (NtSetInformationFile Hook) on 64-bit Windows, Intel VT-x.
- TSC Offseting as Countermeasure for TSC-based Time-Profiler.
- Tagged Translation Lookaside Buffer by ASID/VPID feature.
- Critical Hypervisor Protection.
- Software-Level Code Integrity Enforcement.
- Hardware-Level Code Integrity Enforcement, both Intel EPT and AMD NPT.

# License
This repository is under MIT license. <br>

# Code of Conduct
The Code of Conduct is added to NoirVisor Project since May.5th, 2019. Please follow rule when contributing.