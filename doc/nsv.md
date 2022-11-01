# NoirVisor Secure Virtualization
This document provides the specification of NoirVisor Secure Virtualization (NSV). This is the preliminary draft version of document.

## General Architecture
NoirVisor's Customizable Virtual Machine feature allows a guest program to be running in a protected environment. In NSV, NoirVisor guarantees the confidentiality by restricting the host from accessing the guest.

It is important to note that NSV therefore *represents a departure from the standard CVM programming model*, as the user hypervisor is no longer able to inspect or alter all guest code or data. Memories managed by guest can be marked as either private or shared through a special hypercall which only NoirVisor can receive.

Not only is the user hypervisor required to be conformant to the standard, but the NSV guests are also required to fit itself to the standard. The essential idea of NSV is borrowed from AMD-V's SEV-ES and SEV-SNP, so a guest ready for SEV should fit to NSV without too much special modifications. (e.g.: concept of AE, usage of GHCB, handler of `#VC` exceptions, etc.)

### User Hypervisor: Set Mapping
When a user hypervisor sets memory mapping for the secured guest, they are initially insecure, indicating that user hypervisor still has accesses to these memories. Guest may issue a hypercall to claim the memory as secure so that user hypervisor will lose access. \
User hypervisor can unmap the memory mapping to revoke its secure state. After the memory is unmapped, content of secure pages would be encrypted so that memory contents won't be leaked.

### User Hypervisor: Guest-Host Communication Block
The Guest-Host Communication Block (GHCB) is an explicitly insecure 4KiB page that guest and host will be working with. The layout of GHCB is user-defined, meaning that the format of GHCB can be defined in any ways that both NSV guest and user hypervisor can recognize.

### User Hypervisor: Automatic Exit
Automatic Exit (AE) is a VM-Exit that user hypervisor is allowed to handle. Non-Automatic Exit (NAE) is a VM-Exit that user hypervisor is forbidden to handle. This design is borrowed from SEV-ES.

#### AE List
Following VM-Exits are defined as Automatic Exits:

| Name | Notes | NoirVisor advances `rip` register |
|---|---|---|
| `pause` instruction | Guest indicates a spin-lock. | Yes |
| `hlt` instruction | Guest halts the vCPU. | Yes |
| Shutdown condition | Guest triggers a triple-fault. | No |
| Memory Access | Guest accesses insecure memory with fault. | No |
| Invalid State | Guest vCPU state is invalid. | No |
| Explicit Hypercall | Guest issues an explicit hypercall to user hypervisor. | Yes |

All other VM-Exits are classified as Non-Automatic Exits (NAEs).

#### Converting NAE to AE
If guest triggers an NAE (e.g.: Guest executed `cpuid` instruction while user hypervisor wants to intercept it), an exception will be thrown into the guest, notifying that user hypervisor wants to intercept this instruction. \
Current implementation of NSV will throw VMM Communication (`#VC`, vector 28) exceptions for such VM-Exits. \
Interceptions toward the `#VC` exception by user hypervisor is ignored by NoirVisor.

If guest wishes such NAE to be handled by user hypervisor, guest should issue an explicit hypercall to the host using `vmcall` or `vmmcall` instructions with a `rep` prefix in its `#VC` handler. \
For example, user hypervisor wants to intercept the `cpuid` instruction, when NSV guest executes a `cpuid` instruction, a `#VC` exception will be thrown into the NSV guest. NSV guest would be aware that it is executing `cpuid` instruction, so if NSV guest wishes to let the user hypervisor handles `cpuid`, it should fill the GHCB, which is a block of insecure memory, with its `eax` and `ecx` register value and reserve a space in GHCB to let the user hypervisor fill the `eax`, `ebx`, `ecx` and `edx` values.

```Mermaid
sequenceDiagram;
    participant NsvGuestUser;
    participant NsvGuestKernel;
    participant NoirVisor;
    participant UserHypervisor;
    NsvGuestUser->>NoirVisor: Triggers VM-Exit by executing cpuid instruction;
    NoirVisor->>NsvGuestKernel: Inject VMM-Communication Exception;
    Note left of NsvGuestKernel: Fill GHCB with eax and ecx value;
    NsvGuestKernel->>NoirVisor: Explicitly hypercall;
    NoirVisor->>UserHypervisor: Automatic Exit;
    Note right of UserHypervisor: Fill GHCB with eax, ebx, ecx and edx value;
    UserHypervisor->>NoirVisor: Run NsvGuest vCPU;
    NoirVisor->>NsvGuestKernel: Run vCPU;
    Note left of NsvGuestKernel: Write to eax, ebx, ecx, edx registers according to GHCB;
    NsvGuestKernel->>NsvGuestUser: Execute iret instruction to end exception handling;
```

