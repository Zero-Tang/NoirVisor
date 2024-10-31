# NoirVisor CVM Support for x86-S Architecture
In April 2023, Intel released a [proposal of x86-S architecture](https://www.intel.com/content/www/us/en/developer/articles/technical/envisioning-future-simplified-architecture.html) that will dispose compatibility support for legacy systems. This document serves as the draft of making NoirVisor CVM emulate a legacy x86 processor in future x86-S processor.

## Changes in x86-S ISA
According to Intel's proposal, following changes are made:

- CPU is always in paged mode.
- 32-bit Ring0 and v8086 mode are removed.
- Ring1 and Ring2 modes are removed.
- 16-bit mode is removed.
- 16-bit addressing is removed.
- Fixed MTRRs are removed.
- User-level I/O and String I/O are removed.
- CR0 Write-Through mode is removed.
- Legacy FPU control bits in CR0 are removed.
- Ring3 interrupt flag control is removed.
- Obsolete CR access (`clts`, `smsw` and `lmsw`) instrtuctions are removed. They will trigger `#UD` exception.
- `#SS` and `#NP` exceptions are removed and will be replaced with `#GP`. But they can still be injected.
- The SIPI is rearchitected to a new 64-bit SIPI mechanism.
- New mechanism of switching 4-level and 5-level paging is introduced.
- xAPIC is removed. x2APIC must be used instead.
- 8259 support is removed.
- `NX`, `SCE`, and `LME`/`LMA` bits must be set in `EFER` MSR.
- `#SS` and `#NP` exceptions are removed.
- Segmentation architecture is reduced.
	- Limits are removed for `cs`, `ds`, `es`, `fs`, `gs` and `ss`.
	- Bases are removed for `cs`, `ds`, `es`, and `ss`.
	- Far `jmp`, `call` and `ret` can no longer switch privileges.
	- The `busy` bit in TSS is ignored.