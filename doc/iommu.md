# I/O Memory Management Unit
The IOMMU (I/O Memory Management Unit) is the modern hardware-accelerated virtualization technology used to virtualize peripheral hardware. NoirVisor will utilize IOMMU for various purposes.

## Enabling IOMMU
You will need to activate IOMMU in motherboard firmware or VM settings.

VMware only supports virtualizing Intel VT-d and doesn't support virtualizing AMD-Vi. If your host has Intel CPU, you may enable IOMMU for VMware VMs by checking the "Virtualize IOMMU" box in CPU virtualization.

For QEMU+KVM, you need to add an IOMMU device. For example, you can enable Intel VT-d in your guest:
```
qemu-system-x86_64 -accel kvm -machine q35,kernel-irqchip=split -cpu host,hypervisor=off,vmx=on -device intel-iommu,intremap=on,device-iotlb=on,pt=on,aw-bits=48
```

It seems NoirVisor can't use IOMMU in real machine as a Type-II Hypervisor. It will cause the system to immediately freeze.

### Windows
In order to make NoirVisor use IOMMU, set up the following registry. NoirVisor will not use IOMMU by default.
```bat
reg add "HKLM\SOFTWARE\Zero-Tang\NoirVisor" /v "EnableIommu" /t REG_DWORD /d 1 /f
```

## General Architecture
The design of IOMMU is generally straightforward if you are already familiar with Intel EPT or AMD NPT.

### DMA Remapping
DMA Remapping is the cornerstone to protect memory from DMA attacks. This chapter briefly introduces the key idea of DMA Remapping. \
The address of DMA can be considered 80-bit pointer. 64 bits of them are the physical address of memory, the other 16 bits are the "BDF" (Bus-Device-Function) address of the PCI device references the physical memory. To remap DMA, IOMMU implements an additional layer called "Device Table" (AMD's term) or "Context Table" (Intel's term). The IOMMU will walk such tables before walking the page tables in order to dispatch the DMA transaction to the appropriate page table for a specific device.

In this document, "device table" and "context table" may be used interchangeably. You may simply understand them as a table that contains root addresses of page tables that translate DMA address for PCI devices at any BDF addresses.

In Intel VT-d, there is a `Root-Table Pointer Register`. You will feed the physical address of a "Root-Table", a 4KiB page contains 256 entries of context tables, to this MMIO register. Each context table is a 4KiB page that contains 256 entries of page tables. The page tables don't seem to need explainations, as they are simply used for translating addresses.

In AMD-Vi, there is a `Device-Table Base-Address Register`. You will feed the physical address of a device table, a variable-sized table that contains at most 65536 device table entries (DTEs), to this MMIO register. Each DTE contains information about DMA remapping, interrupt routing, etc. We will currently focus on DMA remapping.

## Implementation Consideration
There are certain additional considerations in order to implement IOMMU in NoirVisor.

### Activating IOMMU
To activate IOMMU, a hypervisor should setup the one set of page table and a device table. All root pointers of page table in the device table should point to the same set of page table so that IOMMU can emulate it. \
After setting up the IOMMU, the hypervisor should use MMIO to write the root pointer of device table into IOMMU, then use MMIO to activate IOMMU's DMA Remapping.

### Choose Paging Mode
Both Intel VT-d and AMD-Vi support various paging modes to support a specific GPA space. NoirVisor's idea is to use a minimum GPA space to cover all physical RAM. To do so, NoirVisor will query all available physical RAMs in the system. Depending on the value of ToM (Top-of-Memory), NoirVisor will choose the minimum level of paging. For example, if the maximum ToM is less than 512 GiB, NoirVisor will choose 39-bit paging, instead of 48-bit. For hollow regions (i.e.: addresses with no physical RAM), NoirVisor will simply not map that region.

### Enumerate Memory Ranges
Despite the notion that NoirVisor will not map hollow regions, the true goal is to reduce memory consumption while building the IOMMU page tables. In other words, if it may reduce memory consumption, hollow regions can be set to present in IOMMU page tables. For example, suppose the IOMMU support 2MiB large pages, and for two memory regions - where one's tail and the other's head are in the same 2MiB page - NoirVisor will simply set the 2MiB page to be present. Because otherwise NoirVisor will have to setup PTE - consuming an extra page - in order to reset the page tables for hollow regions between the two memory regions.

To achieve this, NoirVisor, while setting page table entries during memory-range enumeration, will increment according to the largest supported page size.

NoirVisor does not require large-page support, because VMware's virtualized Intel VT-d does not support them.

Unlike EPT/NPT, IOMMU does not have to cover MMIO ranges in order to let MMIO operations pass-thru. For this specific reason, NoirVisor will build IOMMU page table only to cover physical RAM.

### Nested Virtualization
For Type-II hypervisor, nested virtualization is required in order to make IOMMU work properly, as most modern OSes are IOMMU-aware and they will utilize IOMMU. (e.g.: route interrupts for x2APIC, since I/O APIC only supports xAPIC)

#### Intercept MMIO
IOMMU requires MMIO in order to control the operations. As a result, NoirVisor will have to intercept the IOMMU's MMIO range via Intel EPT or AMD NPT. \
NoirVisor has an internal AVL-tree that manages I/O regions. Intercepted MMIOs will be dispatched according to the tree-walk. Therefore, IOMMU driver can register IOMMU's MMIO regions in order to filter accesses to IOMMU.