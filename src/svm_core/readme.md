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

In summary, four #NPF will occur in a single execution of hooked function. In comparison, Intel EPT offers flexibility that intensive access can still have least performance penalty.

# Critical Hypervisor Protection
This feature is an essential security feature. I found this feature missing in most open-source light-weight hypervisor project. The key is that VMCB and other essential pages are not protected through Nested Paging even if they enabled AMD NPT. It should be pointed out that a malware is aware of the format of VMCB. In this regard, malware may corrupt the VMCB through memory access instruction.

# Real-Time Code Integrity
Real-Time CI is now implemented by AMD Nested Paging.

# Future Feature (Roadmap)
In future, NoirVisor has following plans:

- Implement SVM-Nesting (This will be a long term project.)
- Implement NPT-based Stealth Inline Hook

# SVM-Nesting Algorithm (Incomplete Version)
To nest another working hypervisor is the highest focus of Project NoirVisor. However, starting from the repository creation, this goal has not been satisfied yet. Here, I will state down the algorithm and it will be updated in future as problems arises. <br>
We will divide the problem into several sub-problems, as stated below:

- Host SVM-Related MSR and CPUID Virtualization
- Virtualize SVM Instructions
- Virtualize VM-Entry
- Virtualize VM-Exit
- Virtualize ASID
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
Also, we should intercept access to SVM_HSAVE_PA MSR. <br>

## Virtualize SVM Instructions
There are eight SVM instructions: vmrun, vmload, vmsave, vmmcall, clgi, stgi, invlpga, skinit. Instructions should be virtualized accordingly. <br>
In essence, vmrun instruction takes the place of VM-Entry and VM-Exit. We will leave this to special chapters - Virtualize VM-Entry and Virtualize VM-Exit. <br>
For vmload and vmsave instructions, since they can be accelerated by processor's built-in acceleration feature, we will leave this to special chapter - Utilize Accelerated Nested Virtualization. <br>
For clgi and stgi instructions, since they can be accelerated by processor's built-in acceleration feature, we will leave this to special chapter - Utilize Accelerated Nested Virtualization. <br>
For invlpga instruction, we pass parameters to invalidate specific TLB entry in Nested Guest. <br>
For skinit instruction, this is somewhat optional since it is rarely used.

## Virtualize VM-Entry
Basically, this is intercepting the vmrun instructions. This instruction loads VMCB by address saved in rax register. <br>
Then, we follow the steps indicated in AMD64 architecture programming manual. Details will be omitted here. The steps are:

- Saving Host State. We save host state to the address indicated by SVM_HSAVE_PA MSR.
- Validate and Load Guest State. We should check consistency of the Guest State from VMCB. If no inconsistency is found, load the Guest State to L2 VMCB. Otherwise, fails the consistency check and issue VM-Exit.
- Load Control State. Again, check inconsistency check, like ASID and VMRUN interception. If no inconsistency is found, load Control State to L2 VMCB. Otherwise, fails the consistency check and issue VM-Exit.
- Finally, execute vmrun instruction with address of L2 VMCB, concluding the VM-Entry.

More details will be included in chapter Virtualize L2 VMCB.

## Virtualize VM-Exit
Steps are given in the following:

- On interceptions, we copy guest state to the VMCB given by the Nested Hypervisor.
- Load the address of VMCB as rax register to L1 VMCB.
- Load L1 Host State as Guest State to L1 VMCB.
- Load Control State. Due to Decode Assist and Next-RIP-Saving features, we need to set something in VMCB Control State, such as Exit Reason, Information, Next RIP, etc.
- Finally, execute vmrun instruction with address of L1 VMCB, concluding the VM-Exit.

## Virtualize ASID
In NoirVisor, the L0 context use ASID=0, and L1 context use ASID=1. To virtualize ASID, we first we should use ASID>1 for any L2 context. For a simple algorithm, we increment ASID by 1 to virtualize it. If L2 ASID<2, then the VMCB is inconsistent.

## Virtualize Nested Paging
To virtualize NPT, we should merge the page tables. However, I don't have an algorithm regarding page-table merging. So, the SVM-nesting feature in future NoirVisor may not support NPT unless I have one.

## Utilize Accelerated Nested Virtualization
AMD64 architecture manual specifies Nested Virtualization Acceleration. To improve performance, we may utilize nested virtualization acceleration feature. The processor may help us to accelerate vmload and vmsave instructions and Nested GIF.

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

### L2 VMCB Optimization: <b> State Caching </b>
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

## Devirtualize on-the-fly
Since NoirVisor is possible to be unloaded before the nested hypervisor ends, it is necessary to put nested hypervisor directly onto the processor - that is, L1 becomes L0 and L2 becomes L1. <br>
This is a suggestion for Nested Virtualization in NoirVisor. By now, algorithm design will not be made. After the Nested Virtualization feature completes, NoirVisor may refuse unloading as long as the nested hypervisor did not leave. <br>