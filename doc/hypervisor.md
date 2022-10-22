# Overview of Project NoirVisor
NoirVisor starts as a common global hypervision project like [HyperPlatform](https://github.com/tandasat/HyperPlatform), [hvpp](https://github.com/wbenny/hvpp), etc. that leverages hardware-accelerated virtualization technology to do security researches. \
This document should provide you a general idea about hypervisor development. You might be able to join development of Project NoirVisor.

## General Idea of Hypervisor
Processor's virtualization extension provides an efficient way to load and save the guest state into the processor and allows a hypervisor to set up certain interceptions in order to efficiently and correctly instrument the behavior of the virtual machine. \
To be more specific, hypervisor may issue a VM-Entry to efficiently load the state of guest to start Guest's operation. When the interception condition is met, processor would issue a VM-Exit to store the state of guest and reload the state of host. \
Such VM-Entry is also called subversion.

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

## Host Environment Setup
Many implementations of Type-II global hypervision share the environment with host. This is a critical flaw when a malicious guest program attempts to tamper with the host environment. \
For Type-I hypervisors, this is a must-do or the hypervision won't last to OS runtime otherwise. Because Type-II hypervisors can share the host environment with guest kernel, this is usually not a problem if there is no malicious kernel program which attempts to tamper with the host environment.

### Allocation Stage
The allocation stage should start before the broadcast begins. Processor state which should be allocated including:

- Paging Structure (CR3, Page Table) is shared among all logical processors.
- Interrupt Descriptor Table (Custom Interrupt Handlers) is exclusive to each processor.
- Global Descriptor Table (Copy from current environment) is exclusive to each processor.
- Task Segment State (Copy from current environment) is exclusive to each processor.
- GS Base (Create by NoirVisor) is exclusive to each processor.

For shared states, initializations are done in the allocation stage.

### Initialization Stage
The initialization stage should start after capturing the processor state, but before subverting the processor. Processor state which should be initialized including:

- Interrupt Descriptor Table should be initialized by copying the IDT from the captured IDT and by replacing certain exception handlers with NoirVisor's handler.
- Global Descriptor Table should be initialized by copying the GDT from the captured GDT. No revision is required.
- Task Segment State should be initialized by copying the TSS from the captured TSS. No revision is required.
- GS Base should point to the host vCPU structure.

### Setup Stage
The setup stage should start right before the subversion begins.

- Use `lidt` and `lgdt` instructions to setup IDT, GDT and TSS.
- Use `wrmsr` instruction to setup GS Base.
- Use `mov cr3` instruction to setup Paging Structure.
- Use `mov cr4` instruction to enable `OSFXSR` and `OSXSAVE`.