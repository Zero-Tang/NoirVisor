# ACPI Driver
NoirVisor may access ACPI (Advanced Configuration and Power Interface) to operate external devices like IOMMU in order to help virtualization.

## DMAR Table
The DMA-Remapping (a.k.a. `DMAR`) Table in ACPI defines the Intel VT-d operations.

## IVRS Table
The I/O Virtualization Reporting Structure (a.k.a. `IVRS`) Table in ACPI defines the AMD-Vi operations.

## AML Parser
NoirVisor may take exclusive access of some devices (e.g.: serial ports) defined in ACPI namespace. In order to do so, NoirVisor must make patches to AML (ACPI Machine Language).