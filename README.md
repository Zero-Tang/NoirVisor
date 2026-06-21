# NoirVisor
NoirVisor - The Grimoire Hypervisor solution for AMD64 Processors.

<p align=center>
    <img src="https://img.shields.io/github/license/Zero-Tang/NoirVisor?color=blue&style=flat">
    <a href="https://discord.gg/5cKn5FdK6U">
        <img src="https://img.shields.io/discord/796222913774354432?color=red&label=Discord&style=flat">
    </a>
    <img src="https://img.shields.io/github/stars/Zero-Tang/NoirVisor?color=orange">
    <img src="https://img.shields.io/github/forks/Zero-Tang/NoirVisor?color=silver">
    <a href="https://qm.qq.com/cgi-bin/qm/qr?k=ly7ROfTm6VD9pBuw6zI85TuYaWCu3li8&jump_from=webapi">
        <img border="0" src="https://pub.idqqimg.com/wpa/images/group.png" alt="NoirVisor虚拟化交流群" title="769616136">
    </a>
</p>

Tips: if the link for QQ Group does not work, try to hover the icon over the shield icon and see text.

# Introduction
NoirVisor is a hardware-accelerated hypervisor (a.k.a VMM, Virtual Machine Monitor) with support to complex functions and purposes. It is designed to support processors based on x86 architecture with hardware-accelerated virtualization feature. For example, Intel processors supporting Intel VT-x or AMD processors supporting AMD-V meet the requirement. By design, NoirVisor determines the processor manufacturer and selects the function core.

