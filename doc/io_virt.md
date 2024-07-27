# I/O Virtualization
This feature is intended for virtualizing specific peripheral hardware.

## PIO Virtualization
NoirVisor may access some external hardware via Port I/O. For example, NoirVisor may use serial port in order to connect to external debugger. The serial port usually takes 8 I/O ports to work. For instance, `COM2` may use ports `0x2F8-0x2FF`. If NoirVisor uses `COM2` to debug, care must be taken to prevent other software from accessing ports `0x2F8-0x2FF`. \
NoirVisor may intercept accesses to I/O ports by setting the IOPM in VMCB or I/O Bitmap in VMCS so that other software cannot tamper with the hardware which NoirVisor exclusively owns.

Drivers inside NoirVisor should register I/O port interceptions and corresponding callbacks.

## MMIO Virtualization
NoirVisor may access some external hardware via Port I/O. For example, NoirVisor may use IOMMU in order to virtualize peripheral PCI devices. \
In order to virtualize MMIO, NoirVisor may intercept MMIO by setting the Nested Paging or Extended Paging, so that other software cannot tamper with the hardware.

Drivers inside NoirVisor should register MMIO interceptions and corresponding callbacks.

## I/O Filtering
NoirVisor implements an I/O filtering mechanism for subverted host. Drivers inside NoirVisor may register their I/O regions so that they can receive callbacks. \
The I/O regions are organized in AVL-tree so that the I/O interceptions can be dispatched in `O(log_2_n)` time complexity.

## I/O Virtualization for Customizable VM
This feature is intended for CVM Guest to access physical peripheral hardware with either exclusive or shared accesses. The documentation is not available yet.