### User Hypervisor: Trusted Boot
Albeit NoirVisor advocates for designing customizable virtual machine platforms, trusted image is required for booting Secure Guests. \
Currently, specification of Trusted Boot is unavailable.

## NoirVisor Secure Hypervisor Interfaces
This chapter describes the NoirVisor Secure Hypervisor Interfaces (NoirVisor SeHvI).

### Feature Discovery
Guest running on NoirVisor CVM may query NoirVisor's functionality by executing `cpuid` instruction with certain leaves:

**Input `EAX=0x4000_0000`: Hypervisor Identification**

| Output Register | Description |
|---|---|
| `eax` | Maximum Value of Hypervisor `cpuid` leaves. |
| `ebx` | Hypervisor Vendor ID Signature. (Bytes 0-3) |
| `ecx` | Hypervisor Vendor ID Signature. (Bytes 4-7) |
| `edx` | Hypervisor Vendor ID Signature. (Bytes 8-11) |

For NoirVisor, when `ebx`, `ecx` and `edx` registers are forged into a string, it should be an ANSI string: `NoirVisor ZT`.

**Input `EAX=0x4000_0001`: Hypervisor Vendor-Neutral Identification**

| Output Register | Description |
|---|---|
| `eax` | Hypervisor Interface Signature. - 0x3123764E "Nv#1" |
| `ebx` | Reserved. |
| `ecx` | Reserved. |
| `edx` | Reserved. |

**Input `EAX=0x4000_0002`: Hypervisor System Identity**

The specification is not available yet.

**Input `EAX=0x4000_0003`: Hypervisor Feature Identification**

<table>
	<thead>
		<tr>
			<th>Output Register</th>
			<th>Bits</th>
			<th>Description</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td rowspan=4><code>eax</code></td>
			<td>31-3</td>
			<td>Reserved</td>
		</tr>
		<tr>
			<td>2</td>
			<td>NPIEP is supported</td>
		</tr>
		<tr>
			<td>1</td>
			<td>Secure Synthetic Timer is supported</td>
		</tr>
		<tr>
			<td>0</td>
			<td>Secure Inter-Processor Interrupt is supported</td>
		</tr>
		<tr>
			<td><code>ebx</code></td>
			<td>31-0</td>
			<td>Reserved</td>
		</tr>
		<tr>
			<td><code>ecx</code></td>
			<td>31-0</td>
			<td>Reserved</td>
		</tr>
		<tr>
			<td><code>edx</code></td>
			<td>31-0</td>
			<td>Reserved</td>
		</tr>
	</tbody>
</table>

### Synthetic Model-Specific Registers
Guest running on NoirVisor CVM as NSV may access Synthetic MSRs. \
The following is a table of Synthetic MSRs defined by NoirVisor for NSV guests.

