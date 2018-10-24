# NoirVisor

NoirVisor - Hardware-Accelerated Hypervisor solution with support to complex functions and purposes and nested virtualization.

# Introduction
NoirVisor is a hardware-accelerated hypervisor (a.k.a VMM, Virtual Machine Monitor) with support to complex functions and purposes. It is designed to support processors based on x86 architecture with hardware-accelerated virtualization feature. For example, Intel processors supporting Intel VT-x or AMD processors supporting AMD-V meet the requirement. By designation, NoirVisor determines the processor manufacturer and selects the function core.

# Processor Requirement
Intel Processors based on Intel 64 and IA-32 Architecture, with support to Intel VT-x. Intel EPT is prefered, but not required.
AMD Processors based on AMD64 Architecture, with support to AMD-V. Nested Paging is prefered, but not required.
Other processors based on x86 architecture may be supported in future.

# Nested Virtualization
NoirVisor is developed in highest focus on nested virtualization. It is not currently supported, but will be developed in future.

# Announcement to all contributors
NoirVisor is coded in the C programming language and the assembly since it is procedure-oriented designed.
DO NOT PROVIDE CODES WITH C++ WHICH INVOLVES THE NoirVisor Core IN YOUR PULL-REQUEST!

# Build
To build a kernel-mode driver on Windows, you should install Windows Driver Kit 7.1.0 to default path on C disk. Then run the provided batch file to build it.

# Supported Platforms
NoirVisor is designed to be cross-platform. It can be built to a kernel-mode component of an operating system, or even as a software with bootstrap running on bare-metal.
Currently, NoirVisor supports the Windows Operating System higher than Windows Server 2000, running as a kernel-mode driver.
If there is already a hypervisor running in the system, make sure it supports native virtualization nesting.

# Development Status
Project NoirVisor has just launched.

# License
All the codes with NoirVisor are licensed under MIT License.