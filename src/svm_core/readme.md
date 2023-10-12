# NoirVisor - SVM Core
This directory is the virtualization engine based on AMD-V. \
All code in this directory should be cross-platform designed.

# Files
svm_main.c is the code file that initializes, sets up, and finalizes the virtualization engine based on AMD-V. \
svm_exit.c is the code file that handles all the VM-Exits derived from the processor. \
svm_cpuid.c is the code file that handles the VM-Exits induced by CPUID instruction. \
svm_def.h defines basic structures for AMD-V, details regarding the VMCB. \
svm_exit.h defines defines basic constants, and miscellaneous stuff for VM-Exit. \
svm_vmcb.h defines macros for operating the VMCB and offsets of fields in VMCB. \
svm_cpuid.h defines facilities for CPUID VM-Exit handlers.

# Features
NoirVisor uses the following AMD-V features:
- Decode Assists. This feature is the basic requirement. Otherwise, NoirVisor don't know what is going on in the VM-Exit.
- Next-RIP Saving. This feature is the basic requirement, Otherwise, NoirVisor don't know how to advance the instruction pointer.
- MSR Permission Map. This feature enables the stealth MSR-hook for syscall and sysenter hook.
- Address Space Identifier. This is also known as ASID. This feature would tag the TLBs with an ASID so that - on VM-Entry and VM-Exit - processor would not invalidate TLB.
- VMCB Clean Bits. This is used for VMCB state caching. This feature would allow processor to cache the VMCB state so that the processor has a better performance in virtualization.

# Stealth MSR-hook Algorithm
This feature utilizes the processor ability to intercept rdmsr instruction. \
Set the bit in bitmap to intercept LSTAR or SYSENTER_EIP MSR-read. \
On interception, edit the eax and edx register to represent the original value. \
Don't forget to set the bit in interception list in VMCB.

