# Shadow-Paging
Shadow paging is an algorithm used to virtualize EPT/NPT in nested virtualization.

## Principles of Shadow Paging
This chapter discusses the principle of shadow paging.

### Shadow Page Table
Shadow Page Table is the actual table that will be fed to the VMCS/VMCB on VM-Entry. It is derived from virtualizing the guest's EPT/NPT.

### Interceptions
Any updates to TLB that involves GPA->HPA translations must be intercepted.

- The `invlpga` and `invlpgb` instructions are AMD-V exclusive..
- The `invvpid` instruction is Intel VT-x exclusive.
- Setting the `TLB Control` field in VMCB.

### Shadowing the Translation
We'll define the following terms:

- NGPA: Physical Address referenced by the Nested Guest.
- GPA: Physical Address referenced by the Guest.
- HPA: Physical Address corresponding to the GPA/NGPA in Host.

In traditional memory virtualization (i.e.: when EPT/NPT was not invented yet), GVA is directly translated into HPA by shadowing the supervisor's MMU (specified in Guest `CR3`). \
We will follow this idea to virtualize EPT/NPT. In other words, NGPA will be directly translated into HPA by shadowing the `EPTP` or `NCR3`.