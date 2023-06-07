# Parhelion
This is a formalized version to NoirVisor Secure Virtualization.

## Introduction
In the original implementation of NoirVisor NSV, I introduced the `#VC` exception for handling NAEs. This is a borrowed idea from AMD-V SEV-ES. However, due to the same terminology, this is actually a kind of naming conflict and could cause confusions. \
In this regard, I present Parhelion, a new mechanism to renovate the design of handling NAEs.

Parhelion Mode is an operating mode designed for handling Non-Automatic Exits like I/O operations. Normally, these activities are transparent to conventional operating systems and applications. Parhelion Mode is used by platform firmware and specialized low-level device drivers, rather than the operating systems.

The Parhelion exception-handling mechanism differs substantially from the standard interrupt-handling mechanism of x86-based system software. Parhelion Mode is entered using a special exception called Parhelion Exceptions, issued by NoirVisor due to Non-Automatic Exits (NAEs). As NoirVisor is issuing a Parhelion Exception to the virtual processor (vCPU), the state of vCPU is saved into Parhelion RAM. Interrupts and Exceptions that ordinarily causes control transfers to the operating system are disabled when Parhelion Mode is entered. the vCPU exits Parhelion Mode, restores the saved vCPU state, and resumes normal execution by using special hypercall instruction (either `vmcall` or `vmmcall` instruction).

## Parhelion Resources
The Parhelion resources supported by the NoirVisor consist of Parhelion RAM, and Parhelion MSRs. The Parhelion RAM includes the state-save area and Parhelion Exception handler.

### Parhelion RAM
Parhelion RAM is the memory space accessed by the vCPU when in Parhelion Mode. The default size of Parhelion RAM is 32KiB and can range between 16KiB to 4GiB. Parhelion RAM shares the same address space with the guest software. However, regular guest software are forbidden from accessing Parhelion RAM for the sake of failsafe considerations.

### Parhelion MSR
Parhelion MSRs are used to define Parhelion operations.

| MSR Index | Access | Description |
|---|---|---|
| 0x4001_0130 | WO | Activate the Parhelion virtual machine. |
| 0x4001_0131 | RO | Active status of Parhelion. |
| 0x4001_0132 | R/W | Parhelion RAM Base. |
| 0x4001_0133 | R/W | Parhelion Hypercall Toggle. |

### Parhelion Hypercalls
There are specific hypercalls that only NoirVisor will be processing. These hypercalls are called Parhelion Hypercalls. \
NoirVisor defined an MSR called `Parhelion Hypercall Toggle` to differentiate whether the hypercall should be processed by NoirVisor or the User Hypervisor.

### Atomicity
In Parhelion Mode, external interrupts, NMIs and SMIs are blocked until Parhelion software leaves Parhelion Mode.

## Parhelion Exception
Parhelion Exception is an extended version of the VMM-Communication (`#VC`) exception. \
Its behavior would be similar to System-Management Interrupt (SMI).

Parhelion Mode is entered using the Parhelion Exception. Parhelion Exception is a non-maskable exception that operates differently from and independently of other interrupts and exceptions.

### Control-flow Redirection
On Parhelion Exception, NoirVisor redirects the control-flow as follows:

- NoirVisor saves the vCPU state into the Parhelion RAM. Exact location of the state-save area is defined by a Parhelion MSR.
- NoirVisor loads the vCPU state from the Parhelion RAM. Exact location of the state-load area is defined by a Parhelion MSR.
- NoirVisor puts recorded information of Non-Automatic Exit (NAE) into the Parhelion RAM.
- NoirVisor switches the address space of the vCPU so that the guest will be running under Parhelion Mode.
- NoirVisor issues the VM-Entry so that the control-flow redirection is completed.

### Return from Parhelion
On return from Parhelion, NoirVisor redirects the control-flow as follows:

- NoirVisor loads the vCPU state from Parhelion RAM that saved the vCPU state before Parhelion Exception.
- NoirVisor switches the address space of the vCPU so that the guest will be running under regular mode.
- NoirVisor issues the VM-Entry so that the return-from-Parhelion is completed.

### Converting to Automatic Exits (AEs)
During Parhelion Mode, the guest is very likely to convert NAEs into AEs. \
As the guest software completes conversion, it will issue a hypercall instruction (formerly known as Explicit Hypercall) to transfer the control-flow to the host. On this hypercall, NoirVisor will switch to the host context in order to let the host process the VM-Exit. \
As the user hypervisor completes processing Explicit Hypercall, it will return to the guest. On return, the guest is still under the Parhelion Mode. It is the Parhelion Mode guest's duty to reflect the results of Non-Automatic Exits.

### Nested Exceptions
NoirVisor discourages nested Parhelion Exceptions. If Parhelion software has internal I/O operations (e.g.: debug-printer), they should be implemented by using explicit hypercalls. \
The drafted version is to treat nested Parhelion Exceptions as shutdown conditions.

## Security Considerations
Parhelion Mode should be considered as something similar to System Management Mode (SMM). It will be very dangerous if anything malicious made its way into Parhelion Mode of the Guest.

### Booting the Guest
To enter Parhelion Mode, the Guest's image must be verified. The procedure of verifying boot image is undecided yet. The reference could be taken from the `skinit` instruction procedure.