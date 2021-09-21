# NoirVisor - SVM Core
This directory is the virtualization engine based on AMD-V. <br>
All code in this directory should be cross-platform designed.

# Files
svm_main.c is the code file that initializes, sets up, and finalizes the virtualization engine based on AMD-V. <br>
svm_exit.c is the code file that handles all the VM-Exits derived from the processor. <br>
svm_cpuid.c is the code file that handles the VM-Exits induced by CPUID instruction. <br>
svm_def.h defines basic structures for AMD-V, details regarding the VMCB. <br>
svm_exit.h defines defines basic constants, and miscellaneous stuff for VM-Exit. <br>
svm_vmcb.h defines macros for operating the VMCB and offsets of fields in VMCB. <br>
svm_cpuid.h defines facilities for CPUID VM-Exit handlers.

# Features
NoirVisor uses the following AMD-V features:
- Decode Assists. This feature is the basic requirement. Otherwise, NoirVisor don't know what is going on in the VM-Exit.
- Next-RIP Saving. This feature is the basic requirement, Otherwise, NoirVisor don't know how to advance the instruction pointer.
- MSR Permission Map. This feature enables the stealth MSR-hook for syscall and sysenter hook.
- Address Space Identifier. This is also known as ASID. This feature would tag the TLBs with an ASID so that - on VM-Entry and VM-Exit - processor would not invalidate TLB.
- VMCB Clean Bits. This is used for VMCB state caching. This feature would allow processor to cache the VMCB state so that the processor has a better performance in virtualization.

# Stealth MSR-hook Algorithm
This feature utilizes the processor ability to intercept rdmsr instruction. <br>
Set the bit in bitmap to intercept LSTAR or SYSENTER_EIP MSR-read. <br>
On interception, edit the eax and edx register to represent the original value. <br>
Don't forget to set the bit in interception list in VMCB.

# Stealth Inline Hook Algorithm
This feature utilizes the Nested Paging feature of processor. <br>
The difference between Intel and AMD is that AMD lacks the "execution-only page" feature. In this regard, algorithm that applied on Intel EPT cannot be applied to AMD NPT. <br>
The idea originally comes from tandasat's repository SimpleSvmHook (https://github.com/tandasat/SimpleSvmHook), but with my additional optimizations regarding it.

## Algorithm Detail
Setup two page tables. They are called Primary Page Table and Secondary Page Table. <br>
The Primary Page Table grants all accesses to all pages except that the hooked page is revoked execution access. At this moment, PTE points to original page. <br>
The Secondary Page Table grants R/W accesses, without execution access, to all pages except that the hooked page is given execution access in addition. At this moment, PTE points to hooked page. <br>
Load Primary Page Table into VMCB in the first time. On execution, VM-Exit due to #NPF occurs. So swap the nCR3 to the Secondary Page Table and issue VM-Entry. When instruction pointer goes outside, #NPF occurs and then swap the nCR3 to Primary Page Table and issue VM-Entry.

## Algorithm Issue
When instruction pointer is inside the hooked page, code may recognize the patch since the read access is granted. This is special case. <br>
There is performance problem as well. There will be no performance cost for reading the hooked page. But performance penalty on executing the hooked page may be significant due to:

1. When hooked function is called, #NPF occurs and swapped to the Secondary.
2. The jump instruction is executed, #NPF occurs and swapped to the Primary.
3. The proxy function may detour to function remainder, #NPF occurs and swapped to the Secondary.
4. Hooked function execution completed, #NPF occurs and swapped to the Primary.

In summary, there will be at least four #NPF occurring in a single execution of hooked function. In comparison, Intel EPT offers flexibility that intensive access can still have least performance penalty.

# Critical Hypervisor Protection
This feature is an essential security feature. I found this feature missing in most open-source light-weight hypervisor project. The key is that VMCB and other essential pages are not protected through Nested Paging even if they enabled AMD NPT. It should be pointed out that a malware is aware of the format of VMCB. In this regard, malware may corrupt the VMCB through memory access instruction.

