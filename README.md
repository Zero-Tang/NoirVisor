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
NoirVisor is a Type-1 bluepill hypervisor that aims to provide custom virtualization and secure computing services.

**IMPORTANT NOTES:** If you're looking for a stealthy hypervisor implementation, you're at the wrong place.

# Processor Requirement
Your CPU must support either Intel VT-x /w EPT or AMD-V /w NPT. However, this does not mean only Intel and AMD CPUs are supported.

- Processors produced by Intel, VIA, Zhaoxin (兆芯) and Montage (澜起) may support Intel VT-x. \
- Processors produced by AMD and Hygon (海光) may support AMD-V.

# Build
See [documentation](./doc/make.md) for more information about preparation and using python script to build NoirVisor.

# Run
To run NoirVisor, you will have to boot NoirVisor's hypervisor before an OS loads

## Windows Driver
If you the NoirVisor service is not installed:
```bat
sc create NoirVisor type= kernel binPath= <Path to NoirVisor driver file>
```
Once NoirVisor service is installed, start the driver:
```bat
sc start NoirVisor
```

You may unload NoirVisor by using command-line:
```bat
sc stop NoirVisor
```
If you need to uninstall NoirVisor service:
```bat
sc delete NoirVisor
```

## EFI Application and Runtime Driver
There are several methods to run NoirVisor.

### Running on a physical machine
This method can also be used on VMware. \
Use a USB flash stick and setup with GUID Partition Table (GPT). Construct a partition and format it info FAT32 file system. After you successfully build the image, you should see two images: `bootx64.efi` and `NoirVisor.efi` \
Those two files are EFI Application and Runtime Driver respectively. \
Copy EFI Application to `\EFI\BOOT\bootx64.efi` \
Copy EFI Runtime Driver to `\NoirVisor.efi` \
As the USB flash stick is ready, enter your firmware settings and set it prior to the operating system. Disable Secure Boot feature unless you can sign the executable.

### Running on a virtual machine
After you built NoirVisor, several virtual disk image options are available: `raw`, `vhdx` and `vmdk`. Add the image to your virtual machine as a hard drive. \
Also see [Quick Start on Emulators](/doc/debug.md#quick-start-on-emulators).

# Documents
This repository provides [additional documents](/doc/readme.md) which help new developers to join development.

# Customizable VM
Customizable VM is a feature that provides a set of APIs to run an arbitrary guest, instead of to just subvert the host system. In a word, it is aimed to be a competitor of the Windows Hypervisor Platform (WHP).

# Security Advisories
You should not report security vulnerabilities through the GitHub issue. You should [read this document](./security.md) to check out the steps to report security vulnerability.

# Publications
Here lists some informal publications (blogs) regarding hypervisor development:

- Extending the Tradition Hypervisor's Approach of System Call Hooking in the Post-2018 Windows Operating Systems: https://tangptr.com/?p=149
- MTRR Emulation: Beginner’s Common Mistake in EPT Setup: https://tangptr.com/?p=163
- Introduction to NoirVisor CVM: The Open-Source Alternative of the Windows Hypervisor Platform: https://tangptr.com/?p=173

# License
This repository is dual licensed with [MIT](./LICENSE-MIT) and [Apache-2.0](./LICENSE-APACHE), just like [rust](https://github.com/rust-lang/rust/blob/main/COPYRIGHT).