| MSR Index | MSR Name | Access | Affinity | Description |
|---|---|---|---|---|
| 0x4000_0000 | nsv_msr_guest_os_id | R/W | VM-wide | Reports the guest OS in NSV. |
| 0x4000_0001 | nsv_msr_ghcb | R/W | per-vCPU | Specifies the GPA of Guest-Host Communication Block. |
| 0x4000_0002 | nsv_msr_vcpu_index | RO | per-vCPU | Specifies the index of vCPU. |
| 0x4000_0040 | nsv_msr_npiep_config | R/W | per-vCPU | Configures the [Non-Privileged Instruction Execution Prevention](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/vp-properties#non-privileged-instruction-execution-prevention-npiep) feature. |
| 0x4000_0070 | nsv_msr_eoi | WO | per-vCPU | Issues an End-of-Interrupt signal. |
| 0x4000_0071 | nsv_msr_icr | R/W | per-vCPU | Issues an IPI. |
| 0x4000_0072 | nsv_msr_tpr | R/W | per-vCPU | Used for accessing CR8 in 32-bit mode. |
| 0x4001_0131 | nsv_msr_active_status | RO | VM-wide | Specifies the active status of NSV. |

#### NSV Active Status
The `nsv_msr_active_status` is a special Synthetic MSR for NSV guest to confirm whether it is running in a confidential environment. In order to prevent impersonation, user hypervisor's interceptions toward this MSR is explicitly ignored even for Non-NSV guests.

| Bits | Description |
|---|---|
| 63:1	| Reserved. |
| 0		| NSV is enabled. |

However, it is still possible to attack NSV guests through impersonation by using other hypervisor platforms like WHP. Current design lacks an authentication method to verify the hypervisor platform is provided by NoirVisor.

## Reverse Mapping Table (Candidate 1 Design)
The Reverse Mapping Table (RMT) is a global table that records the mapping relationship from HPA to GPA so that host physical pages are guaranteed to be assigned to their unique guest physical pages. This design is borrowed from SEV-SNP. \
Each entry of RMT has 32 bytes of data, describing a continuous set of pages, defined as following:

| Bit Offset | Entry Name | Description |
|---|---|---|
| 255:204	| GPA	| The starting PFN of guest physical pages being mapped from |
| 203:192	| Rsvd	| Reserved |
| 191:140	| HPA	| The starting PFN of host physical pages being mapped to |
| 139:129	| Rsvd	| Reserved |
| 128		| Share	| This page range is shared with others |
| 127:96	| Len	| Number of pages of the mapping range |
| 95:90		| Owner	| The ownership purpose of this host page |
| 89:88		| PS	| Page Size of this entry |
| 31:0		| ASID	| The Address Space Identifier assigned for the page |

The RMT is a sorted array of RMT entries in terms of HPA, and thereby allows binary search to efficiently prevent aliased mapping.

### Ownership Purpose
The `Ownership Purpose` field is a 6-bit field describing the purpose of the page ownership.

| Value | Description |
|---|---|
| 0x00	| Owned by Subverted Host |
| 0x01	| Owned by NoirVisor |
| 0x02	| Assigned to Insecure Guest |
| 0x03	| Assigned to Secure Guest as Private Pages |
| 0x04	| Assigned to Secure Guest as Insecure Memory |
| 0x3F	| Unused entry |

All other values are reserved.

### Page Size
The `Page Size` field is a 2-bit field describing the unit size of pages as described in an entry of RMT.

| Value | Description |
|---|---|
| 0b00	| Unit size of the page is 4KiB |
| 0b01	| Unit size of the page is 2MiB |
| 0b10	| Unit size of the page is 1GiB |
| 0b11	| Reserved |

### Shared Pages
Entries indicating that pages are shared between VMs must have the same HPA, Length and Share fields in the RMT and be adjacent to each other in the array. Therefore, it is easier to track shared pages in Candidate 1 implementation.

## Reverse Mapping Table (Candidate 2 Design)
The Reverse Mapping Table (RMT) is a global table that records the mapping relationship from HPA to GPA so that host physical pages are guaranteed to be assigned to their unique guest physical pages. This design is borrowed from SEV-SNP. \
Each entry of RMT has 16 bytes of data, defined as following:

| Bit Offset | Entry Name | Description |
|---|---|---|
| 127:76	| GPA	| The PFN of the guest physical page being mapped from |
| 75:64		| Rsvd	| Reserved |
| 63:56		| Owner	| The ownership purpose of this host page |
| 55		| Share	| This page is shared with others |
| 54:32		| Rsvd	| Reserved |
| 31:0		| ASID	| The Address Space Identifier or Virtual Processor Identifier assigned for the page |

The RMT is an array indexed by Host PFN and thereby allows constant running time to prevent aliased mapping.

### Ownership Purpose
The `Ownership Purpose` field is a 8-bit field describing the purpose of the page ownership.

| Value | Description |
|---|---|
| 0x00 | Owned by NoirVisor (For this entry, ASID must be zero.) |
| 0x01 | Owned by Subverted Host (For this entry, ASID must be one.) |
| 0x02 | Assigned to Non-NSV Guest |
| 0x03 | Assigned to NSV Guest as Secure Memory (For this entry, the `Shared` field must be reset.) |
| 0x04 | Assigned to NSV Guest as Insecure Memory (For this entry, the `Shared` field must be set.) |

### Shared Pages
Interoperating VMs typically use shared virtual memories. For candidate 2 implementation of RMT, it would be hard to track shared pages efficiently in that only one ASID is recorded. Also, it requires transferring the ownership to other Guests or the Subverted Host.