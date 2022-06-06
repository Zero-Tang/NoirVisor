# I/O Virtualization
This feature is intended for virtualizing specific peripheral hardware.

## PIO Virtualization
NoirVisor may access some external hardware via Port I/O. For example, NoirVisor may use serial port in order to connect to external debugger. The serial port usually takes 8 I/O ports to work. For instance, `COM2` may use ports `0x2F8-0x2FF`. If NoirVisor uses `COM2` to debug, care must be taken to prevent other software from accessing ports `0x2F8-0x2FF`. \
NoirVisor may intercept accesses to I/O ports by setting the IOPM in VMCB or I/O Bitmap in VMCS so that other software cannot tamper with the hardware which NoirVisor exclusively owns.

## MMIO Virtualization
Currently, there are no peripheral hardware which would be exclusively owned by NoirVisor over MMIO. \
Such hardware may include IOMMU. Future implementation of NoirVisor would use IOMMU. However, the operating system may interfere in that IOMMU is often used with x2APIC. \
In order to virtualize MMIO, NoirVisor may intercept MMIO by setting the Nested Paging or Extended Paging.

## I/O Virtualization for Customizable VM
This feature is intended for CVM Guest to access physical peripheral hardware with either exclusive or shared accesses. The documentation is not available yet.