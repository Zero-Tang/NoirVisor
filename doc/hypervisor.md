# Overview of Project NoirVisor
NoirVisor starts as a common global hypervision project like [HyperPlatform](https://github.com/tandasat/HyperPlatform), [hvpp](https://github.com/wbenny/hvpp), etc. that leverages hardware-accelerated virtualization technology to do security researches. \
This document should provide you a general idea about hypervisor development. You might be able to join development of Project NoirVisor.

## General Idea of Hypervisor
Processor's virtualization extension provides an efficient way to load and save the guest state into the processor and allows a hypervisor to set up certain interceptions in order to efficiently and correctly instrument the behavior of the virtual machine. \
To be more specific, hypervisor may issue a VM-Entry to efficiently load the state of guest to start Guest's operation. When the interception condition is met, processor would issue a VM-Exit to store the state of guest and reload the state of host.

## Global Hypervision
Global Hypervision means to leverage the hardware-accelerated virtualization technology to monitor all activities on a computer. \
To summarize, Global Hypervision would capture the state of the physical machine, then issue VM-Entry based on the captured state. Then, the hypervisor gains monitoring on the whole system.

## Stealthy Hooks
In first few years of development, NoirVisor achieved some stealthy hooks into the kernel. However, these are rootkit-like techniques. They look interesting at first, but soon I lost interest in further development in that Microsoft's detections of stealthy hooks make me sick about such developments. \
Stealthy hooks are implemented by intercepting the paths of detection. For example, the stealthy hook on `syscall` is implemented by intercepting the `rdmsr` instruction on `LSTAR` MSR, so that detectors would receive the original address.

## Customizable Virtual Machines
In 2021, I started working on a new feature called Customizable Virtual Machines, abbreviated as NoirVisor CVM. This is a feature similar to the [Windows Hypervisor Platform](https://docs.microsoft.com/en-us/virtualization/api/hypervisor-platform/hypervisor-platform), but aims to provide more flexibility than WHP. \
CVM feature is fairly functional in AMD-V. It is not working on Intel VT-x yet, which requires further debugging. \
Some features are missing (e.g.: virtualization of APIC, SMM, etc.), though.

## Nested Virtualization
The term nested virtualization has ambiguity in terms of NoirVisor: to run a hypervisor under the global hypervision or under CVM Guests. \
Nested Virtualization for Global Hypervision on AMD-V is now in debugging stage. \
Nested Virtualization for CVM Guests is not available yet.

## Type-I Hypervisor
If NoirVisor is loaded as a Type-I hypervisor, this means the Operating System is booted after the NoirVisor. As a Type-I hypervisor, NoirVisor can monitor the booting activity of the Operating System.

## Type-II Hypervisor
If NoirVisor is loaded as a Type-II hypervisor, this means the NoirVisor is a loaded as kernel-mode module in the Operating System. As a Type-II hypervisor, NoirVisor can only monitor regular runtime activities in the Operating System.