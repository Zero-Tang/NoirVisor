# NoirVisor VT-Core
This directory is the virtualization engine based on Intel VT-x. <br>
All code in this directory should be cross-platform designed.

# Files
vt_main.c is the code file that initializes, sets up, and finalizes the virtualization engine based on Intel VT-x. <br>
vt_exit.c is the code file that handles all the VM-Exits derived from the processor. <br>
vt_ept.c is the code file that initializes, sets up, and finalizes the page table based on Intel EPT. Note that it does not handle the EPT Violation. EPT Violation handler is in vt_exit.c <br>
vt_def.h defines basic structures for Intel VT-x, including the MSR-based supporting facility. <br>
vt_ept.h defines basic structures for Intel EPT, plus the EPT manager. <br>
vt_exit.h defines basic constants, structures on VM-Exit. <br>
vt_vmcs.h defines VMCS fields encoding. <br>

# Features
NoirVisor uses the following Intel VT-x features: <br>
MSR-Bitmap. This feature enables the stealth MSR-hook. (For syscall and sysenter hook) <br>
Virtual Processor Identifier. This is also known as VPID. This feature would tag the TLBs with a VPID so that - on VM-Entry and VM-Exit - processor would not invalidate TLB. <br>
Extended Page Table. This is also known as EPT. This feature enables the stealth Inline Hook.

# Stealth MSR-hook Algorithm
This feature utilizes the processor ability to intercept rdmsr instruction. <br>
Set the bit in bitmap to intercept the LSTAR or SYSENTER_EIP MSR-read. <br>
On interception, edit the eax and edx register to represent the original value. <br>
Don't forget to set the bit in primary processor-based execution control in VMCS.

# Stealth Inline Hook Algorithm
This feature utilizes the Intel EPT feature. <br>
To maximize performance - to reduce unnecessary VM-Exit - we use 4KB-paging for hooked page. <br>
There are two pages for one hooked page: the original page and hooked page. <br>
These two pages should have following attributes: <br>
The original page has R+/W+/X- access rights. This means, on execution to the page, EPT violation should occur. <br>
The hooked page has R-/W-/X+ access rights. This means, on read/write to the page, EPT violation should occur. <br>
In this regard, NoirVisor requires that Intel EPT should supports execution-only paging. Otherwise, NoirVisor cannot hide Inline Hooks. <br>
Hooked page includes the hook code, whereas the original page includes the original code. <br>
On VM-Exit, NoirVisor should locate the hook page by GPA (Guest Physical Address) then swap the PTE entry.

# CPUID Cache
This feature could enhance the performance of CPUID-induced VM-Exit on Nested-VMM scenario. (e.g NoirVisor running in a Virtual Machine.) It might slightly lower down the performance on Non-Nested scenario, but generally should have a good performance no matter the scenario. <br>
However, due to complexity of sub-leaf for cpuid instruction, caching architecture in NoirVisor could only accelerate scenarios under ecx=0.

# Critical Hypervisor Protection
This feature is an essential security feature. I found this feature missing in most open-source light-weight hypervisor project. The key is that VMCS and other essential pages are not protected through Intel EPT even if they enabled Intel EPT. It should be pointed out that a malware can be aware of the format of VMCS of a specific processor. In this regard, malware may corrupt the VMCS through memory access instruction.

# Future Feature (Roadmap)
In future, NoirVisor would implement support to VMX-nesting in VT-Core. VMCS-shadowing would be implemented as well.