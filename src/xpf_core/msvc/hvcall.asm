; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2026, Zero Tang. All rights reserved.
;
; This file assists handling hypercalls in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/msvc/hvcall.asm

.code

externdef MSHV_HVCALL_VA:qword

nvc_forward_fast_hypercall proc frame

	; Calling Convention:
	; rcx: Guest GPR state
	; rdx: Guest volatile XMM state
	; Save all GPR & volatile XMM state.
	sub rsp,28h
	.endprolog
	mov qword ptr [rsp],rcx
	mov rax,qword ptr [rcx+20h]
	movaps xmm0,xmmword ptr [rax]
	movaps xmm1,xmmword ptr [rax+10h]
	movaps xmm2,xmmword ptr [rax+20h]
	movaps xmm3,xmmword ptr [rax+30h]
	movaps xmm4,xmmword ptr [rax+40h]
	movaps xmm5,xmmword ptr [rax+50h]
	mov r8,qword ptr [rcx+18h]
	mov rdx,qword ptr [rcx+10h]
	mov rcx,qword ptr [rcx+8]
	call qword ptr [MSHV_HVCALL_VA]
	mov rcx,qword ptr [rsp]
	mov qword ptr [rcx+10h],rcx
	mov qword ptr [rcx+18h],r8
	mov qword ptr [rcx],rax
	mov rax,qword ptr [rcx+20h]
	movaps xmmword ptr [rax],xmm0
	movaps xmmword ptr [rax+10h],xmm1
	movaps xmmword ptr [rax+20h],xmm2
	movaps xmmword ptr [rax+30h],xmm3
	movaps xmmword ptr [rax+40h],xmm4
	movaps xmmword ptr [rax+50h],xmm5
	add rsp,28h
	ret

nvc_forward_fast_hypercall endp

nvc_forward_memory_mapped_hypercall proc frame

	sub rsp,28h
	.endprolog
	mov rax,r9
	vmmcall
	add rsp,28h
	ret

nvc_forward_memory_mapped_hypercall endp

end