Namesake: NoirVisor is named after the [***Grimoire Noir***](https://nier.fandom.com/wiki/Grimoire_Noir) in NieR:Gestalt/Replicant.

# Processor Requirement
Intel Processors based on Intel 64 and IA-32 Architecture, with support to Intel VT-x /w EPT. \
AMD Processors based on AMD64 Architecture, with support to AMD-V /w NPT. \
Other processors based on x86 architecture may be supported in future. \
Currently, it is discovered that x86 processors produced by VIA, Zhaoxin and Hygon supports Hardware-Accelerated Virtualization Technology. In summary, certain facts are observed that:
- Processors produced by Intel Corporation may support Intel VT-x.
- Processors produced by Advanced Micro Devices Inc. may support AMD-V.
- Processors produced by VIA Technologies Inc. may support Intel VT-x.
- Processors produced by Shanghai Zhaoxin Semiconductor Co, Ltd. may support Intel VT-x.
- Processors produced by Tianjin Haiguang Advanced Technology Investment Co, Ltd. may support AMD-V.
- Processors produced by Montage Technologies may support Intel VT-x.

Note that early Zhaoxin and VIA use Centaur as vendor.

After refactoring NoirVisor Core with Rust, Intel EPT and AMD NPT are now required to boot NoirVisor for security reasons!

# Nested Virtualization
Algorithm regarding the Nested Virtualization was written in the readme files in both VT-Core and SVM-Core directories. \
For Nested Intel VT-x Algorithm, visit [here](src/vt_core/readme.md#vmx-nesting-algorithm-incomplete-version). \
For Nested AMD-V Algorithm, visit [here](src/svm_core/readme.md#svm-nesting-algorithm-incomplete-version).

Nested AMD-V is not supported yet. \
Nested Intel VT-x is not supported yet.

# Announcement to all contributors
NoirVisor is coded in the C programming language, Assembly and Rust. \
**NO C++ CODES ARE ACCEPTED IN THIS PROJECT!**

You should consult the [contribution guidelines](./contributing.md) and [code of conduct](./code_of_conduct.md) while making contributions.

## Rust
If your patch includes Rust codes, make sure it could pass `cargo clippy --all` checks! No warnings and errors are allowed. \
If you believe `clippy` is prompting dubious warnings and errors, do not put `#[allow(...)]` on your own. Instead, report in your PR and state why you think that is false positive. \
If it is really is false positive, relevant suppression will be put.

For your convenience, if `clippy` prompted too many errors and warnings, you may double click `clippy.bat` to restrict the output in one window.

## Git
To contribute to NoirVisor, you need to make a fork of this repository to your own account. Submit changes to your forked repository. When your patch completes, submit a [PR (short for Pull Request)](https://github.com/Zero-Tang/NoirVisor/pulls).

If your patch is trivial, there isn't no specific commit history requirement. The PR will merge to `master` branch with the **Squash** strategy. You need to "force" a synchronization between the fork after the merge.

If your patch is substantial, you need to maintain a strict commit history. This PR will merge to `master` branch with the **Merge-Commit** strategy. The rules for commit history are:

- Changes in each commit belongs to the same purpose. No all-in-one commit.
- No "nit" commits. You need to amend or squash your commits.
- To synchronize new changes from `master` branch to your fork, use the **Rebase** strategy.

# Build
We use Python script to build NoirVisor. The minimum version required for building NoirVisor is 3.9 by virtue of the typing syntax. In other words, building NoirVisor through Python script in Windows 7 is not supported. There are no `pip` package requirements for compilation.

Python-based compilation is parallel. It will achieve a great performance in building NoirVisor.

See [documentation](./doc/make.md) for more information using python script to build NoirVisor.

**Note that the `cargo build` command only builds the NoirVisor Core instead of the whole NoirVisor project!**

**TL;DR?** In short, make sure Python, Rust Nightly and MSVC (including WDK) are installed. \
To build Windows Driver:
```
make
```
To build UEFI Application & Runtime Driver:
```
make /target uefi
```

# Test

## Windows Driver
There is a .NET Framework 4.0 based GUI loader available on GitHub now: https://github.com/Zero-Tang/NoirVisorLoader \
If you are using operating systems older than Windows 8, you are supposed to manually install .NET Framework 4.0 or higher. \
If you use the digital signature provided in NoirVisor's repository, then you should enable the test-signing on your machine. \
You may enable Stealth SSDT Hook by setting up registry. Please note that since hooking is a very dangerous behavior, NoirVisor disables them on default. \
**Caveat: The stealth hook functionalities are *deprecated* in that I'm tired of doing this. They are disabled by default. Future updates of NoirVisor will rarely address issues from them. If you encountered issues from stealth hook features, expect no fixes will be applied. This project has no interest in fixing them.** 
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "StealthMsrHook" /t REG_DWORD /d 1 /f
```
You may enable Stealth Inline Hook by setting up registry:
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "StealthInlineHook" /t REG_DWORD /d 1 /f
```
You may set the values to 0, or remove the value key, in order to disable these features again.

You may load NoirVisor by using command-line or batch script:
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "SubvertOnDriverLoad" /t REG_DWORD /d 1 /f
sc create NoirVisor type= kernel binPath= <Path to NoirVisor driver file>
sc start NoirVisor
```
You may unload NoirVisor by using command-line or batch script as well:
```bat
sc stop NoirVisor
sc delete NoirVisor
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "SubvertOnDriverLoad" /t REG_DWORD /d 0 /f
```
The `SubvertOnDriverLoad` registry key value specifies whether the driver should subvert the system or not on the entry. This key value conflicts with NoirVisor Loader. You must delete or disable this key value in order to use NoirVisor Loader.

## EFI Application and Runtime Driver
There are two methods to test NoirVisor.

### Running on a physical machine
This method can also be used on VMware. \
Use a USB flash stick and setup with GUID Partition Table (GPT). Construct a partition and format it info FAT32 file system. After you successfully build the image, you should see two images: `bootx64.efi` and `NoirVisor.efi` \
Those two files are EFI Application and Runtime Driver respectively. \
Copy EFI Application to `\EFI\BOOT\bootx64.efi` \
Copy EFI Runtime Driver to `\NoirVisor.efi` \
As the USB flash stick is ready, enter your firmware settings and set it prior to the operating system. Disable Secure Boot feature unless you can sign the executable. \
NoirVisor has defined its own vendor GUID `{2B1F2A1E-DBDF-44AC-DABCC7A130E2E71E}`. Developments regarding Layered Hypervisor would require accessing NoirVisor's UEFI variables.

### Running on a virtual machine
The point of this method is to build a virtual disk image. \
You may use `mtools` in order to make a virtual disk image. The pre-built `mtools` executables are provided [here](https://github.com/Zero-Tang/NoirVisor/files/12706542/mtools-4.0.43-bin.zip). Put them into directories listed in `PATH` environment variable.

Build script for NoirVisor on UEFI includes above commands. Add `NoirVisor-Uefi.img` as a floppy image in your virtual machine.

### Running on Emulator
**QEMU**: In the `tests` directory, execute the `run_qemu.py` script. Note that QEMU TCG accelerator only supports AMD-V!
```
python run_qemu.py
```
If KVM is available:
```
python run_qemu.py -accel kvm
```
If you want to emulate more processors:
```
python run_qemu.py -smp 4
```
If you want to run the optimized binary:
```
python run_qemu.py -release
```
If you want to enable GDB debug stub at `localhost:1234`:
```
python run_qemu.py -gdb
```
If you want to test IOMMU for NoirVisor:
```
python run_qemu.py -iommu intel|amd
```
If you're using [the QEMU fork which contains the virtual PCILeech device](https://github.com/qemu-pcileech/qemu), you may enable the virtual PCILeech device to listen on 6789 port:
```
python run_qemu.py -iommu intel|amd -pcileech
```

Note that QEMU TCG may output debug logs. Pass `-debug` argument to this script in order to give `-d` argument to QEMU. You might most likely be interested in giving `-d int` argument in order to log exceptions. In other words:
```
python run_qemu.py -debug int
```
Debug log is written into `qemu.log`.

**Bochs**: In the `tests` directory, execute the `run_bochs.py` script. Note that only [Bochs-3.0](https://sourceforge.net/projects/bochs/files/bochs/3.0/) is supported. \
To emulate Intel VT-x in Bochs:
```
python run_bochs.py --cpu-model corei7_icelake_u
```
To emulate AMD-V in Bochs:
```
python run_bochs.py --cpu-model ryzen
```
To run the optimized binary in Bochs:
```
python run_bochs.py --release
```
Bochs does not support emulating IOMMU (i.e.: Intel VT-d and AMD-Vi) yet. To test IOMMU functionality, only QEMU is supported.

# Documents
This repository provides [additional documents](/doc/readme.md) which help new developers to join development.

# Detection of NoirVisor
As specified in AMD64 Architecture Programming Manual, `CPUID.EAX=1.ECX[bit 31]` indicates hypervisor presence. So NoirVisor will set this bit. For CPUID instruction, since AMD defines that function leaves 0x40000000-0x400000FF are reserved for hypervisor use, we will use them. Most hypervisors would use leaf 0x40000000 in order to identify itself as the hypervisor vendor. The string constructed by register sequence EBX-ECX-EDX is used to identify vendor of hypervisor. For example, VMware hypervisor vendor string is `VMwareVMware`. In NoirVisor, hypervisor vendor string is defined as `NoirVisor ZT`.

You may disable the detection for NoirVisor in Windows via setting up the registry. \
Locate the registry key: `HKLM\Software\Zero-Tang\NoirVisor`. If this key does not exist then create it. \
Edit the `CpuidPresence` Key Value to 0. Feel free to execute the following command if you find it less taxing to do:
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "CpuidPresence" /t REG_DWORD /d 0 /f
```

You may disable the detection for NoirVisor in UEFI via building the custom binary configuration file. \
You may copy the [DefaultUefiConfig.json](./build/DefaultUefiConfig.json), and set the `CpuidPresence` to `false`. Then use the provided python script to build it:
```
python makeueficonfig.py NewUefiConfig.json NoirVisorConfig.bin
```
You should see a generated file called `NoirVisorConfig.bin`. Place it to the root directory of the boot medium.

## NoirVisor as a Nested Hypervisor
If NoirVisor is subverting a system under a virtualized environment with exposed detection (e.g: VMware virtual machines with `hypervisor.cpuid.v0 = TRUE` configuration) as a Type-II hypervisor, the operating system may have already been using functionalities provided by the hypervisor. In this regard, NoirVisor should pass-through the access to hypervisor functionalities (e.g: `cpuid` instructions, accesses to Microsoft Synthetic MSRs, hypercalls, etc.)

# Customizable VM
Customizable VM is the true explanation of "complex functions and purposes". As the project creator and director, Zero's true intention to create this project is for studying Hardware-Acclerated Virtualization Technology. Therefore, any features which is related to virtualization and which Zero has ideas to implement will be added in the project. \
Customizable VM is the feature that Zero researches about Virtualization: to run an arbitrary guest, instead of to just subvert the host system. In a word, it is aimed to be a competitor of the Windows Hypervisor Platform (WHP). \
For CVM Algorithm on AMD-V, visit [here](src/svm_core/readme.md#customizable-vm-scheduler-algorithm). \
For CVM Algorithm on Intel VT-x, visit [here](src/vt_core/readme.md#customizable-vm-scheduler-algorithm).

APIs to invoke Customizable VMs are available in the [NoirCvmApi](https://github.com/Zero-Tang/NoirCvmApi) repository. The documentation of the APIs is available in the [wiki page](https://github.com/Zero-Tang/NoirCvmApi/wiki).

# NoirVisor Secure Virtualization
NSV (a.k.a NoirVisor Secure Virtualization) is a security extension to NoirVisor CVM. This extension is a crossover project with Columbia University's Operating Systems II course project. Read [this document](./doc/nsv.md) for further details.

# NPIEP
NPIEP (a.k.a Non-Privileged Instruction Execution Prevention) is an important security feature in Microsoft Virtualization-based Security. As a hypervisor project in conformance to Microsoft `Hv#1` interface, NoirVisor would provide this feature to the guest. This feature is similar to `UMIP` provided by later models of x86 processors. The differences are:

- NPIEP does not raise an exception even if the instruction is executed in user mode.
- NPIEP would prevent the guest from reading the real values of descriptor tables.
- NPIEP does not intercept `smsw` instruction, probably in that Intel VT-x does not support intercepting this instruction.

For further details of NPIEP, visit [here](src/mshv_core/readme.md#non-privileged-instruction-execution-prevention).

# Security Advisories
You should not report security vulnerabilities through the GitHub issue. You should [read this document](./security.md) to check out the steps to report security vulnerability.

# Supported Platforms
NoirVisor is designed to be cross-platform. It can be built to a kernel-mode component of an operating system, or even as a software with bootstrap running on bare-metal. \
Currently, NoirVisor supports 64-bit Windows Operating System newer than or same as Windows 7, running as a kernel-mode driver. \
Porting to Unified Extensible Firmware Interface (UEFI) is in progress. \
If there is already a hypervisor running in the system, make sure it supports native virtualization nesting.

# Development Status
Project NoirVisor has six future development plans:

- Develop Customizable VM engine for complex purposes.
- Develop Nested Virtualization.
- Develop IOMMU Core on AMD-Vi.
- Port NoirVisor to the Rust Programming Language.
- Port NoirVisor to Linux.
- Port NoirVisor to UEFI and corresponding layered hypervisor.

# Publications
Here lists some informal publications (blogs) regarding hypervisor development:

- Extending the Tradition Hypervisor's Approach of System Call Hooking in the Post-2018 Windows Operating Systems: https://tangptr.com/?p=149
- MTRR Emulation: Beginner’s Common Mistake in EPT Setup: https://tangptr.com/?p=163
- Introduction to NoirVisor CVM: The Open-Source Alternative of the Windows Hypervisor Platform: https://tangptr.com/?p=173

# Completed Features

- Bluepill-like Hypervisor for both Intel VT-x /w EPT and AMD-V /w NPT.
- Minimal Microsoft `Hv#1` Hypervisor Functionalities.
- Critical Hypervisor Protection.
- Hardware-Level Code Integrity Enforcement, both Intel EPT and AMD NPT.

# License
This repository is under MIT license.

# Code of Conduct
The Code of Conduct is added to NoirVisor Project since May.5th, 2019. Please follow the rules when contributing.