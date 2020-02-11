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

# CPUID Cache
This feature could enhance the performance of CPUID-induced VM-Exit on Nested-VMM scenario. (e.g NoirVisor running in a Virtual Machine.) It might slightly lower down the performance on Non-Nested scenario, but generally should have a good performance no matter the scenario. <br>
The remastered design is to handle different leaf functions with multiple functions. In this case, the structure of storing the cpuid cache will be leaf-specific.

# Critical Hypervisor Protection
This feature is an essential security feature. I found this feature missing in most open-source light-weight hypervisor project. The key is that VMCB and other essential pages are not protected through Nested Paging even if they enabled AMD NPT. It should be pointed out that a malware is aware of the format of VMCB. In this regard, malware may corrupt the VMCB through memory access instruction.

# Real-Time Code Integrity
Real-Time CI is now implemented by AMD Nested Paging.

# Future Feature (Roadmap)
In future, NoirVisor has following plans:

- Remaster CPUID-caching architecture with flexible design
- Implement SVM-Nesting (This will be a long term project.)
- Implement NPT-based Stealth Inline Hook