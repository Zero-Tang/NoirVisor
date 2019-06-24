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
In future, NoirVisor has following plans:

- Implement VMX-Nesting.
- Implement EPT-based Code Integrity Enforcement.

# VMX-Nesting Algorithm (Incomplete Version)
To nest another working hypervisor is the highest focus of Project NoirVisor. However, starting from the repository creation, this goal has not been satisfied yet. Here, I will state down the algorithm and it will be updated in future as problems arises. <br>
We will divide the problem into several sub-problems, as stated below: <br> 

- Enable/Disable Nested VMX
- Virtualize capability MSRs
- Virtualize VM-Entry
- Virtualize VM-Exit
- Virtualize VPID
- Virtualize EPT
- Utilize VMCS-Shadowing

## Glossary
L0: This term refers to the Host context. <br>
L1: This term refers to the Guest Context. <br>
L2: This term refers to the Nested Guest Context. <br>

## Enable/Disable Nested VMX
For this sub-problem, we intercept the VMXON/VMXOFF instructions and write to CR4.VMXE field. <br>
On VM-Exit by VMXON, check CR4.VMXE. If the bit is set, setup the nested vcpu. Otherwise, setup the rflags for error. Then issue a VM-Entry. If nested vcpu was already setup, issue an error through rflags. <br>
At the same time, save the address given by VMXON instruction. We may utilize the page in future. <br>
On VM-Exit by VMXOFF, mark the nested vcpu as disabled. Then issue a VM-Entry. <br>
On VM-Exit by setting CR4.VMXE, set nested vcpu as ready-for-vmxon. Then issue a VM-Entry. <br>
On VM-Exit by resetting CR4.VMXE, if nested vcpu was not disabled, issue an exception. <br>

## Virtualize capability MSRs
For this sub-problem, we intercept the RDMSR instruction from 0x480 to 0x491. <br>
This sub-problem addresses the reporting VMX capability. We should only report features that we can successfully emulate, instead of forwarding the original values.

## Virtualize VM-Entry
For this sub-problem, we intercept vmlaunch and vmresume instructions. For algorithm considerations, we assume we know about VMCS given by nested VMM.

### Set up L2 VMCS - Guest State Fields.
For Guest State Area, things are simple: copy everything to L2 VMCS.

### Set up L2 VMCS - Host State Fields.
For Host State Area, we should not copy from Guest VMM's VMCS. Instead, we setup our own context, since context goes to L0 as VM-Exit occurs.

### Set up L2 VMCS - VM-Execution Control Fields.
For VM-Execution Control Fields, we first copy from Guest VMM's VMCS. Then add additional controls that we need. Compare with the fields that we support, fail the VM-Entry if unsupported fields was set. <br>
Details regarding the "additional controls" will be stated in future.

### Issue VM-Entry.
L2 VMCS are completed. Issue a VM-Entry to enter L2 Context - that is, Guest VMM issued a VM-Entry.

## Virtualize VM-Exit
For this sub-problem, interceptions were already given by L2 VMCS VM-Execution Control Fields. Steps are given below.

### Set up L1 VMCS.
For L1 VMCS, we simply focus on one thing: Guest State Area. Set the guest context to be host context given by Guest VMM's VMCS.

### Set up L2 VMCS.
For L2 VMCS, we update the Read-Only Area, Guest State Area so that Guest VMM knows what happened.

### Issue VM-Entry.
Things are done. Issue a VM-Entry to enter L1 Context - that is, Guest VMM received a VM-Exit.

## Virtualize VPID
The Virtual Processor Identifier (VPID) is a mechanism that accelerate the Translation Lookaside Buffer (TLB) in Intel VT-x. If VMM disabled VPID, then the processor has to flush all TLBs on any VM-Entries and VM-Exits. Otherwise, the processor marks the TLBs with VPID based on the context. On VM-Entries and VM-Exits, the processor switches VPID. VPID other than the current context will not be used.

### Virtualize VPID Field
Since NoirVisor's VPID usage is simple, we define the following:

- At L0 context, VPID is zero.
- At L1 context, VPID is one.

For L2 context, things may vary due to the following:

- Guest may disable VPID.
- Guest may set VPID to random values.

For second consideration, we increment by 1 to the VPID so that the TLB will be marked with VPID higher than 0 and 1. If Guest VMM gives a VPID that equals zero, fail the VM-Entry.

### No-VPID Scenario
In case that guest disabled VPID, we should also enable VPID in L2 VMCS. Set L2's VPID to one as well as L1's. On VM-Entries and VM-Exits induced by L2, flush all TLBs that VPID equals one by the invvpid instruction.

### Virtualize VPID Invalidation
Exactly, what we should do is to redirect the VPID (increment by 1).

## Virtualize EPT
To virtualize EPT, we should merge the page tables. However, I don't have an algorithm regarding page-table merging. So, the VMX-nesting feature in future NoirVisor may not support EPT unless I have one.

## Utilize VMCS-Shadowing
This feature could be a hard-point for me because by lab does not own a processor that supports this feature. The newest Intel CPU I have is the Intel Core i7-7500U, where the VMCS-Shadowing feature is unsupported. <br>
In addition, VMware WorkStation (by now, version 15.1.0) does not emulate VMCS shadowing, even if the host machine supports it. (Test on Intel i5-6400 CPU) <br>
With VMCS-Shadowing, we can reduce the VM-Exits induced by vmread and vmwrite instructions.