# Real-Time Code Integrity
Real-Time CI is now implemented by AMD Nested Paging.

# Future Feature (Roadmap)
In future, NoirVisor has following plans:

- Implement SVM-Nesting (This will be a long term project.)
- Implement Customizable VM based on SVM-Core.

# SVM-Nesting Algorithm (Incomplete Version) for Global Hypervision
To nest another working hypervisor is the highest focus of Project NoirVisor. However, starting from the repository creation, this goal has not been satisfied yet. Here, I will state down the algorithm and it will be updated in future as problems arises. <br>
We will divide the problem into several sub-problems, as stated below:

- Host SVM-Related MSR and CPUID Virtualization
- Virtualize SVM Instructions
- Virtualize VM-Entry
- Virtualize VM-Exit
- Virtualize ASID
- Virtualize GIF
- Virtualize Nested Paging
- Utilize Accelerated Nested Virtualization
- Virtualize L2 VMCB
- Devirtualize on-the-fly

## Glossary
L0: This term refers to the Host context. <br>
L1: This term refers to the Guest Context. <br>
L2: This term refers to the Nested Guest Context.

## Host SVM-Related MSR and CPUID Virtualization
For this sub-problem, we intercept the rdmsr/wrmsr and cpuid instructions. <br>
The Guest enables and disables SVM by setting EFER.SVME bit. <br>
Also, we should intercept access to all SVM-Related MSRs. <br>

## Virtualize SVM Instructions
There are eight SVM instructions: `vmrun`, `vmload`, `vmsave`, `vmmcall`, `clgi`, `stgi`, `invlpga`, `skinit`. Instructions should be virtualized accordingly. <br>
In essence, `vmrun` instruction takes the place of VM-Entry and VM-Exit. We will leave this to special chapters - Virtualize VM-Entry and Virtualize VM-Exit. <br>
For `vmload` and `vmsave` instructions, since they can be accelerated by processor's built-in acceleration feature, we will leave this to special chapter - Virtualize `vmload` and `vmsave`. <br>
For `clgi` and `stgi` instructions, since they can be accelerated by processor's built-in acceleration feature, we will leave this to special chapter - Virtualize GIF. <br>
For `invlpga` instruction, we pass parameters to invalidate specific TLB entry in Nested Guest. <br>
For `skinit` instruction, this is somewhat optional since it is used only for security oriented virtualization. Even VMware's hypervisor does not emulate this.

## Virtualize VM-Entry
Basically, this is intercepting the vmrun instructions. This instruction loads VMCB by address saved in rax register. <br>
Then, we follow the steps indicated in AMD64 architecture programming manual. Details will be omitted here. The steps are:

- Saving Host State. We save host state to the address indicated by SVM_HSAVE_PA MSR.
- Validate and Load Guest State. We should check consistency of the Guest State from VMCB. If no inconsistency is found, load the Guest State to L2 VMCB. Otherwise, fails the consistency check and issue VM-Exit.
- Load Control State. Again, check inconsistency check, like ASID and VMRUN interception. If no inconsistency is found, load Control State to L2 VMCB. Otherwise, fails the consistency check and issue VM-Exit.
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

## Virtualize ASID
In NoirVisor, the L0 context use ASID=0, and L1 context use ASID=1. To virtualize ASID, we should use ASID>1 for any L2 context. For a simple algorithm, we increment ASID by 1 to virtualize it. If L2 ASID<2, then the VMCB is inconsistent. In CPUID, we decrement the ASID range by 1.

## Virtualize GIF
GIF is quite a special feature in AMD-V. It is a global flag that controls the interrupt behavior of the processor. To virtualize GIF, we should identify two scenarios and do what should be done. <br>
- The processor provides hardware support for virtualizing GIF. As far as I know, processors starting at Ryzen should be providing this feature.
- No hardware support for virtualizing GIF. VMware Workstation 16.1.2 does not provide this feature.

