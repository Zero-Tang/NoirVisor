; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2026, Zero Tang. All rights reserved.
;
; This file is a header file of NoirVisor assembly part.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/msvc/noirhv.inc

ifdef _amd64

ktrap_frame_size		equ 190h
mach_frame_size			equ 30h
gpr_stack_size			equ 80h
volatile_gpr_stack_size	equ 40h

; Macro for NMI-blocking iret.
nmiret macro

	; Emulate the iret instruction in order not to unblock NMIs.
	push rax					; Save the rax
	; Now the stack layout would be like:
	; qword ptr [rsp+8*0]	rax
	; qword ptr [rsp+8*1]	rip
	; word ptr [rsp+8*2]	cs
	; qword ptr [rsp+8*3]	rflags
	; qword ptr [rsp+8*4]	rsp
	; qword ptr [rsp+8*5]	ss
	mov rax,rsp					; Save the NMI stack pointer.
	lss rsp,[rax+8*4]			; Restore ss and rsp
	; Note that rsp is switched to pre-NMI stack, so use rax for memory addressing.
	push qword ptr [rax+18h]	; Store rflags to pre-NMI stack.
	popf						; Restore rflags
	push qword ptr [rax+10h]	; Store cs to pre-NMI stack.
	push qword ptr [rax+8h]		; Store rip to pre-NMI stack.
	mov rax,qword ptr [rax]		; Restore rax
	retf						; Restore cs and rip

endm

; Macro for pushing volatile GPRs to stack.
pushvq_fast macro rsp_offset

	mov qword ptr [rsp+rsp_offset+00h],rax
	.pushreg rax
	mov qword ptr [rsp+rsp_offset+08h],rcx
	.pushreg rcx
	mov qword ptr [rsp+rsp_offset+10h],rdx
	.pushreg rdx
	mov qword ptr [rsp+rsp_offset+18h],rbx
	.pushreg rbx
	.allocstack 32
	mov qword ptr [rsp+rsp_offset+40h],r8
	.pushreg r8
	mov qword ptr [rsp+rsp_offset+48h],r9
	.pushreg r9
	mov qword ptr [rsp+rsp_offset+50h],r10
	.pushreg r10
	mov qword ptr [rsp+rsp_offset+58h],r11
	.pushreg r11
	.allocstack 32
endm

; Macro for pushing all GPRs to stack.
pushaq_fast macro rsp_offset
	
	mov qword ptr [rsp+rsp_offset+00h],rax
	.pushreg rax
	mov qword ptr [rsp+rsp_offset+08h],rcx
	.pushreg rcx
	mov qword ptr [rsp+rsp_offset+10h],rdx
	.pushreg rdx
	mov qword ptr [rsp+rsp_offset+18h],rbx
	.pushreg rbx
	.allocstack 8
	mov qword ptr [rsp+rsp_offset+28h],rbp
	.pushreg rbp
	mov qword ptr [rsp+rsp_offset+30h],rsi
	.pushreg rsi
	mov qword ptr [rsp+rsp_offset+38h],rdi
	.pushreg rdi
	mov qword ptr [rsp+rsp_offset+40h],r8
	.pushreg r8
	mov qword ptr [rsp+rsp_offset+48h],r9
	.pushreg r9
	mov qword ptr [rsp+rsp_offset+50h],r10
	.pushreg r10
	mov qword ptr [rsp+rsp_offset+58h],r11
	.pushreg r11
	mov qword ptr [rsp+rsp_offset+60h],r12
	.pushreg r12
	mov qword ptr [rsp+rsp_offset+68h],r13
	.pushreg r13
	mov qword ptr [rsp+rsp_offset+70h],r14
	.pushreg r14
	mov qword ptr [rsp+rsp_offset+78h],r15
	.pushreg r15
	
endm

pushaq macro
	
	sub rsp,gpr_stack_size
	pushaq_fast 0h
	
endm

; Macro for popping all GPRs from stack.
popaq_fast macro rsp_offset

	mov rax,qword ptr [rsp+rsp_offset]
	mov rcx,qword ptr [rsp+rsp_offset+8]
	mov rdx,qword ptr [rsp+rsp_offset+10h]
	mov rbx,qword ptr [rsp+rsp_offset+18h]
	mov rbp,qword ptr [rsp+rsp_offset+28h]
	mov rsi,qword ptr [rsp+rsp_offset+30h]
	mov rdi,qword ptr [rsp+rsp_offset+38h]
	mov r8, qword ptr [rsp+rsp_offset+40h]
	mov r9, qword ptr [rsp+rsp_offset+48h]
	mov r10,qword ptr [rsp+rsp_offset+50h]
	mov r11,qword ptr [rsp+rsp_offset+58h]
	mov r12,qword ptr [rsp+rsp_offset+60h]
	mov r13,qword ptr [rsp+rsp_offset+68h]
	mov r14,qword ptr [rsp+rsp_offset+70h]
	mov r15,qword ptr [rsp+rsp_offset+78h]

endm


popaq macro

	popaq_fast 0h
	add rsp,gpr_stack_size

endm

pushax_volatile_fast macro rsp_offset

	.allocstack 70h
	movaps xmmword ptr [rsp+rsp_offset+00h],xmm0
	movaps xmmword ptr [rsp+rsp_offset+10h],xmm1
	movaps xmmword ptr [rsp+rsp_offset+20h],xmm2
	movaps xmmword ptr [rsp+rsp_offset+30h],xmm3
	movaps xmmword ptr [rsp+rsp_offset+40h],xmm4
	movaps xmmword ptr [rsp+rsp_offset+50h],xmm5
	stmxcsr dword ptr [rsp+rsp_offset+60h]

endm

popax_volatile_fast macro rsp_offset

	movaps xmm0,xmmword ptr [rsp+rsp_offset+00h]
	movaps xmm1,xmmword ptr [rsp+rsp_offset+10h]
	movaps xmm2,xmmword ptr [rsp+rsp_offset+20h]
	movaps xmm3,xmmword ptr [rsp+rsp_offset+30h]
	movaps xmm4,xmmword ptr [rsp+rsp_offset+40h]
	movaps xmm5,xmmword ptr [rsp+rsp_offset+50h]
	ldmxcsr dword ptr [rsp+rsp_offset+60h]

endm

else

endif