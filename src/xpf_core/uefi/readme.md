# NoirVisor XPF-Core (UEFI Chapter)
Hypervisors on UEFI are different from traditional hypervisors on operating systems. In terms of porting NoirVisor to UEFI, it would be a Type-I hypervisor that will own a completely different address space than the Type-II hypervisors.

## Logic of System Subversion
To subvert the system in terms of Type-I hypervisors, the algorithm should be like:

1. Initialize the host state of current processor, including IDT, GDT, paging structures, to runtime memory pool.
2. Initialize TSS with runtime memory pool. Intel VT-x requires host environment to have present TSS. There is no such requirement in AMD-V.
3. Load host state. For Intel VT-x, copy the state to VMCS. For AMD-V, load them directly.
4. Load guest state. Copy the state to VMCS/VMCB.
5. Launch guest. Subversion is half-way done.
6. Intercept INIT-signal. Emulate the signal and halt the processor execution to wait for Startup-IPI.
7. Intercept SIPI. Emulate the interrupt and resume processor execution.

## Debug Port
The default method of printing debug messages should be outputting via Debug-Port Protocol. Most platforms do not have this protocol set up. Therefore, NoirVisor has to set up its own connection for debug output. Current implementation will be over serial connection. This method should work on any virtual machines. Modern embedded systems should have serial connections on board. Modern desktop systems can install PCI card to extend serial port capability. Future implementations of NoirVisor will add support of using USB to output debug messages.

### Debug Port in Runtime Stage
Because most protocols are gone when UEFI enters the Runtime stage, NoirVisor must choose an alternative method to output debug messages.

#### Serial I/O
Typically, the communication of serial I/O would be done via doing I/O instructions. However, the UEFI does not provide a generic way to obtain the exact port number. Nevertheless, with the power of hypervisor, this is feasible. We may set unconditional I/O interceptions to observe which I/O port was intercepted when invoking the EFI Serial I/O Protocol. Specifically, it should be observed which port was given the data to output.