### GIF Virtualization with Hardware Support
When the guest `EFER.SVME` is clear, we should intercept `stgi` and `clgi` instructions, and inject `#UD` exception back to the guest. <br>
When the guest `EFER.SVME` is set, we should stop intercepting `stgi` and `clgi` instructions, and enable the `vGIF` hardware support.

### GIF Virtualization without Hardware Support
If there is no hardware support to GIF Virtualization, even if we intercept interrupts subject to be held pending, the external interrupts will eventually be taken when we enter the nested hypervisor's context. This means the nested hypervisor is actually **interruptible** while `GIF=0` logically. **Only interrupts to be discarded can be emulated properly.** This is thereby a flaw in an otherwise perfect global hypervisor that supports nested virtualization. For the sake of accurate nested virtualization, nested virtualization is infeasible in case there is no hardware support for GIF virtualization. <br>
Why is it feasible for hypervisors like VMware Workstation to implement Nested Virtualization on AMD-V without the hardware support of virtualizing GIF? The reason is simple: any external interrupts supposed to be delivered to the guest are injected by hypervisor, instead of being originated from real external hardwares, so VMM will delay the event injection until the virtual processor logically has the GIF set.

### GIF Logics
As defined by AMD, certain interrupts are controlled by GIF:
| Interrupt Source								| Actions on `GIF=0` Scenario	| Actions on `GIF=1` Scenario	|
|-----------------------------------------------|-------------------------------|-------------------------------|
| `#DB` Trap due to breakpoint register match   | Discarded						| Act as usual					|
| `INIT` Signal									| Held Pending					| Act as usual					|
| `NMI` - Non-Maskable Interrupt				| Held Pending					| Act as usual					|
| External `SMI`								| Held Pending					| Act as usual					|
| Internal `SMI`								| Discarded						| Act as usual					|
| External Interrupts and Virtual Interrupts	| Held Pending					| Act as usual					|
| `#MC` Exception								| Held Pending					| Act as usual					|

Here is a table that lists the conditions that change the GIF.
| Condition				| Result					|
|-----------------------|---------------------------|
| `stgi` Instruction	| Sets GIF					|
| `clgi` Instruction	| Clears GIF				|
| `vmrun` Instruction	| Depends on `vGIF` in VMCB	|
| VM-Exit				| Clears GIF				|

## Virtualize Nested Paging
To virtualize NPT, we should merge the page tables. However, I don't have an algorithm regarding page-table merging. So, the SVM-nesting feature in future NoirVisor may not support NPT unless I have one. <br>
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
AMD64 Architecture Manual stated a processor feature: VMCB State Caching. <br>
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