## The KVA-Shadow Problem upon MSR-Hook
The detail of the problem is stated in the [readme document of VT-Core](../vt_core/readme.md#the-kva-shadow-problem-upon-msr-hook). However, it seems that Windows do not enable KVA-Shadow mechanism on AMD processors. \
AMD-V does not support masking `#PF` exceptions according to the exception error-code. Therefore, there could be tons of unwanted `#PF` VM-Exits.

## Supervisor-Mode Access Prevention Problem
The detail of the problem and its solution is stated in the [readme document of VT-Core](../vt_core/readme.md#supervisor-mode-access-prevention-problem).

# Stealth Inline Hook Algorithm
This feature utilizes the Nested Paging feature of processor. \
The difference between Intel and AMD is that AMD lacks the "execution-only page" feature. In this regard, algorithm that applied on Intel EPT cannot be applied to AMD NPT. \
The idea originally comes from tandasat's repository SimpleSvmHook (https://github.com/tandasat/SimpleSvmHook), but with my additional optimizations regarding it. \
Note: In that AMD-V lacks the "execution-only" page feature, it is unrecommended to use stealth inline hook feature on AMD machines.

## Algorithm Detail
Setup two page tables. They are called Primary Page Table and Secondary Page Table. \
The Primary Page Table grants all accesses to all pages except that the hooked page is revoked execution access. At this moment, PTE points to original page. \
The Secondary Page Table grants R/W accesses, without execution access, to all pages except that the hooked page is given execution access in addition. At this moment, PTE points to hooked page. \
Load Primary Page Table into VMCB in the first time. On execution, VM-Exit due to #NPF occurs. So swap the nCR3 to the Secondary Page Table and issue VM-Entry. When instruction pointer goes outside, #NPF occurs and then swap the nCR3 to Primary Page Table and issue VM-Entry.

## Algorithm Issue
When instruction pointer is inside the hooked page, code may recognize the patch since the read access is granted. This is special case. \
There is performance problem as well. There will be no performance cost for reading the hooked page. But performance penalty on executing the hooked page may be significant in that:

1. When hooked function is called, #NPF occurs and swapped to the Secondary.
2. The jump instruction is executed, #NPF occurs and swapped to the Primary.
3. The proxy function may detour to function remainder, #NPF occurs and swapped to the Secondary.
4. Hooked function execution completed, #NPF occurs and swapped to the Primary.
5. The hooked function may invoke other functions in other pages.

## Potential Optimization
Since the hooked function may invoke other functions in other pages, we may grant execution permission to these pages in the Secondary Page Table. For example, `NtSetInformationFile` routine in Windows would call functions like `ObReferenceObjectByHandle`, `NtfsFsdSetInformation`, etc., and the proxy function, so unless these pages include code which detects hooks, it is a fairly good idea to permit executions for these pages on the Secondary Page Table. \

In summary, there will be at least four #NPF occurring in a single execution of hooked function. In comparison, Intel EPT offers flexibility that intensive access can still have least performance penalty.

# Critical Hypervisor Protection
This feature is an essential security feature. I found this feature missing in most open-source light-weight hypervisor project. The key is that VMCB and other essential pages are not protected through Nested Paging even if they enabled AMD NPT. It should be pointed out that a malware is aware of the format of VMCB. In this regard, malware may corrupt the VMCB through memory access instruction.

# Real-Time Code Integrity
Real-Time CI is now implemented by AMD Nested Paging.

# Type-I Hypervisor
If NoirVisor is loaded as a Type-I hypervisor, it is responsible to handle APIC-related accesses.

## Nested Paging
Upon system subversion, NoirVisor would set interception of writes to the APIC Page.

### Nested Page-Fault Handler
When write to the APIC Page is intercepted, NoirVisor would check do the following:

1. Check the offset of APIC Page Access.
2. If the access is on the Interrupt Control Register (Low), redirect the access to the shadowed page so that when single-stepping, NoirVisor can inspect the value being written to the ICR.
3. Single-step the guest.

### Debug Exception Handler
In that the Step 3 specifies single-stepping the guest, the `#DB` exception will be generated. On interception of `#DB` exception:

1. Inspect the value being written if the access is targetting ICR. The value being written is stored in the shadow page.
2. If the access is writing the ICR and the message is SIPI:
	1. discard the message if target processor is not in Wait-for-SIPI state.
	2. send the message otherwise by marking the spin-lock is released.
3. If the access is writing the ICR but the message is INIT:
	1. discard the message if target processor is already in Wait-for-SIPI state.
	2. send the message otherwise by forwarding this write to the APIC page.
4. If the access is writing the ICR but the message is elsewise, forward this write to the APIC page.
5. Stop single-stepping.

## Security Exception Handler
According to AMD-V, the INIT signal will be held pending if `VM_CR.R_INIT` is not set. Therefore, this bit must be set. To intercept INIT signal with this bit set, NoirVisor should intercept `#SX` exception. \
Upon interception on `#SX` exception:

1. Emulate the `INIT` signal behavior: initialize various register states.
2. Emulate the Wait-for-SIPI state by enterring spin-lock.
3. Leave the spin-lock when an SIPI arrives. Emulate the SIPI as well.

# Future Feature (Roadmap)
In future, NoirVisor has following plans:

- Implement SVM-Nesting (This will be a long term project.)
- Implement Type-I hypervisor.

# SVM-Nesting Algorithm (Incomplete Version) for Global Hypervision
To nest another working hypervisor is the highest focus of Project NoirVisor. However, starting from the repository creation, this goal has not been satisfied yet. Here, I will state down the algorithm and it will be updated in future as problems arises. \
We will divide the problem into several sub-problems, as stated below:

- Host SVM-Related MSR and CPUID Virtualization
- Virtualize SVM Instructions
- Virtualize VM-Entry
- Virtualize VM-Exit
- Virtualize ASID
- Virtualize GIF
- Virtualize Local SVM APIC
- Virtualize Nested Paging
- Utilize Accelerated Nested Virtualization
- Virtualize L2 VMCB
- Devirtualize on-the-fly

## Glossary
L0: This term refers to the Host context. \
L1: This term refers to the Guest Context. \
L2: This term refers to the Nested Guest Context.

## Host SVM-Related MSR and CPUID Virtualization
For this sub-problem, we intercept the rdmsr/wrmsr and cpuid instructions. \
The Guest enables and disables SVM by setting EFER.SVME bit. \
Also, we should intercept access to all SVM-Related MSRs. \

## Virtualize SVM Instructions
There are eight SVM instructions: `vmrun`, `vmload`, `vmsave`, `vmmcall`, `clgi`, `stgi`, `invlpga`, `skinit`. Instructions should be virtualized accordingly. \
In essence, `vmrun` instruction takes the place of VM-Entry and VM-Exit. We will leave this to special chapters - Virtualize VM-Entry and Virtualize VM-Exit. \
For `vmload` and `vmsave` instructions, since they can be accelerated by processor's built-in acceleration feature, we will leave this to special chapter - Virtualize `vmload` and `vmsave`. \
For `clgi` and `stgi` instructions, since they can be accelerated by processor's built-in acceleration feature, we will leave this to special chapter - Virtualize GIF. \
For `invlpga` instruction, we pass parameters to invalidate specific TLB entry in Nested Guest. \
For `skinit` instruction, this is somewhat optional since it is used only for security oriented virtualization. Even VMware's hypervisor does not emulate this.

## Virtualize VM-Entry
Basically, this is intercepting the vmrun instructions. This instruction loads VMCB by address saved in rax register. \
Then, we follow the steps indicated in AMD64 architecture programming manual. Details will be omitted here. The steps are:

- Saving Host State. We save host state to the address indicated by `SVM_HSAVE_PA` MSR.
- Validate and Load Guest State. We should check consistency of the Guest State from VMCB. If no inconsistency is found, load the Guest State to L2 VMCB. Otherwise, fails the consistency check and issue VM-Exit.
- Load Control State. Again, check inconsistency check, like ASID and `vmrun` interception. If no inconsistency is found, load Control State to L2 VMCB. Otherwise, fails the consistency check and issue VM-Exit.
- Virtualize GIF. We emulate that GIF is set.
- Finally, execute vmrun instruction with address of L2 VMCB, concluding the VM-Entry.

More details will be included in chapter Virtualize L2 VMCB.

## Virtualize VM-Exit
Steps are given in the following:

- On interceptions, we copy guest state to the VMCB given by the Nested Hypervisor.
- Load the address of VMCB as rax register to L1 VMCB.
- Load L1 Host State as Guest State to L1 VMCB.
- Load Control State. Due to Decode Assist and Next-RIP-Saving features, we need to set something in L2 VMCB Control State, such as Exit Reason, Information, Next RIP, etc.
- Virtualize GIF. We emulate that vGIF is cleared.
- Finally, execute vmrun instruction with address of L1 VMCB, concluding the VM-Exit.

## Differentiate Hypercall Source

## Virtualize ASID
In NoirVisor, the L0 context use ASID=0, and L1 context use ASID=1. To virtualize ASID, we should use ASID>1 for any L2 context. For a simple algorithm, we increment ASID by 1 to virtualize it. If L2 ASID<2, then the VMCB is inconsistent. In CPUID, we decrement the ASID range by 1. \
If nested virtualization is disabled, all ASIDs are reserved for Customizable VMs.

## Virtualize GIF
GIF is quite a special feature in AMD-V. It is a global flag that controls the interrupt behavior of the processor. To virtualize GIF, we should identify two scenarios and do what should be done. \
- The processor provides hardware support for virtualizing GIF. As far as I know, processors starting at Ryzen should be providing this feature.
- No hardware support for virtualizing GIF. VMware Workstation 16.1.2 does not provide this feature.

### GIF Virtualization with Hardware Support
The hardware support of virtualizing GIF is actually irrelevant to nested virtualization in terms of global virtualization projects. The `vGIF` controls only virtual interrupts. In other words, the `vGIF` has no effects against any physical interrupts, including `NMI`s and `SMI`s. \
However, it should be useful with nested virtualization within the scope for CVM.

### GIF Virtualization without Hardware Support
In that the hardware support of GIF virtualization is irrelevant to nested virtualization for NoirVisor, there must be something done to ensure correct behavior of GIF Virtualization. \
If the `vGIF` is cleared, all interrupts affected by GIF must be intercepted. In addition to interceptions, the host IDT should be replaced by NoirVisor in order to remove pending interrupts and record corresponding information of the interrupt. \

#### Masking Physical Interrupts
As a matter of fact, there is no need to intercept physical interrupts. The Local APIC support provided by the AMD-V is sufficient to keep interrupts pending. Setting the `V_INTR_MASKING` bit in VMCB and clearing the `RFLAGS.IF` bit in host should hold any physical interrupts pending regardless of the value of `RFLAGS.IF` in guest.

#### Masking NMIs
There is no easy way to mask `NMI`s like we did for masking physical interrupts. The `NMI` handler must be hijacked. When `NMI`s are intercepted, execute the `stgi` instruction to let the processor discard the pending `NMI`. \
The algorithm of hijacked `NMI` handler is simple: mark the vCPU has a pending `NMI`. However, do not execute the `iret` instruction in order to return because doing so would cancel the masking of `NMI`s. \
Likewise, when there is a pending `NMI`, the `iret` instruction must be intercepted and emulated so that masking of `NMI` is unaffected. \
When unmasking `NMI`s (i.e: the `vGIF` is set), remove interceptions of `NMI`s and `iret` instructions. However, emulating the `iret` instruction is required on `iret`-interception. Otherwise, the stack may conflict if the handler of `NMI` specified `IST`-mechanism.

##### NMI Masking of Matryoshka
There is a scenario where NMI is already masked but the processor has just entered state of `GIF=0`. This scenario is called NMI Masking of Matryoshka (i.e: Nested NMI Masking; two or more factors are masking NMIs). Such scenario could happen in two ways:

- The program executed `clgi` instruction in an NMI handler.
- The program triggered a VM-Exit during NMI.

In either way, the processor keeps the blocking of NMI internally. NoirVisor's logically-emulated masking of NMI is ineffective: no NMIs will be intercepted, so leaving the `vGIF=0` is fine with processor's internal-blocking of NMI. \
Even if the guest executed `iret` instruction while `vGIF=0`, where internal-blocking of NMI is removed by the processor, the interception of NMI is still in effect as indicated in VMCB. \
In a word, NMI Masking of Matryoshka does not affect the logic of masking NMIs from virtualized GIF.

#### Masking Debug Exceptions
The method to mask `#DB`s are simple: intercept the `#DB` exception. \
Please note that `#DB` trace trap due to `RFLAGS.TF` bit is not to be held pending, so forward the `#DB` by injecting it.

#### Masking SMIs
General hypervisors which do not gain control of system-management mode cannot hijack SMIs, so SMIs cannot be blocked when `vGIF=0`. Plus, the firmware may lock accesses to system-management mode, so interceptions of SMIs are actually ignored by the processor. This is a thereby fundamental flaw in an otherwise perfect global hypervisor that supports nested virtualization. \
By virtue of this, current implementation of NoirVisor would give up masking SMIs. Future implementation of NoirVisor, if to be integrated in firmware, would properly mask SMIs.

#### Masking INIT signals
In that NoirVisor would specify redirecting `INIT` signals to `#SX` exceptions, we are actually intercepting `#SX` exceptions in order to mask `INIT` signals. Mark there is pending `INIT` signal in the vCPU. When `vGIF` is set, we will be unmasking this `INIT` signal. Emulate the `INIT` signal if `VM_CR.R_INIT` is not set. Otherwise, inject an `#SX` exception.

#### Masking Machine-Checks
The logic of masking `#MC` exceptions is simple: mark the vCPU has a pending `#MC` exception, and inject the `#MC` exception once the `vGIF` is logically set.

#### Unmasking Interrupts
The AMD64 Architecture defines priorities for pending interrupts. In that `#MC` aborts, `NMI`s and `INIT` signals are held pending while `GIF=0`, we need to take care about their orders when unmasking. \
Generally, the relationship of their priorities are: `#MC` aborts > `INIT` signals > `NMI`s.

### GIF Logics
As defined by AMD, certain interrupts are controlled by GIF:
| Interrupt Source								| Actions on `GIF=0` Scenario	| Actions on `GIF=1` Scenario	|
|-----------------------------------------------|-------------------------------|-------------------------------|
| `#DB` Trap due to breakpoint register match   | Discarded						| Act as usual					|
| `#DB` Trap due to single-stepping				| Act as usual					| Act as usual					|
| `INIT` Signal									| Held Pending					| Act as usual					|
| `NMI` - Non-Maskable Interrupt				| Held Pending					| Act as usual					|
| External `SMI`								| Held Pending					| Act as usual					|
| Internal `SMI`								| Discarded						| Act as usual					|
| External Interrupts and Virtual Interrupts	| Held Pending					| Act as usual					|
| `#MC` Exception								| Held Pending					| Act as usual					|

Here is a table that lists the conditions that change the GIF.
| Condition				| Result		|
|-----------------------|---------------|
| `stgi` Instruction	| Sets GIF		|
| `clgi` Instruction	| Clears GIF	|
| `vmrun` Instruction	| Sets GIF		|
| VM-Exit				| Clears GIF	|

## Virtualize Local SVM APIC
There is a `V_INTR_MASKING` control bit in VMCB by virtue of the Local APIC support in AMD-V. In that the behavior of interrupt is controlled by the `RFlags.IF` bit of the host if the `V_INTR_MASKING` bit is set, ignorance of this bit may trigger deadlocking of the system: no physical interrupts can be intercepted because the `RFlags.IF` bit in host is reset. In order to address this issue, on VM-Entry of nested guest, the `V_INTR_MASKING` bit in VMCB must be checked. Copy the `RFlags.IF` bit from the guest to the host if `V_INTR_MASKING` bit is set.

## Virtualize Nested Paging
To virtualize NPT, we should merge the page tables. However, I don't have an algorithm regarding page-table merging. So, the SVM-nesting feature in future NoirVisor may not support NPT unless I have one. \
An easier, and high-performance, but protection-degraded approach to achieve this goal is to simply pass the Nested CR3 from the guest to the processor. This approach is fine for regular virtual machine monitors like VMware Workstation, VirtualBox, etc. because the physical memories to be virtualized do not conflict with NoirVisor's NPT Protection. However, this approach is indeed unsuitable to global hypervisors like NoirVisor itself.

## Virtualize `vmload` and `vmsave`
Newer models of processors that supports AMD-V allows automatic virtualization of `vmload` and `vmsave` instructions. This feature would automatically translate the input of these two instructions and load / store the processor state into VMCB so that these two instructions could behave correctly without triggerring VM-Exits. However, some details are worth mentioning while we utilize this feature.

- The `EFER.SVME` bit must be monitored.
- If the `EFER.SVME` bit of Guest is reset, intercept the `vmload` and `vmsave` instructions (and throw `#UD` when executed).
- If the `EFER.SVME` bit of Guest is set, stop intercepting the `vmload` and `vmsave` instructions so that acceleration is ensured.

## L2 Virtual Machine Control Block
Here, I re-emphasize framework of how nested virtualization works:

- VM-Entry is moving L1 to L2. More specifically, it is moving from L1 to L0, then to L2.
- VM-Exit is moving L2 to L1. More specifically, it is moving from L2 to L0, then to L1.

However, VMCB constructed by Guest cannot be directly passed to vmrun instruction. Hence, we define two types of VMCB: L2C-VMCB and L2T-VMCB:

- L2C-VMCB is allocated by guest. The nested hypervisor will be responsible to modify it.
- L2T-VMCB is allocated by host. NoirVisor will be responsible to synchronize between them.

For basic implementation, each vCPU should have one L2T-VMCB. They behave in following manner:

- On VM-Exit induced by vmrun instruction, we synchronize the L2C Guest State and Control Area to L2T-VMCB.
- On VM-Exit induced by nested guest execution, we synchronize the L2T Guest State and Control Area to L2C-VMCB.

### L2 VMCB Optimization: State Caching
AMD64 Architecture Manual stated a processor feature: VMCB State Caching. \
In essence, it specifies following processor states that can be cached:

- Interception Vectors, TSC Offset and Pause Filter Count.
- I/O Permission Bitmap and MSR Permission Bitmap.
- Address Space Identifier - that is, TLB tagging.
- Nested Paging - Paging Structure, Paging Base, PAT, and Encrypted NP State.
- Control Registers and Debug Registers.
- Interrupt/Global Descriptor Table.
- CS/DS/ES/SS Segment Registers State and CPL.
- Last Branch Record.
- Advanced Virtual Interrupt Controller.

Also, as defined in architecture manual, following processor states will not be cached:

- TLB Control.
- Interrupt Shadow.
- VMCB Status Fields.
- Event Injections.
- RFlags, RIP, RSP, RAX registers.

We may assume that uncached state is always dirty. If certain state is not marked as clean in L2C VMCB Clean Fields, that state is dirty. Dirty state should be synchronized to the L2T VMCB. Therefore, hypervisors that always leave VMCB-Clean Fields as zeros would suffer a great performance penalty.

### Organization of Data Structures for Nested vCPUs
It should be expected that nested hypervisor would run multiple VMCBs. If all L2C VMCBs have to overwrite the same L2T VMCB, then performance could be a grave issue. Therefore, certain amount of VMCBs must be cached. \
The data structure that holds them would be sorted arrays that allow binary searches. The array should be sorted in accordance to the physical address of VMCB so that, on VM-Exit due to `vmrun` instruction, NoirVisor can locate the corresponding L2T-VMCB with `O(logn)` complexity. \

## Devirtualize on-the-fly
Since NoirVisor is possible to be unloaded before the nested hypervisor ends, it is necessary to put nested hypervisor directly onto the processor - that is, L1 becomes L0 and L2 becomes L1. \
This is a suggestion for Nested Virtualization in NoirVisor. By now, algorithm design will not be made. After the Nested Virtualization feature completes, NoirVisor may refuse unloading as long as the nested hypervisor did not leave. \

## Nesting VMware's Hypervisor
In order to nest VMware's hypervisor, NoirVisor has to support features that VMware's hypervisor also supports. It is observed that:

- Nested Paging is supported by VMware Workstation.
- SVM Lock is supported by VMware Workstation. (It is unlikely that nesting VMware's Hypervisor requires this feature)
- Next-RIP Saving is supported by VMware Workstation.
- VMCB Clean Bits is supported by VMware Workstation.
- Flush TLB by ASID is supported by VMware Workstation.
- Decode Assists is supported by VMware Workstation.

## Roadmap of Nested Virtualization
Nested Virtualization in NoirVisor will be developped in three stages:
- Simple (No NPT, single VMCB): Like [SimpleSvm](https://github.com/tandasat/SimpleSvm) with removal of NPT.
- Intermediate (with NPT, single VMCB): Like [SimpleSvmHook](https://github.com/tandasat/SimpleSvmHook)
- Advanced (with NPT, multiple VMCBs): Like [VMware Workstation](https://www.vmware.com/products/workstation-pro/workstation-pro-evaluation.html), [VirtualBox](https://www.virtualbox.org/), [NoirVisor](https://github.com/Zero-Tang/NoirVisor) or Hyper-V.

# Customizable VM Scheduler Algorithm
This chapter describes the CVM scheduler algorithm for SVM-Core.

## World Switch - Host to Guest
The only condition to switch the processor context from the host to the guest is the host's hypercall to request runnning a guest vCPU. This hypercall specifies the target vCPU so that NoirVisor would switch to a correct vCPU. \
Prior to running a vCPU, NoirVisor should save the processor states. The states subject to be manually saved by NoirVisor includes:
- General-Purpose Registers. (Excludes `rax`,`rsp`,`rip`,`rflags` because they are saved in VMCB)
- Debug Registers. (Excludes `dr6` and `dr7` because they are saved in VMCB)
- Extended States. (We might use `xsaves` instruction to save extended state)

To run a vCPU, NoirVisor should load the processor states for the Guest. In addition to the registers mentioned above, NoirVisor should load the following registers to VMCB:
- Control Registers. (Includes `cr0`,`cr2`,`cr3`,`cr4`,`cr8`)
- Debug Registers. (Includes `dr6`,`dr7`)
- General-Purpose Registers. (Includes `rax`,`rsp`,`rip`,`rflags`)
- Segment Registers.
- Model-Specific Registers. (Includes `efer`, etc.)

All states other than General-Purpose Registers are subject to be cached so that we won't have to frequently load them into VMCB. The following union defines the states to be cached by NoirVisor:

```C
typedef union _noir_cvm_vcpu_state_cache
{
	struct
	{
		u32 gprvalid:1;		// Includes rsp,rip,rflags.
		u32 cr_valid:1;		// Includes cr0,cr3,cr4.
		u32 cr2valid:1;		// Includes cr2.
		u32 dr_valid:1;		// Includes dr6,dr7.
		u32 sr_valid:1;		// Includes cs,ds,es,ss.
		u32 fg_valid:1;		// Includes fs,gs,kgsbase.
		u32 dt_valid:1;		// Includes idtr,gdtr.
		u32 lt_valid:1;		// Includes tr,ldtr.
		u32 sc_valid:1;		// Includes star,lstar,cstar,sfmask.
		u32 se_valid:1;		// Includes esp,eip,cs for sysenter.
		u32 tp_valid:1;		// Includes cr8.tpr.
		u32 ef_valid:1;		// Includes efer.
		u32 pa_valid:1;		// Includes pat.
		u32 reserved:18;
		u32 synchronized:1;
	};
	u32 value;
}noir_cvm_vcpu_state_cache,*noir_cvm_vcpu_state_cache_p;
```

If the host changed some of the state of vCPU, its corresponding bit should be cleared so that NoirVisor would copy these contents to the VMCB.

## World Switch - Guest to Host
The condition to switch the processor context from the guest to the host is certain VM-Exits from the guest. Not all VM-Exits require a world switch. VM-Exits that require a world switch includes:
- External Interrupts. (Includes `physical INTR`, `NMI`, and `SMI`)
- I/O Instruction. (Includes `in`, `rep outsb` instructions, etc.)
- Processor Halt. (The `hlt` instruction)
- Shutdown Condition. (Includes Triple-Fault, etc.)
- Hypercall (The `vmmcall` instruction)
- Return from SMM (The `rsm` instruction)
- User-defined interceptions (Exceptions, `cpuid` instruction, etc.)

Only vCPU states not saved into VMCB should be saved for Guest and loaded for Host.

## World Switch - vCPU Migration
To migrate a vCPU to another logical processor, it is required to clear the VMCB clean bits before actually scheduling it onto the vCPU.

## vCPU Relocation
For large-scale systems, Non-Uniform Memory Architecture (NUMA) is very common. Memory affinity can significantly affect performance. Current implementation of NoirVisor, however, does not support relocating the vCPU to a different node. Nevertheless, support of relocation is in plan. It is recommended to set the host thread affinity of the vCPU so that the vCPU won't be scheduled onto a distant physical core.

## Emulation of MTF
The `Monitor Trap Flag (MTF)` feature on Intel VT-x is a very powerful feature to emulate per-instruction. However, AMD-V lacks this feature, so this feature must be emulated. \
This chapter discusses how to emulate the MTF in AMD-V.

### Trap-Flag
MTF is similar to Trap-Flag in standard x86. It will generate a `#DB` exception per instruction execution. However, in following conditions, the `#DB` instruction will not be generated:

- An interrupt arrives, including exceptions, software interrupts and external interrupts.
- The `BTF` flag in `DEBUG_CTRL` MSR is set.
- The processor is under an interrupt shadow. (e.g.: `mov` to `ss` or `sti` instruction)
- The `syscall` is executed with `SFMASK.TF` bit set.

### Hiding the Trap-Flag
There are some conditions that may expose the `rflags` register to the software:

- An interrupt arrives. This may copy the `rflags` register to the interrupt frame on the stack.
- The `pushf` instruction may copy the content of `rflags` register to the stack.
- The `syscall` instruction may copy the content of `rflags` register to the `r11` register.

The following conditions may overwrite the `rflags.tf` bit:
- An interrupt arrives. It would clear the `rflags.if` and `rflags.tf` bits.
- The `iret`, `sysret` and `popf` instructions are executed.

### Interception Settings for MTF Emulation
The following interceptions must be set in order to emulate MTF. \
MTF Emulation is not currently implemented in NoirVisor.

#### Intercept Exceptions
All exceptions must be intercepted because they will generate exception frames which includes the content of `rflags.tf` bit. \
In addition to exception interceptions, the event injection field must be watched. No injection will be handled by the processor: event injection will be emulated by NoirVisor. \
The `#DB` exception is triggered because the `rflags.tf` bit is set. If the `DR6.BS` bit is set but the shadowing `rflags.tf` bit is reset, then do not forward this exception in that this is actually an MTF VM-Exit.

#### Intercept Instructions
The `pushf`, `popf`, `int`, `icebp` and `iret` instructions will be intercepted because they can either read or write the `rflags` register. \
Emulate the read/write to the stack. As the emulation completes, an MTF VM-Exit is onset. \
The `syscall` and `sysret` instructions will be intercepted by clearing the `EFER.SCE` bit. Such interception is indicated by the interception to `#UD` exception.

#### Intercept Interrupts
This chapter only discusses the external interrupts. `NMI`s, exceptions and software interrupts are not included. \
Set the interceptions to virtual interrupt in VMCB. When the interception to virtual interrupt is taken, emulate the virtual interrupt.

#### Intercept MSRs
The `Debug Control` MSR must be intercepted in that the `BTF` bit seriously affects the behavior of single-stepping. \
The LBR register must be virtualized when emulating interrupts except `#DB` and `#MC` exceptions.

### Shadowed Bits in MTF Emulation
The following bits are shadowed in MTF Emulation.

- The `RFLAGS.TF` bit is shadowed in that this is the cornerstone to single-stepping.
- The `EFER.SCE` bit is shadowed in that `syscall` and `sysret` instructions must be intercepted.
- The `DEBUG_CTRL.BTF` bit is shadowed in that `BTF` seriously changes the behavior of `RFLAGS.TF` bit.

## Interception Settings
Certain interceptions must be set to ensure correct functionality of NoirVisor CVM.

### Interrupt Interception
In that Guests of CVM are not supposed to handle external interrupts from real hardwares, any external interrupts must be intercepted. We may set interceptions upon `physical INTR`, `NMI` and `SMI`. We do not have to intercept `virtual INTR` because they are subject to be injected by hypervisor. Interceptions upon `virtual INTR` may induce dead loop. \
Upon interception, perform a world switch. Specify the reason of interception to be the scheduler's exit. For such reason of exit, because `GIF` becomes set when NoirVisor executes `vmrun` instruction in order to switch to host, interrupts will be handled by the Host.

### Interrupt Window Interception (Candidate 1 Design)
Interrupt Window means a specific opportunity that a vCPU can take interrupts. This feature is intended for User Hypervisors to forge an interrupt queue so that event injections are not lost if the Guest had not taken the previous interrupt yet. \
Unlike Intel VT-x, AMD-V does not provide a dedicated control to intercept Interrupt Windows. In that AMD-V provides a unique way to inject external interrupts via virtualized local APICs, injecting an event while Guest `RFlags.IF` can be emulated properly. Plus, untaken interrupt requests would remain still. Therefore, scheduler's exit can simply check if the `V_IRQ` bit in VMCB is cleared. If the bit is cleared while there was an external interrupt injection on record, issue a VM-Exit of interrupt-window interception and remove the record of event injection. \
AMD-V provides capability of intercepting `iret` and `popf` instruction. However, it requires parsing Guest stack and emulating the instruction. Such emulation could be arcane in the `GIF=0` context. Plus, AMD-V does not support intercepting `sti` instruction, so accurate interception of interrupt window is infeasible in AMD-V.

Also, intercepting the `iret` instruction does not mean an accurate approach of capturing `NMI` windows. According to Chapter 4.5 "NMI Mask", [AMD Supervisor Entry Extensions](https://www.amd.com/system/files/TechDocs/57115.pdf), the `iret` instruction, nevertheless, should be evaluated in that it does not necessarily mean that `NMI-blocking` is deactivated. By virtue of the difficulty of parsing the guest's stack, interception of `NMI-window` is currently remained unimplemented.

### Interrupt Window Interception (Candidate 2 Design)
Candidate 1 Design of intercepting Interrupt Window (excluding the NMI-Window) is quite imprecise. As such, this chapter defines a new approach to find the exact window.

There is an option to intercept virtual interrupts. In this way, we can set the `V_IRQ` bit. On interception of virtual interrupts, this means the vCPU is open to receive virtual interrupts. \
NoirVisor intercepts interrupt-window once the user hypervisor injects a virtual interrupt. In this regard, upon injection, we can set interception on virtual interrupt so that the interception will happen when the vCPU is taking the interrupt. On interception, we use the regular event injection method, which takes the same approach as injecting exceptions, in order to inject the virtual interrupt. As such, the virtual interrupt is taken by the vCPU, unencumbered from the VMCB's setting. Once the vCPU is able to take virtual interrupt again (i.e.: the `RFlags.IF` bit is set), the interception of virtual interrupt will happen again. NoirVisor will differentiate such interception by preplacing a bit to indicate that this intercepted dummy VIRQ is intended for indicating interrupt-window.

In future implementation of AMD-V (Zen 4), NMI-window can be intercepted in the similar approach.

### CPUID Interception
The `cpuid` instruction must be intercepted by NoirVisor, even though host may choose not to intercept it. If the host chooses not to intercept `cpuid` instruction, NoirVisor would handle the `cpuid` instruction itself. Otherwise, switch the world to the host so that host will be handling Guest's `cpuid` instruction.

#### CPUID Quick-Path
The User Hypervisor may specify a pre-defined value for guest vCPU. Therefore, interception of `cpuid` instruction does not have to exit to user-mode, and thereby increases the performance of `cpuid` emulation.

NoirVisor defines a preset of CPUID Quick-Paths that indicate what CPU functionalities are exposed to the Guest. \
There are 7 predefined standard leaves (0,1,5,6,7,B,D) and 9 extended leaves (0x8000_0000 to 0x8000_0008) in the CPUID Quick-Paths.

### RSM Interception
The `rsm` instruction is optionally intercepted by NoirVisor. If the host chooses not to intercept `rsm` instruction, NoirVisor would not specify the intercept bit in VMCB.

### INVD Interception
The `invd` instruction must be intercepted by NoirVisor. The `invd` instruction invalidates all caches without writing them back to the main memory. In this regard, NoirVisor must take responsibility to prevent the guest purposefully corrupting the global memory. Virtualization of `invd` instruction is actually simple: execute `wbinvd` instruction on exit of `invd`. At least this is what Hyper-V would do.

### I/O Interception
I/O Interceptions are mandatory in that I/O operations in Guest are not supposed to go to real hardwares. Instead, they should go to virtual appliances, which Host is supposed to emulate. Switch the world to the host so that host will be handling Guest's I/O. \
Future implementation of NoirVisor CVM may permit a guest to access the real hardware.

### MSR Interception
Similar to the `cpuid` instruction, Host can choose whether to intercept this instruction or not. If the host does not intercept MSR-Related instructions, NoirVisor would handle them and masking certain operations (e.g: accessing the `EFER` MSR). Otherwise, NoirVisor would switch to the Host to handle the instructions.

### Shutdown Interception
Usually, shutdown is induced by triple-fault. NoirVisor won't handle this interception itself. Switch to the host so the interception is transfered to the Host.

### SVM Interception
Currently, NoirVisor does not support a hypervisor to be nested inside a CVM. Simply inject a `#UD` exception to guest if SVM instructions are intercepted. Do not switch to the host to indicate the guest is attempting to execute SVM instructions.

### Exception Interception
Exception interception is optional: it is only intercepted if the User Hypervisor specifies so. The `#SX` and `#MC` are exceptions to this rule: `#SX` must be intercepted in that this means an `INIT` signal has arrived - unless the guest sets `R_INIT` bit in `VM_CR` MSR, `#SX` should neither be injected to guest nor be transfered to the host - and `#MC` must be intercepted in that this is a physical-hardware-issued exception, which must be taken by host interrupt handler.

### Nested Page Fault
Nested Page Fault, usually abbreviated as `#NPF`, indicates that a wrong physical address is accessed. NoirVisor would not handle `#NPF` itself, but transfer the interception to the host. In addition, information like fetched instruction bytes, access information, etc., would be recorded. \

#### Instruction Decode
In NoirVisor x Columbia Crossover Event, `#NPF` fault is now associated with MMIO (Memory-Mapped I/O). For any `#NPF`s which are not instruction fetches, instructions will be decoded by disassembler in order to extract MMIO operation details.

- If an instruction triggers `#NPF` read-fault, decode the size of source operand (which is the memory referenced by MMIO) and get the metadata of the destination operand.
- If an instruction triggers `#NPF` write-fault, decode the size of destination operand (which is the memory referenced by MMIO) and get the metadata of the source operand.
- Decode the instruction length for this MMIO instruction.

## Accessing Guest Physical Pages
NoirVisor does not keep GPA-to-HVA translation on track. However, NoirVisor, if installed as a Type-II hypervisor, loads a special page table that maps both an identity map of physical memory and regular system memory. Therefore, when NoirVisor has to operate the guest pages, the only required actions are translating the GVA to HPA. Not to mention if NoirVisor is installed as a Type-I hypervisor, only HPA can be accessed.

## SMM Virtualization
SMM, abbreviation that stands for System Management Mode, is a standard x86 component to handle critical system-control activities (e.g.: Power Management, Interactions with NVRAM on motherboard, etc.). \
This chapter provides a preliminary draft of virtualizing SMM.

### SMRAM Virtualization
SMRAM is a special memory area consists of 64KiB of memory. It is an independent RAM storage and is not located on the main system memory. For example, if the `SMBASE` is pointed to 0x00030000, memory accesses to `0x00030000-0x0003FFFF` are different from inside and outside of SMM. By virtue of this, User Hypervisor is responsible to set up a different mapping when the vCPU is entering SMM. \
The User Hypervisor may specify support for SMM so that, on VM creation, two sets of mapping will be created: one for regular mode, and the other for mapping SMRAM. They will have different ASIDs.

### Virtualizing SMI
NoirVisor defines a new type of interrupt to indicate the SMI. When `vGIF` is set and the processor is not in SMM, the pending SMI will be immediately injected by NoirVisor. The state of the vCPU is reset by NoirVisor when virtualizing SMM.

Returning from SMM is virtualized by NoirVisor by intercepting the `rsm` instructions.

### SMRAM Relocation
SMRAM relocation must be supported on multi-processing systems, or otherwise the system will face serious race conditions when multiple SMIs arrive on different cores simultaneously.

## APIC Virtualization
APIC, acronym that stands for Advance Programmable Interrupt Controller, is a standard x86 component to utilize multi-core processing. Although Customizable VM does not necessarily need APIC to call other cores - hypercalls can be used instead - virtualization of APIC is favorable at best. \
AMD-V supports a special feature called AVIC (Advanced Virtual Interrupt Controller) to accelerate APIC virtualization. NoirVisor may consult whether AVIC is supported. \
Current implementation of NoirVisor CVM does not utilize AVIC feature. Virtualization of APIC is considered as User Hypervisor's task.

### APIC Virtualization with AVIC
With AVIC, IPIs (Inter-Processor Interrupts) can be delivered without causing VM-Exits if target vCPU is already scheduled onto a physical core. 

#### Overall Architecture
There are three special kinds of pages: the `AVIC Backing Page`, `Physical APIC ID Table`, `Logical APIC ID Table`. They must be mapped to Write-Back cacheable memory type, according to AMD64 architecture.

- The `AVIC Backing Page` will reflect the writes from the Guest program to the APIC page. Each vCPU is given a unique page.
- The `Physical APIC ID Table` is unique among different virtual machines. All vCPUs within a VM share the same page. This page indicates the status of all vCPUs in this VM: their backing page pointer, whether they are running or not, which vCPU were the vCPUs assigned to, etc.
- The `Logical APIC ID Table` is unique among different virtual machines. All vCPUs within a VM share the same page. This page maps the logical APIC ID to the physical APIC ID in the VM.

#### World-Switch
AVIC won't automatically set and clear the `Is Running` bit in `Physical APIC ID Table` entry. NoirVisor should be setting this bit when scheduling vCPUs. \
When NoirVisor schedules a vCPU onto the physical core, the `Is Running` bit and `Host Physical APIC ID` field in the corresponding `Physical APIC ID Table` entry should be set accordingly. \
When NoirVisor schedules a vCPU out of the physical core, the `Is Running` bit should be cleared so AVIC will not deliver IPI to a wrong physical core but will trigger a VM-Exit instead.

### APIC Virtualization without AVIC
Currently implementation of NoirVisor requires the User Hypervisor to emulate the behavior of APIC accesses. \
User hypervisor must explicitly unmap the memory region of the APIC page so that any APIC-page accesses will result in Memory-Access interceptions. NoirVisor will help decoding the MMIO instruction. Contents in APIC page, however, must be virtualized by user hypervisor.

In future implementations of NoirVisor CVM, IPI-virtualization, without AVIC, can be handled by NoirVisor.

## NoirVisor Secure Virtualization
The NoirVisor Secure Virtualization (NoirVisor NSV) is a security extension to NoirVisor Customizable Virtual Machines.

### Secure Memory
NoirVisor NSV separates Guest Physical Memory from Host Physical Memory by construting isolated paging, meaning that permissions for accessing physical pages assigned to the guest would be revoked from the host and granted to the guest exclusively.

### NSV Violation
If the subverted host accesses Secure Memory, then `#NPF` is triggered. Such `#NPF` is refered as NSV-Violation. Current implementation of NoirVisor simply ignores these memory access instructions and continues the execution of the host.

## Encrypted Virtual Machine
AMD-V provides a mechanism for encrypted guests. This mechanism enables a guest to run in a manner where codes and data are encrypted and the only decrypted version of them are only available within the guest itself. It would require the hypervisor to enable the `SEV` (Secure Encrypted Virtualization) feature. Please note that, as the hypervisor is no longer able to inspect or alter all guest code or data, the hypervisor model is departing from the standard x86 virtualization model. \
Running a VM with SEV-enabled must have coordination with AMD Secure Processor, often abbreviated as AMD-SP, which is an external PCI device. Please note that VT-Core and SVM-Core of NoirVisor are considered to be CPU-driving. As such, coordination with AMD-SP is not included as a part of SVM-Core.

### Encrypted State
Optionally, the user hypervisor may choose to encrypt the state of vCPU. This would require the hypervisor to enable the `SEV-ES` (Secure Encrypted Virtualization: Encrypted State) feature. If this feature is enabled, the state of vCPU is no longer able to be directly inspected or altered, like the way of encrypted memory with `SEV` enabled. In order to properly emulate the instructions that may trigger NAEs (Non-Automatic Exits, e.g: interception of `cpuid` instruction), the secure processor would first issue a `#VC` exception - namely the VMM Communication exception, a contributory fault with vector 29 - to the guest so that the guest may copy necessary state to a decrypted memory area called GHCB, acronym that stands for Guest-Host Communication Block. The layout of GHCB can be custom because hardware would never access it directly. When the state of vCPU is copied successfully, the guest should execute the `vmgexit` instruction, an AE (Automatic Exit) which does not trigger `#VC` exception, to notify the hypervisor about the exit. \
To enable `SEV-ES`, an additional page must be allocated for `VMSA` (VM Save Area for Encrypted States). Coordination with AMD-SP is also required so that the initial encrypted image of vCPU state can be created properly.

### Secure Nested Paging
Optionally, the user hypervisor may choose to secure the nested paging. This would require the hypervisor to enable the `SEV-SNP` (Secure Encrypted Virtualization: Secure Nested Paging) feature.

### Interception Settings
For guests that have `SEV-ES` enabled, there are differences of AE and NAE. NoirVisor can handle AEs (e.g: physical interrupts) itself, but NAEs (e.g: `cpuid` instructions) must be properly handled by user hypervisors. Therefore, interceptions of physical interrupts, NMIs, etc. can be done normally. However, interceptions of `cpuid` instructions, interceptions of `MSR` operations, etc. must be handled by User Hypervisor.