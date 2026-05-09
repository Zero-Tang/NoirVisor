// NoirVisor - Hardware-Accelerated Hypervisor solution
//
// Copyright (c) Zero Tang, 2018-2026. All rights reserved.
//
// This file is a header file of NoirVisor assembly part.
//
// This program is distributed in the hope that it will be successful, but
// without any warranty (no matter implied warranty of merchantability or
// fitness for a particular purpose, etc.).

// If optimizer is enabled, all files referenced in `global_asm!` may be merged together.
// Therefore, use traditional method to make sure this header file is included for only once.
.ifndef HELPER_INC
HELPER_INC=1

// Special macro that returns from NMI without unblocking it.
.macro nmiret
	push rax
	// Now, the stack layout is:
	// qword ptr [rsp+8*0]: rax
	// qword ptr [rsp+8*1]: rip
	//  word ptr [rsp+8*2]: cs
	// qword ptr [rsp+8*3]: rflags
	// qword ptr [rsp+8*4]: rsp
	//  word ptr [rsp+8*5]: ss
	mov rax,rsp					// Save the NMI stack pointer.
	lss rsp,[rax+8*4]			// Restore ss:rsp
	push qword ptr [rax+0x18]	// Store rflags to pre-NMI stack
	popfq						// Restore rflags
	push qword ptr [rax+0x10]	// Store cs to pre-NMI stack.
	push qword ptr [rax+0x8]	// Store rip to pre-NMI stack.
	mov rax,qword ptr [rax]		// Restore rax.
	retfq						// Restore cs and rip.
.endm

// Saves all GPRs onto the stack.
.macro pushaq_fast rsp_offset
	mov qword ptr [rsp+\rsp_offset],rax
	.seh_savereg rax,\rsp_offset+0x00
	mov qword ptr [rsp+\rsp_offset+8],rcx
	.seh_savereg rcx,\rsp_offset+0x08
	mov qword ptr [rsp+\rsp_offset+0x10],rdx
	.seh_savereg rdx,\rsp_offset+0x10
	mov qword ptr [rsp+\rsp_offset+0x18],rbx
	.seh_savereg rbx,\rsp_offset+0x18
	mov qword ptr [rsp+\rsp_offset+0x28],rbp
	.seh_savereg rbp,\rsp_offset+0x20
	mov qword ptr [rsp+\rsp_offset+0x30],rsi
	.seh_savereg rsi,\rsp_offset+0x30
	mov qword ptr [rsp+\rsp_offset+0x38],rdi
	.seh_savereg rdi,\rsp_offset+0x38
	mov qword ptr [rsp+\rsp_offset+0x40],r8
	.seh_savereg r8,\rsp_offset+0x40
	mov qword ptr [rsp+\rsp_offset+0x48],r9
	.seh_savereg r9,\rsp_offset+0x48
	mov qword ptr [rsp+\rsp_offset+0x50],r10
	.seh_savereg r10,\rsp_offset+0x50
	mov qword ptr [rsp+\rsp_offset+0x58],r11
	.seh_savereg r11,\rsp_offset+0x58
	mov qword ptr [rsp+\rsp_offset+0x60],r12
	.seh_savereg r12,\rsp_offset+0x60
	mov qword ptr [rsp+\rsp_offset+0x68],r13
	.seh_savereg r13,\rsp_offset+0x68
	mov qword ptr [rsp+\rsp_offset+0x70],r14
	.seh_savereg r14,\rsp_offset+0x70
	mov qword ptr [rsp+\rsp_offset+0x78],r15
	.seh_savereg r15,\rsp_offset+0x78
.endm

// Restores all GPRs from the stack.
.macro popaq_fast rsp_offset
	mov rax,qword ptr [rsp+\rsp_offset]
	mov rcx,qword ptr [rsp+\rsp_offset+8]
	mov rdx,qword ptr [rsp+\rsp_offset+0x10]
	mov rbx,qword ptr [rsp+\rsp_offset+0x18]
	mov rbp,qword ptr [rsp+\rsp_offset+0x28]
	mov rsi,qword ptr [rsp+\rsp_offset+0x30]
	mov rdi,qword ptr [rsp+\rsp_offset+0x38]
	mov r8, qword ptr [rsp+\rsp_offset+0x40]
	mov r9, qword ptr [rsp+\rsp_offset+0x48]
	mov r10,qword ptr [rsp+\rsp_offset+0x50]
	mov r11,qword ptr [rsp+\rsp_offset+0x58]
	mov r12,qword ptr [rsp+\rsp_offset+0x60]
	mov r13,qword ptr [rsp+\rsp_offset+0x68]
	mov r14,qword ptr [rsp+\rsp_offset+0x70]
	mov r15,qword ptr [rsp+\rsp_offset+0x78]
.endm

.macro save_volatile_xmm xstate
	stmxcsr dword ptr [\xstate+0x18]
	movaps xmmword ptr [\xstate+0xA0],xmm0
	movaps xmmword ptr [\xstate+0xB0],xmm1
	movaps xmmword ptr [\xstate+0xC0],xmm2
	movaps xmmword ptr [\xstate+0xD0],xmm3
	movaps xmmword ptr [\xstate+0xE0],xmm4
	movaps xmmword ptr [\xstate+0xF0],xmm5
.endm

.macro restore_volatile_xmm xstate
	movaps xmm0,xmmword ptr [\xstate+0xA0]
	movaps xmm1,xmmword ptr [\xstate+0xB0]
	movaps xmm2,xmmword ptr [\xstate+0xC0]
	movaps xmm3,xmmword ptr [\xstate+0xD0]
	movaps xmm4,xmmword ptr [\xstate+0xE0]
	movaps xmm5,xmmword ptr [\xstate+0xF0]
	ldmxcsr dword ptr [\xstate+0x18]
.endm

.endif