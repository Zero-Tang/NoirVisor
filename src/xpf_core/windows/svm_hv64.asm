; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018, Zero Tang. All rights reserved.
;
; This file is part of NoirVisor SVM Core written in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/svm_hv64.asm

.code

pushaq macro
	
	push r15
	push r14
	push r13
	push r12
	push r11
	push r10
	push r9
	push r8
	push rdi
	push rsi
	push rbp
	sub rsp,8
	push rbx
	push rdx
	push rcx
	push rax
	
endm

popaq macro

	pop rax
	pop rcx
	pop rdx
	pop rbx
	add rsp,8
	pop rbp
	pop rsi
	pop rdi
	pop r8
	pop r9
	pop r10
	pop r11
	pop r12
	pop r13
	pop r14
	pop r15

endm

noir_svm_vmmcall proc

	vmmcall
	ret

noir_svm_vmmcall endp

end