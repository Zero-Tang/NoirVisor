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

%ifdef _amd64

; Macro for pushing all GPRs to stack.
%macro pushaq 0
	
	sub rsp,80h
	mov qword[rsp+00h],rax
	mov qword[rsp+08h],rcx
	mov qword[rsp+10h],rdx
	mov qword[rsp+18h],rbx
	mov qword[rsp+28h],rbp
	mov qword[rsp+30h],rsi
	mov qword[rsp+38h],rdi
	mov qword[rsp+40h],r8
	mov qword[rsp+48h],r9
	mov qword[rsp+50h],r10
	mov qword[rsp+58h],r11
	mov qword[rsp+60h],r12
	mov qword[rsp+68h],r13
	mov qword[rsp+70h],r14
	mov qword[rsp+78h],r15
	
%endmacro

; Macro for popping all GPRs from stack.
%macro popaq 0

	mov rax,qword[rsp]
	mov rcx,qword[rsp+8]
	mov rdx,qword[rsp+10h]
	mov rbx,qword[rsp+18h]
	mov rbp,qword[rsp+28h]
	mov rsi,qword[rsp+30h]
	mov rdi,qword[rsp+38h]
	mov r8, qword[rsp+40h]
	mov r9, qword[rsp+48h]
	mov r10,qword[rsp+50h]
	mov r11,qword[rsp+58h]
	mov r12,qword[rsp+60h]
	mov r13,qword[rsp+68h]
	mov r14,qword[rsp+70h]
	mov r15,qword[rsp+78h]
	add rsp,80h

%endmacro

; Macro for pushing all XMM registers onto stack
%macro pushax 0

	sub rsp,100h
	movaps xmmword[rsp+00h],xmm0
	movaps xmmword[rsp+10h],xmm1
	movaps xmmword[rsp+20h],xmm2
	movaps xmmword[rsp+30h],xmm3
	movaps xmmword[rsp+40h],xmm4
	movaps xmmword[rsp+50h],xmm5
	movaps xmmword[rsp+60h],xmm6
	movaps xmmword[rsp+70h],xmm7
	movaps xmmword[rsp+80h],xmm8
	movaps xmmword[rsp+90h],xmm9
	movaps xmmword[rsp+0A0h],xmm10
	movaps xmmword[rsp+0B0h],xmm11
	movaps xmmword[rsp+0C0h],xmm12
	movaps xmmword[rsp+0D0h],xmm13
	movaps xmmword[rsp+0E0h],xmm14
	movaps xmmword[rsp+0F0h],xmm15

%endmacro

; Macro for popping all XMM registers from stack
%macro popax 0

	movaps xmm0,xmmword[rsp+00h]
	movaps xmm1,xmmword[rsp+10h]
	movaps xmm2,xmmword[rsp+20h]
	movaps xmm3,xmmword[rsp+30h]
	movaps xmm4,xmmword[rsp+40h]
	movaps xmm5,xmmword[rsp+50h]
	movaps xmm6,xmmword[rsp+60h]
	movaps xmm7,xmmword[rsp+70h]
	movaps xmm8,xmmword[rsp+80h]
	movaps xmm9,xmmword[rsp+90h]
	movaps xmm10,xmmword[rsp+0A0h]
	movaps xmm11,xmmword[rsp+0B0h]
	movaps xmm12,xmmword[rsp+0C0h]
	movaps xmm13,xmmword[rsp+0D0h]
	movaps xmm14,xmmword[rsp+0E0h]
	movaps xmm15,xmmword[rsp+0F0h]
	add rsp,100h

%endmacro

%else

; Macro for pushing all XMM registers onto stack
%macro pushax 0

	sub esp,80h
	movaps xmmword[esp+00h],xmm0
	movaps xmmword[esp+10h],xmm1
	movaps xmmword[esp+20h],xmm2
	movaps xmmword[esp+30h],xmm3
	movaps xmmword[esp+40h],xmm4
	movaps xmmword[esp+50h],xmm5
	movaps xmmword[esp+60h],xmm6
	movaps xmmword[esp+70h],xmm7

%endmacro

; Macro for popping all XMM registers from stack
macro popax 0

	movaps xmm0,xmmword[esp+00h]
	movaps xmm1,xmmword[esp+10h]
	movaps xmm2,xmmword[esp+20h]
	movaps xmm3,xmmword[esp+30h]
	movaps xmm4,xmmword[esp+40h]
	movaps xmm5,xmmword[esp+50h]
	movaps xmm6,xmmword[esp+60h]
	movaps xmm7,xmmword[esp+70h]
	add esp,80h

%endmacro

%endif