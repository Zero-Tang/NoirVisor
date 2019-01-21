; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018, Zero Tang. All rights reserved.
;
; This file is part of NoirVisor VT Core written in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/vt_hv64.asm

.code

extern nvc_vt_subvert_processor_i:proc
extern nvc_vt_exit_handler:proc

; Macro for pushing all GPRs to stack.
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

; Macro for poping all GPRs from stack.
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

;Implement VMX instructions

noir_vt_invept proc

	invept xmmword ptr [rdx],rcx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_invept endp

noir_vt_invvpid proc

	invvpid xmmword ptr [rdx],rcx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_invvpid endp

noir_vt_vmcall proc

	vmcall
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmcall endp

nvc_vt_resume_without_entry proc

	mov rsp,rcx
	popaq
	; In the restored GPR layout, we have:
	; rax=rip
	; rcx=rflags
	; rdx=rsp
	mov rsp,rdx		; Restore stack pointer
	push rcx
	popfq			; Restore flags register
	jmp rax			; Restore instruction pointer

nvc_vt_resume_without_entry endp

nvc_vt_exit_handler_a proc

	pushaq
	mov rcx,rsp
	sub rsp,20h
	call nvc_vt_exit_handler
	add rsp,20h
	popaq
	vmresume

nvc_vt_exit_handler_a endp

nvc_vt_subvert_processor_a proc

	pushfq
	pushaq
	mov r8,rsp
	mov r9,vt_launched
	sub rsp,20h
	call nvc_vt_subvert_processor_i
	;At this moment, VM-Entry resulted failure.
	add rsp,20h
	mov qword ptr [rsp],rax
vt_launched:
	; There are four conditions why we are here:
	; 1. al=0. VM-Entry is successful (Expected condition)
	; 2. al=1. VM-Entry failed by valid vm-fail
	; 3. al=2. VM-Entry failed by invalid vm-fail (Not possible, though)
	; 4. al=3. VM-Entry failed due to invalid guest state
	popaq
	popfq
	; Things will be handled in code written in C.
	ret

nvc_vt_subvert_processor_a endp

end