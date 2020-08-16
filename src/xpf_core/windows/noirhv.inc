; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2020, Zero Tang. All rights reserved.
;
; This file is a header file of NoirVisor assembly part.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/noirhv.inc

ifdef _amd64

; Macro for pushing all GPRs to stack.
pushaq macro
	
	sub rsp,80h
	mov qword ptr [rsp+00h],rax
	mov qword ptr [rsp+08h],rcx
	mov qword ptr [rsp+10h],rdx
	mov qword ptr [rsp+18h],rbx
	mov qword ptr [rsp+28h],rbp
	mov qword ptr [rsp+30h],rsi
	mov qword ptr [rsp+38h],rdi
	mov qword ptr [rsp+40h],r8
	mov qword ptr [rsp+48h],r9
	mov qword ptr [rsp+50h],r10
	mov qword ptr [rsp+58h],r11
	mov qword ptr [rsp+60h],r12
	mov qword ptr [rsp+68h],r13
	mov qword ptr [rsp+70h],r14
	mov qword ptr [rsp+78h],r15
	
endm

; Macro for popping all GPRs from stack.
popaq macro

	mov rax,qword ptr [rsp]
	mov rcx,qword ptr [rsp+8]
	mov rdx,qword ptr [rsp+10h]
	mov rbx,qword ptr [rsp+18h]
	mov rbp,qword ptr [rsp+28h]
	mov rsi,qword ptr [rsp+30h]
	mov rdi,qword ptr [rsp+38h]
	mov r8, qword ptr [rsp+40h]
	mov r9, qword ptr [rsp+48h]
	mov r10,qword ptr [rsp+50h]
	mov r11,qword ptr [rsp+58h]
	mov r12,qword ptr [rsp+60h]
	mov r13,qword ptr [rsp+68h]
	mov r14,qword ptr [rsp+70h]
	mov r15,qword ptr [rsp+78h]
	add rsp,80h

endm

; Macro for pushing all XMM registers onto stack
pushax macro

	sub rsp,100h
	movaps xmmword ptr [rsp+00h],xmm0
	movaps xmmword ptr [rsp+10h],xmm1
	movaps xmmword ptr [rsp+20h],xmm2
	movaps xmmword ptr [rsp+30h],xmm3
	movaps xmmword ptr [rsp+40h],xmm4
	movaps xmmword ptr [rsp+50h],xmm5
	movaps xmmword ptr [rsp+60h],xmm6
	movaps xmmword ptr [rsp+70h],xmm7
	movaps xmmword ptr [rsp+80h],xmm8
	movaps xmmword ptr [rsp+90h],xmm9
	movaps xmmword ptr [rsp+0A0h],xmm10
	movaps xmmword ptr [rsp+0B0h],xmm11
	movaps xmmword ptr [rsp+0C0h],xmm12
	movaps xmmword ptr [rsp+0D0h],xmm13
	movaps xmmword ptr [rsp+0E0h],xmm14
	movaps xmmword ptr [rsp+0F0h],xmm15

endm

; Macro for popping all XMM registers from stack
popax macro

	movaps xmm0,xmmword ptr [rsp+00h]
	movaps xmm1,xmmword ptr [rsp+10h]
	movaps xmm2,xmmword ptr [rsp+20h]
	movaps xmm3,xmmword ptr [rsp+30h]
	movaps xmm4,xmmword ptr [rsp+40h]
	movaps xmm5,xmmword ptr [rsp+50h]
	movaps xmm6,xmmword ptr [rsp+60h]
	movaps xmm7,xmmword ptr [rsp+70h]
	movaps xmm8,xmmword ptr [rsp+80h]
	movaps xmm9,xmmword ptr [rsp+90h]
	movaps xmm10,xmmword ptr [rsp+0A0h]
	movaps xmm11,xmmword ptr [rsp+0B0h]
	movaps xmm12,xmmword ptr [rsp+0C0h]
	movaps xmm13,xmmword ptr [rsp+0D0h]
	movaps xmm14,xmmword ptr [rsp+0E0h]
	movaps xmm15,xmmword ptr [rsp+0F0h]
	add rsp,100h

endm

else

; Macro for pushing all XMM registers onto stack
pushax macro

	sub esp,80h
	movaps xmmword ptr [esp+00h],xmm0
	movaps xmmword ptr [esp+10h],xmm1
	movaps xmmword ptr [esp+20h],xmm2
	movaps xmmword ptr [esp+30h],xmm3
	movaps xmmword ptr [esp+40h],xmm4
	movaps xmmword ptr [esp+50h],xmm5
	movaps xmmword ptr [esp+60h],xmm6
	movaps xmmword ptr [esp+70h],xmm7

endm

; Macro for popping all XMM registers from stack
popax macro

	movaps xmm0,xmmword ptr [esp+00h]
	movaps xmm1,xmmword ptr [esp+10h]
	movaps xmm2,xmmword ptr [esp+20h]
	movaps xmm3,xmmword ptr [esp+30h]
	movaps xmm4,xmmword ptr [esp+40h]
	movaps xmm5,xmmword ptr [esp+50h]
	movaps xmm6,xmmword ptr [esp+60h]
	movaps xmm7,xmmword ptr [esp+70h]
	add esp,80h

endm

endif