## Devirtualize on-the-fly
Since NoirVisor is possible to be unloaded before the nested hypervisor ends, it is necessary to put nested hypervisor directly onto the processor - that is, L1 becomes L0 and L2 becomes L1. <br>
This is a suggestion for Nested Virtualization in NoirVisor. By now, algorithm design will not be made. After the Nested Virtualization feature completes, NoirVisor may refuse unloading as long as the nested hypervisor did not leave. <br>

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
The only condition to switch the processor context from the host to the guest is the host's hypercall to request runnning a guest vCPU. This hypercall specifies the target vCPU so that NoirVisor would switch to a correct vCPU. <br>
Prior to running a vCPU, NoirVisor should save the processor states. The states subject to be manually saved by NoirVisor includes:
- General-Purpose Registers. (Excludes `rax`,`rsp`,`rip`,`rflags` because they are saved in VMCB)
- Debug Registers. (Excludes `dr6` and `dr7` because they are saved in VMCB)
- x87 Floating-Point Registers. (We might use `fxsave` instruction to save x87 state)
- SSE/AVX Multimedia Registers. (Includes `xmm0`-`xmm15`,`ymm0`-`ymm15`,etc.)

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
		u32 cr_valid:1;		// Includes cr0,cr3,cr4,efer.
		u32 cr2valid:1;		// Includes cr2.
		u32 dr_valid:1;		// Includes dr6,dr7.
		u32 sr_valid:1;		// Includes cs,ds,es,ss.
		u32 fg_valid:1;		// Includes fs,gs,kgsbase.
		u32 dt_valid:1;		// Includes idtr,gdtr.
		u32 lt_valid:1;		// Includes tr,ldtr.
		u32 sc_valid:1;		// Includes star,lstar,cstar,sfmask.
		u32 se_valid:1;		// Includes esp,eip,cs for sysenter.
		u32 xc_valid:1;		// Includes xcr0.
		u32 tp_valid:1;		// Includes cr8.tpr.
		u32 reserved:21;
	};
	u32 value;
}noir_cvm_vcpu_state_cache,*noir_cvm_vcpu_state_cache_p;
```

If the host changed some of the state of vCPU, its corresponding bit should be cleared so that NoirVisor would copy these content to the VMCB.

## World Switch - Guest to Host
The condition to switch the processor context from the guest to the host is certain VM-Exits from the guest. Not all VM-Exits require a world switch. VM-Exits that require a world switch includes:
- External Interrupts. (Includes `physical INTR`, `NMI`, and `SMI`)
- I/O Instruction. (Includes `in`, `rep outsb` instructions, etc.)
- Processor Halt. (The `hlt` instruction)
- Shutdown Condition. (Includes Triple-Fault, etc.)

Only vCPU states not saved into VMCB should be saved for Guest and loaded for Host.

## Interception Settings
Certain interceptions must be set to ensure correct functionality of NoirVisor CVM.

### Interrupt Interception
In that Guests of CVM are not supposed to handle external interrupts from real hardwares, any external interrupts must be intercepted. We may set interceptions upon `physical INTR`, `NMI` and `SMI`. We do not have to intercept `virtual INTR` because they are subject to be injected by hypervisor. Interceptions upon `virtual INTR` may induce dead loop. <br>
Upon interception, perform a world switch. Specify the reason of interception to be the scheduler's exit. For such reason of exit, because `GIF` becomes set when NoirVisor executes `vmrun` instruction in order to switch to host, interrupts will be handled by the Host.

### CPUID Interception
The `cpuid` instruction must be intercepted by NoirVisor, even though host may choose not to intercept it. If the host chooses not to intercept `cpuid` instruction, NoirVisor would handle the `cpuid` instruction itself. Otherwise, switch the world to the host so that host will be handling Guest's `cpuid` instruction.

### I/O Interception
I/O Interceptions are mandatory in that I/O operations in Guest are not supposed to go to real hardwares. Instead, they should go to virtual appliances, which Host is supposed to emulate. Switch the world to the host so that host will be handling Guest's I/O.

### MSR Interception
Like the `cpuid` instruction, Host can choose whether to intercept this instruction or not. If the host does not intercept MSR-Related instructions, NoirVisor would handle them and masking certain operations (e.g: accessing the `EFER` MSR). Otherwise, NoirVisor would switch to the Host to handle the instructions.

### Shutdown Interception
Usually, shutdown is induced by triple-fault. NoirVisor won't handle this interception itself. Switch to the host so the interception is transfered to the Host.

### SVM Interception
Currently, NoirVisor does not support a hypervisor to be nested inside a CVM. Simply inject a `#UD` exception to guest if SVM instructions are intercepted. Do not switch to the host to indicate the guest is attempting to execute SVM instructions.

### Exception Interception
Exception interception is optional: it is only intercepted if the host specifies so. The `#SX` is an exception to this rule: it must be intercepted in that this means an `INIT` signal has arrived. Unless the guest sets `R_INIT` bit in `VM_CR` MSR, `#SX` should neither be injected to guest nor be transfered to the host.

### Nested Page Fault
Nested Page Fault, usually abbreviated as `#NPF`, indicates that a wrong physical address is accessed. NoirVisor would not handle `#NPF` itself, but transfer the interception to the host. In addition, information like fetched instruction bytes, access information, etc., would be recorded.