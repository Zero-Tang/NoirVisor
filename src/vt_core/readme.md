# NoirVisor VT-Core
This directory is the virtualization engine based on Intel VT-x. <br>
All code in this directory should be cross-platform designed.

# Files
vt_main.c is the code file that initializes, sets up, and finalizes the virtualization engine based on Intel VT-x. <br>
vt_exit.c is the code file that handles all the VM-Exits derived from the processor. <br>
vt_ept.c is the code file that initializes, sets up, and finalizes the page table based on Intel EPT. Note that it does not handle the EPT Violation. EPT Violation handler is in vt_exit.c <br>
vt_nvcpu.c is the code file that supports Nested Virtualization for vCPU. <br>
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
Set the bit in bitmap to intercept the `LSTAR` or `SYSENTER_EIP` MSR-read. <br>
On interception, edit the eax and edx register to represent the original value. <br>
Don't forget to set the bit in primary processor-based execution control in VMCS.

## The KVA-Shadow Problem upon MSR-Hook
By virtue of the [meltdown attack](https://meltdownattack.com/) released in early 2018, the [KVA-shadow](https://msrc-blog.microsoft.com/2018/03/23/kva-shadow-mitigating-meltdown-on-windows/) mechanism is implemented as mitigation upon this attack. With this mitigation, program in user mode is loaded with a special page table that only a few critical pages that include transitions between user-mode and kernel-mode are present. Other kernel pages are absent in this page table and are only to be present when transition is completed. The `KiSystemCall64Shadow` routine is present in such page table. However, the proxy procedure that we used to hook the `syscall` handler are definitely absent in that page table. Therefore, if we hook into `syscall` handler with MSR without special handling, `#PF` exception would occur when the processor enters our proxy procedure. What is even worse is that the `KiPageFaultShadow` routine, which handles the `#PF` exception, is absent in that page table. Therefore, another `#PF` occurs during IDT delivery. Combining these two `#PF` exceptions, they result into `#DF` exception. It seems that the `KiDoubleFaultAbortShadow` routine, which handles the `#DF` exception, is present in that page table, or elsewise triple-fault is being triggered. Hence, Bug-Check `UNEXPECTED_KERNEL_MODE_TRAP` with first parameter `0x8` (double-fault) is issued eventually. <br>

### Solution of KVA-Shadow Problem
The most intuitive solution to solve the problem is to resolve the `#PF` exception when `syscall` handler is entered. This would of course require intercepting `#PF` exceptions. Upon interception, read the `Exit Qualification`, which refers to the faulting address during `#PF` exit, field from the VMCS. Compare the faulting address and the address of `syscall` proxy procedure. If they match, switch the guest `CR3` register so that the page table of the guest would include the proxy procedure. In addition to switching `CR3`, flush TLB via `invvpid` instruction if `VPID` feature is enabled. Otherwise, forward the exception into the guest. `CR2` register should be updated during the exception forwarding.

### Optimize Interception
As a rule of thumb, the less the VM-Exits, the better the performance of the hypervisor. Intel VT-x provides two 32-bit control fields to filter exceptions in VMCS: `Page-Fault Error-Code Mask` and `Page-Fault Error-Code Match`. When a page-fault occurs, the `Page-Fault Error-Code` (`pfec` in short) will be `logical-and`ed with `Page-Fault Error-Code Mask` (`pfec_mask` in short) then compare with `Page-Fault Error-Code Match` (`pfec_match` in short). The comparison can be expressed as the following C pseudo-code:
```C
bool intercept=bittest(exception_bitmap,ia32_page_fault);
u32 pfec_cmp=pfec&pfec_mask;
bool match=pfec_cmp==pfec_match;
if(match ^ !intercept)vmexit(exception,ia32_page_fault);
```
For example, if `pfec_mask` and `pfec_match` are both 0 and page-fault interception is set, all page-fault exceptions will be intercepted because `pfec_cmp` will always be 0 so that `pfec_match` would be matched. Because `match` is `true` and `!intercept` is `false`, the `xor` operation will make the statement within the if condition being `true`. The exception will take place. <br>
Therefore, we can set `pfec_match` to be "instruction-fetch only", and set `pfec_mask` to be masking both "user-mode access" and "instruction-fetch" so that only kernel-mode instruction fetch faults will be intercepted.

## The New PatchGuard Mechanism
Recently, there is a new PatchGuard Mechanism which detects LSTAR MSR-Hook. It is called `KiErrata704Present`. This mechanism would write a temporary address to the LSTAR MSR then execute `syscall` instruction to verify the result. This is a hypervisor-specific technique to detect hooks in that writing a temporary address to LSTAR could unhook LSTAR permanently. In order to mitigate this mechanism, we should intercept writes to LSTAR MSR. If the value to be written equal to the original `syscall` handler, set the MSR to be the proxy handler. Otherwise, set the MSR as is. On interception upon reads from LSTAR MSR, return the original `syscall` handler if current LSTAR is set to the proxy handler, and otherwise return the value previously written to LSTAR MSR.

## Supervisor-Mode Access Prevention Problem
SMAP, acronym that stands for Supervisor-Mode Access Prevention, is an (Should it be "an" or "a"? Do you pronounce "x" in "x86" as "eks" or "cross"?) x86 processor feature that prevents supervisor-mode code from accessing user-mode data. It should be noticed that SMAP does not aim to prevent malicious privileged program from stealing or tampering user-mode data. It, instead, prevents benevolent supervisor-mode code from accessing malevolent user-mode data. For example, the infamous [CVE-2018-8897 vulnerability](https://everdox.net/popss.pdf) deceives the OS `#DB` handler that the `fs`/`gs` segment is pointing to kernel-mode data (e.g: `KPCR` structure in Windows) while, in fact, the `fs`/`gs` segment is pointing to user-mode data (e.g: `TEB` structure in Windows). <br>
The problem is that, if this feature is enabled, the proxy `syscall` handler will be forbidden from reading parameters in user-mode memory (e.g: user stack). 

### Solution of the SMAP Problem
The solution of the SMAP problem is actually simple: execute the `stac` instruction to set `RFlags.AC` bit so that supervisor-mode code can access user-mode memory freely. <br>

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

## Hide Read and Write Accesses from the Same Page
If an instruction in the hooked page accesses some data in the same page, it would infinitely trigger EPT violations. Therefore, care must be taken when the `rip` and `GPA` are in the same page. <br>
Intel VT-x provides the `Monitor Trap Flags` feature that triggers VM-Exits per instruction. We may use this feature to circumvent the this possibility. <br>
When EPT violation handler detects that `GPA` and `rip` are in the same page, grant `R+/W+/X+` permission, map to the original page and enable MTF. An MTF VM-Exit will be immediately triggered after this instruction completes. In this MTF VM-Exit, revoke `R+/W+` permissions, map to the hooked page, and disable the MTF.

## The New PatchGuard Mechanism
The `KiErrata671Present` is a new mechanism that detects hooks concealed by EPT. It looks like:

```Assembly
xor eax,eax
inc eax
ret
```

The invoker of `KiErrata671Present` would replace the `inc eax,eax` instruction with `ret` instruction. Then calls this function to see the result. <br>
If the `KiErrata671Present` locates in the same page with our hooked function, then the `KiErrata671Present` function would return one, in that the write was redirected to the original page instead of the hooked page. <br>
This becomes a dilemma in that there is always a spot to detect inconsistency no matter where we redirect the hook.

# Critical Hypervisor Protection
This feature is an essential security feature. I found this feature missing in most open-source light-weight hypervisor project. The key is that VMCS and other essential pages are not protected through Intel EPT even if they enabled Intel EPT. It should be pointed out that a malware can be aware of the format of VMCS of a specific processor. In this regard, malware may corrupt the VMCS through memory access instruction.

# Real-Time Code Integrity
Real-Time CI is now implemented by Intel EPT.

# Host NMI Processing
Unlike the GIF mechanism in AMD-V, Intel VT-x does not prevent the NMI to be triggered in the host context. The negative side-effect of this phenomenon can be manifested in two ways.

## Type-I Hypervisor
If NoirVisor is loaded as a Type-I Hypervisor, the NMI will be taken in a wrong context. The approach of MiniVisor, written by Satoshi Tanda, is to discard the NMIs. However, if some messages are to be delivered through NMI, loss of message may cause incorrect behavior in the system and may trigger kernel panicking (e.g: NMI watchdog).

## Type-II Hypervisor
If NoirVisor is loaded as a Type-II Hypervisor, the NMI will be taken in an out-of-monitor context. If malicious software hijacked the NMI handler of the OS (e.g: registered an NMI callback through `KeRegisterNmiCallback`), then the malicious software can circumvent the monitoring of hypervisor by issuing NMIs over APIC ICR.

## Solution
The straight-forward solution is to set up hypervisor's own NMI handler for the host. The handler will inject NMI to the guest. However, in terms of returning from NMI, there is something worth discussing:

1. If we emulate the `iret` instruction, the NMI-blocking will remain in the host so that no further NMIs will be fired.
2. If we don't emulate the `iret` instruction, there could be multiple NMIs to be held pending in the same session of VM-Exit.

Unlike AMD-V, for Intel VT-x, it is arduous to emulate the `iret` instruction without breaking registers. AMD-V has a GIF mechanism that allows hypervisors to prepare to accept NMIs. Therefore, AMD-V hypervisors may use calling conventions to circumvent the non-volatility of registers in terms of interrupt handling. Even so, option 2 is still unacceptable by virtue of the incircumventable race condition of NMIs.

In order to return from NMI without unblocking NMIs, following steps may be taken:

1. Push a register (e.g.: the `rax` register) onto the stack in order to save it.
2. Load this register with the value of `rsp`.
3. Use `lss` instruction to load `rsp` and `ss` registers. Note that the stack is already switched!
4. Use `popf` instruction to load `rflags` register.
5. Push the `cs` and `rip` registers onto the stack using special memory-addressing operand (e.g.: the `rax` register).
6. Restore the register with `mov` instruction.
7. Execute `retf` instruction to load `cs` and `rip` registers.

# MTRR Emulation
According to Intel 64 Architecture Manual, the MTRRs have **no effect** on the memory type used for an access to a guest physical address. If we map all memory as write-back with EPT, there could be conflicts in that not all memory are defined as write-back by OS. For example, OS could define MMIO region as uncacheable memory. Similarly, graphics buffer could be mapped as write-combined by OS. Failure to map these memory accordingly could cause certain issues. For example, it is observed that some processors could encounter an `#MC` exception due to L2 cache data-read error. <br>
On AMD-V/NPT, there is no need for MTRR Emulation in that Nested Paging automatically combines the memory type from NPT and MTRR.

## Algorithm
There are two types of MTRRs: Fixed MTRRs and Variable MTRRs. The Fixed MTRRs map the first MiB of system memory. The Variable MTRRs map a variable range of system memory. <br>
The general layout of MTRR Emulation algorithm is described as the following:

1. Check the values of MSRs: `IA32_MTRRCAP` (MSR Index=0xFE), `IA32_MTRR_DEF_TYPE` (MSR Index=0x2FF).
2. Set all pages to the default memory type indicated in the `IA32_MTRR_DEF_TYPE` MSR.
3. If MTRR is enabled as indicated by `IA32_MTRR_DEF_TYPE` MSR, then emulation of variable MTRR is required.
4. If SMRR (MTRR for System-Management Mode) is supported as indicated by `IA32_MTRR_CAP` MSR, then emulation of SMRR is required.
5. If Fixed MTRR is enabled as indicated by `IA32_MTRR_DEF_TYPE` MSR, then emulation of variable MTRR is required.

### Variable MTRR Emulation
According to the definition of variable MTRR, there are two registers: the `base` register and `mask` register. <br>
The pseudo-code of Variable MTRR logics would be like:
```C
mask_base = phys_mask.mask & phys_base.base;
mask_target = phys_mask.mask & (target_address >> 12);
if(mask_base == mask_target)set_memory_type(phys_base.type);
```
For simplicity, we may traverse all possible pages, check if the page is within the range, and set the memory type if the page is in range. <br>
Current EPT implementation of NoirVisor only supports 512GiB of Guest Memory. Therefore, we may simply traverse 512GiB. <br>
Set the bit 11 (a bit ignored by processor) in the final paging structure to indicate that this page is described by a variable MTRR. This bit will be used for checking MTRR overlaps. <br>
If overlapping is detected, decision should be made to choose the correct memory type. The logics described by Intel and AMD are quite complex. For hypervisors, we can reduce the complexity of this logic by simply comparing the value of memory types for overlapped region: the smallest one is to be chosen. See Chapter 11.11.4.1, "MTRR Precedences", Volume 3, Intel 64 and IA-32 Architecture Software Developer's Manual for further details regarding the overlapping variable MTRRs.

### Fixed MTRR Emulation
The logic of Fixed MTRR is quite simple: read all MSRs related to the Fixed MTRRs and set them accordingly. Each MSR for Fixed MTRR defines eight memory types for eight ranges. The size of range depends on which MSR. For example, the `IA32_MTRR_FIX64K_00000` MSR defines eight memory types for eight 64KiB ranges (i.e: 16 pages per range). <br>
Please note that the Fixed MTRRs take higher priority than Variable MTRRs when there are overlappings. Therefore, setting memory types for the Fixed MTRRs are to be done after setting for the Variable MTRRs.

## MTRR-Write Interception
The system might change the value of MTRR. In this regard, the old settings of EPT memory types would be invalid. However, it does not mean that intercepting MTRR is the only thing to do. Re-emulate MTRRs on every interception is a dumb idea. 

On interceptions of writes that set `CR0.CD` bit, set the `CR0.CD` bit in both host and guest so that caching is disabled.

On interceptions of writes to MTRRs:

1. Check the `CR0.CD` bit, if caching is not disabled, re-emulate MTRRs.
2. Otherwise, set a hint that, on writes that sets `CR0.CD`, MTRRs should be re-emulated.
3. Pass the value to be written to the real MTRR.

On interceptions of writes that clear `CR0.CD` bit:

1. Re-emulate MTRRs.
2. Clear the `CR0.CD` bit so that caching is enabled.

When re-emulating MTRRs, do not forget to execute `invept` instruction to flush EPT TLBs.

# Future Feature (Roadmap)
In future, NoirVisor has following plans:

- Implement VMX-Nesting. (This will be a long term project)
- Implement Customizable VM for VT-Core.
- Implement NPIEP. (Non-Privileged Instruction Execution Prevention)

# VMX-Nesting Algorithm (Incomplete Version)
To nest another working hypervisor is the highest focus of Project NoirVisor. However, starting from the repository creation, this goal has not been satisfied yet. Here, I will state down the algorithm and it will be updated in future as problems arises. <br>
We will divide the problem into several sub-problems, as stated below:

- Enable/Disable Nested VMX
- Virtualize capability MSRs
- Virtualize VM-Entry
- Virtualize VM-Exit
- Virtualize VPID
- Virtualize EPT
- Utilize VMCS-Shadowing
- Virtualize L2 VMCS
- Devirtualize on-the-fly.

## Glossary
L0: This term refers to the Host context. <br>
L1: This term refers to the Guest Context. <br>
L2: This term refers to the Nested Guest Context.

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
To virtualize EPT, we should merge the page tables. However, I don't have an algorithm regarding page-table merging. So, the VMX-nesting feature in future NoirVisor may not support EPT unless I have one. <br>
An easier, and high-performance, but protection-degraded approach to achieve this goal is to simply pass the EPT Pointer from the guest to the processor. This approach is fine for virtual machine monitors like VMware Workstation, VirtualBox, etc. because the physical memories to be virtualized do not conflict with NoirVisor's EPT Protection. However, this approach is indeed unsuitable to global hypervisors like NoirVisor itself.

## Utilize VMCS-Shadowing
This feature could be a hard-point for me because my lab does not own a processor that supports this feature. The newest Intel CPU I have is the Intel Core i7-7500U, where the VMCS-Shadowing feature is unsupported. <br>
In addition, VMware WorkStation (by now, version 16.1.2) does not emulate VMCS shadowing, even if the host machine supports it. (Tested on Intel i5-6400 CPU, a machine that does not belong to my lab) <br>
With VMCS-Shadowing, we can reduce the VM-Exits induced by vmread and vmwrite instructions.

## L2 Virtual Machine Control Structure
Since I do not have sufficient supplies to develop VMCS-Shadowing support, software-based VMCS virtualization is necessary. <br>
Here, I re-emphasize the point: <br>
On VM-Entry the processor triggered a VM-Exit due to vmlaunch or vmresume instruction execution. In this regard, we could say processor moved from L1 to L0. To virtualize the VM-Entry, we should move processor from L0 to L2. <br>
On VM-Exit the processor moved from L2 to L0. To virtualize the VM-Exit, we should move processor from L0 to L1. <br>
Therefore, nested VM-Entry, in concept is moving the processor from L1 to L2, but in fact is L1 to L0 then to L2; nested VM-Exit, in concept is moving the processor from L2 to L1, but in fact is L2 to L0 then to L1. <br>

### L2 VMCS Programming Consideration
In system, there might be multiple virtual machines running. Thus, there could be multiple VMCS in different states. How to implement the multi-NestedVMCS support? Followings are my algorithm design. <br>
Make two kinds of VMCS for L2: <b> L2C and L2T </b>.

- L2C-VMCS is where vmread and vmwrite instructions result. L2C is allocated by host. It is formatted hypervisor-implementation specific.
- L2T-VMCS is where vmlaunch and vmresume instructions result. L2T is allocated by guest. It is formatted processor-implementation specific.

L2C and L2T should be paired. They behave in following manner:

- On VM-Exit induced by vmread/vmwrite instructions in L1, we get/put data from/to L2C VMCS.
- On VM-Exit induced by vmlaunch/vmresume instructions in L1, we synchronize the L2C Guest State and Control Area to L2T-VMCS.
- On VM-Exit induced by L2 execution, we synchronize the L2T Guest State and Read-Only Area to L2C-VMCS, then synchronize the L2C Host State to L1 Guest State.

In this way, VM-Entry will take heavy burden on synchronizing L2C to L2T.

### L2 VMCS Optimization: <b> State Caching </b>
The L2 VMCS design, as previously detailed, can have a significant performance issue: heavy-weight vmwrite instructions on nested VM-Entries. To optimize, we should cache the L2C-VMCS so that vmwrite instruction executions could be reduced. I was implied this idea through AMD's design on VMCB State Caching mechanism. <br>
This mechanism is designed for synchronizing from L2C-VMCS to L2T/L1. It should behave in the following: <br>
Prior to guest executing vmlaunch instruction, mark all fields as "dirty" fields. This includes VM-Exit induced by vmclear instruction. <br>
On VM-Exit induced by vmwrite instruction, mark a specific field of VMCS as "dirty" field if new data are different from old data. <br>
On VM-Exit induced by nested VM-Entry, traverse all fields in L2C. If one field during traversal is determined to be "dirty", synchronize it from L2C to L2T, then mark it as "clean" field. <br>
On VM-Exit induced by nested VM-Exit, traverse host state fields in L2C. If one field during traversal is determined to be "dirty", then synchronize it from L2C to L1, and mark it as "clean" field.

### L2 VMCS Optimization: <b> Selective Synchronization </b>
State Caching mechanism optimizes the synchronization from L2C-VMCS to L2T/L1, but cannot optimize synchronization from L2T-VMCS to L2C. To optimize such synchronization, we implement the Select Synchronization mechanism. <br>
The key point is to select sufficient data to synchronize as little as possible. We do not have to read all data from L2T VMCS and synchronize them to L2C in every nested VM-Exit. <br>
There are "Selective Conditions" that help reducing the performance consumption induced by synchronization.

#### Selective Condition A: VM-Exit Reason
Different reasons of exits would require different data. Data not required in VM-Exit should not be synchronized. For example, the CPUID-induced VM-Exit won't need to read exit qualification field. Thus synchronization is unnecessary - that is, do not synchronize exit qualification field so that we got optimized.

#### Selective Condition B: VM-Exit Control
In fact, this is required by Intel SDM. Not doing so may result the nested hypervisor to failure. "Save XXX" in VM-Exit Control field gives us hint whether we should synchronize them of not. For example, If the "Save IA32_EFER" bit is reset, then we do not synchronize the "Guest IA32_EFER" field in L2T VMCS.

### L2 VMCS Optimization: <b> VMCS Shadowing </b>
The performance improvement by VMCS Shadowing depends on how to setup the vmread/vmwrite bitmaps. <br>
By now, I do not have a clear idea on how to effectively utilize this processor feature.

## Devirtualize on-the-fly
Since NoirVisor is possible to be unloaded before the nested hypervisor ends, it is necessary to put nested hypervisor directly onto the processor - that is, L1 becomes L0 and L2 becomes L1. <br>
This is a suggestion for Nested Virtualization in NoirVisor. By now, algorithm design will not be made. After the Nested Virtualization feature completes, NoirVisor may refuse unloading as long as the nested hypervisor did not leave. <br>

# Customizable VM Scheduler Algorithm
This chapter describes the CVM scheduler algorithm for VT-Core.

## World Switch - Host to Guest
The only condition to switch the world from Host to Guest is the Host's hypercall to request switching to the Guest's vCPU. <br>
Prior to switching vCPU, the scheduler should save the Host's state, which includes:

- General-Purpose Registers (Excludes `rsp`, `rip` and `rflags` in that they are already saved in VMCS)
- Control Registers (Excludes `cr8` in that it is subject to be virtualized)
- Debug Registers (Excludes `dr7` in that it is already saved in VMCS)
- Extended State (We may use `xsave` instruction)

To run a vCPU, the scheduler should load state for Guest's vCPU. In addition to the state above, the scheduler should load following state to VMCS:

- Control Registers (Includes `cr0`, `cr3`, `cr4`)
- Debug Registers (Includes `dr7`)
- General-Purpose Registers (Includes `rsp`, `rflags` and `rip`)
- Segment Registers
- Model-Specific Registers (Includes `efer`, etc.)

All states other than General-Purpose Registers are subject to be cached so that we won't have to frequently load them into VMCB. The following union defines the states to be cached by NoirVisor:

```C
typedef union _noir_cvm_vcpu_state_cache
{
	struct
	{
		u32 gprvalid:1;		// Includes rsp,rip,rflags.
		u32 cr_valid:1;		// Includes cr0,cr3,cr4.
		u32 cr2valid:1;		// Includes cr2, uncached in Intel VT-x.
		u32 dr_valid:1;		// Includes dr7.
		u32 sr_valid:1;		// Includes cs,ds,es,ss.
		u32 fg_valid:1;		// Includes fs,gs,kgsbase.
		u32 dt_valid:1;		// Includes idtr,gdtr.
		u32 lt_valid:1;		// Includes tr,ldtr.
		u32 sc_valid:1;		// Includes star,lstar,cstar,sfmask.
		u32 se_valid:1;		// Includes esp,eip,cs for sysenter.
		u32 tp_valid:1;		// Includes cr8.tpr, unused in Intel VT-x.
		u32 ef_valid:1;		// Includes efer.
		u32 pa_valid:1;		// Includes pat.
		u32 reserved:18;
		u32 synchronized:1;
	};
	u32 value;
}noir_cvm_vcpu_state_cache,*noir_cvm_vcpu_state_cache_p;
```

If the host changed some of the state of vCPU, its corresponding bit should be cleared so that the scheduler would copy these contents to the VMCS.

## World Switch - Guest to Host
The condition to switch the processor context from the guest to the host is certain VM-Exits from the guest. Not all VM-Exits require a world switch. VM-Exits that require a world switch includes:
- External Interrupts. (Includes external interrupts and `NMI`s)
- I/O Instruction. (Includes `in`, `rep outsb` instructions, etc.)
- Processor Halt. (The `hlt` instruction)
- Triple-Fault.
- Hypercall (The `vmmcall` instruction)
- User-defined interceptions (Exceptions, `cpuid` instruction, etc.)

Only vCPU states not saved into VMCS should be saved for Guest and loaded for Host.

## World Switch - vCPU Migration
To migrate a vCPU to another logical processor, the VMCS must undergo a sequence of `vmclear`, `vmptrld` and `vmlaunch`. Otherwise, things could go wrong.

## Interception Settings
Certain interceptions must be set to ensure the correct functionality of NoirVisor CVM.

### Interrupt Interception
In that guests are not supposed to take external interrupts from real hardware, they must be intercepted and passed to the host. We may set `external-interrupt exiting` and `NMI exiting` bits in `Pin-Based VM-Execution Controls` field in VMCS. <br>
In terms of external interrupts, we should set the `acknowledge interrupt on exit` to `false` in `VM-Exit Controls` fields of VMCS, so external interrupts would be held pending. Simply returning to the host, like what we did in AMD-V, should have the external interrupts to be delivered. <br>
In terms of NMIs, we should inject the NMIs to the host in order to deliver them. In addition to event injection, the `blocking-by-NMI` bit of `Interruptibility-State` in VMCS for the host should also be set.

### CPUID Interception
The `cpuid` instruction must be intercepted by NoirVisor, even though host may choose not to intercept it. If the host chooses not to intercept `cpuid` instruction, NoirVisor would handle the `cpuid` instruction itself. Otherwise, switch the world to the host so that host will be handling Guest's `cpuid` instruction.

### INVD Interception
The `invd` instruction must be intercepted by NoirVisor. The `invd` instruction invalidates all caches without writing them back to the main memory. In this regard, NoirVisor must take responsibility to prevent the guest purposefully corrupting the global memory. Virtualization of `invd` instruction is actually simple: execute `wbinvd` instruction on exit of `invd`. At least this is what Hyper-V would do. Intel VT-x forces any hypervisor developers to intercept `invd` instruction.

### CR8 Interception
In that Intel VT-x lacks support of local APIC, which is instead present in AMD-V, there is no space left for the `cr8` register. Therefore, NoirVisor must emulate the behavior of the `cr8` register. <br>
When an external interrupt is to be injected, the priority of the interrupt will be compared with `cr8` value. If the priority of the interrupt is greater, the interrupt will be injected. Otherwise, it will be held pending. <br>
When writes to `cr8` register is intercepted, it will check if there is pending external interrupts and inject it accordingly. Nevertheless, if the guest has `RFLAGS.IF` cleared, the interrupt is still pending even if the priority is high enough. Intercept the interrupt-window in order to get it injected. <br>
When reads from `cr8` register is intercepted, it will return the virtualized value of `cr8` register.

### I/O Interception
I/O Interceptions are mandatory in that I/O operations in Guest are not supposed to go to real hardwares. Instead, they should go to virtual appliances, which Host is supposed to emulate. Switch the world to the host so that host will be handling Guest's I/O. Intel VT-x can specify `Unconditional I/O Exits` in `Primary Processor-based VM-Execution Control` field of VMCS, so it may save space so that we don't have to allocate pages for I/O bitmaps.

### MSR Interception
Similar to the `cpuid` instruction, Host can choose whether to intercept this instruction or not. If the host does not intercept MSR-Related instructions, NoirVisor would handle them and masking certain operations (e.g: accessing the `EFER` MSR). Otherwise, NoirVisor would switch to the Host to handle the instructions. Intel VT-x can specify not to `Use MSR Bitmaps` in `Primary Processor-based VM-Execution Control` field of VMCS, so we may only allocate a page in case the host do not intend to intercept MSR accesses.

### Triple-Fault
NoirVisor won't handle triple-fault itself. Switch to the host so the Host may handle it.

### VMX Interception
Currently, NoirVisor does not support a hypervisor to be nested inside a CVM. Simply inject a `#UD` exception to guest if VMX instructions are intercepted. Do not switch to the host to indicate the guest is attempting to execute VMX instructions.

### Exception Interception
Exception interception is optional: it is only intercepted if the User Hypervisor specifies so. The `#MC` is exceptions to this rule: `#MC` must be intercepted in that this is a physical-hardware-issued exception, which must be taken by host interrupt handler.

### EPT Violation
EPT Violation indicates that a wrong physical address is accessed. NoirVisor would not handle EPT Violation itself, but transfer the interception to the host. Unlike AMD-V, albeit information like access type would be recorded, fetched instruction bytes would not be recorded.