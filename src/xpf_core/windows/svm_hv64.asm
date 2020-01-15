; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2020, Zero Tang. All rights reserved.
;
; This file is part of NoirVisor SVM Core written in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/svm_hv64.asm

.code

extern nvc_svm_subvert_processor_i:proc
extern nvc_svm_exit_handler:proc
extern system_cr3:qword

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

; A simple implementation for vmmcall instruction.
noir_svm_vmmcall proc

	vmmcall
	ret

noir_svm_vmmcall endp

nvc_svm_return proc

	; Switch the stack where state is saved.
	mov rsp,rcx
	popaq
	; In the restored GPR layout, we have:
	; rax=rip
	; rcx=rflags
	; rdx=rsp
	push rcx
	popfq			; Restore flags register
	mov rsp,rdx		; Restore stack pointer
	jmp rax			; Restore instruction pointer

nvc_svm_return endp

nvc_svm_exit_handler_a proc

	; At this moment, VM-Exit occured.
	; Save processor's hidden state for VM.
	vmsave rax
	; Save all GPRs, and pass to Exit Handler
	pushaq
	mov rcx,rsp
	; Pass processor index to Exit Handler
	movzx rdx,byte ptr gs:[184h]
	sub rsp,20h
	; Call Exit Handler
	call nvc_svm_exit_handler
	add rsp,20h
	; Restore all the GPRs.
	; Certain context should be revised by VMM.
	popaq
	; After popaq, rax stores the physical
	; address of VMCB again.
	; Load processor's hidden state for VM.
	vmload rax
	vmrun rax
	; VM-Exit occured again, jump back.
	jmp nvc_svm_exit_handler_a

nvc_svm_exit_handler_a endp

nvc_svm_subvert_processor_a proc

	pushfq
	pushaq
	mov rdx,rsp
	mov r8,svm_launched
	push rcx
	mov rcx,qword ptr[rcx+8]
	sub rsp,20h
	; First parameter is in rcx - vcpu
	; Second parameter is in rdx - guest rsp
	; Third parameter is in r8 - guest rip
	call nvc_svm_subvert_processor_i
	add rsp,20h
	; Switch Page Table to System Page Table
	mov rcx,qword ptr[system_cr3]
	mov cr3,rcx
	; Now, rax stores the physical address of VMCB.
	; Switch stack pointer to host stack now.
	pop rcx
	mov rsp,rcx
	; Stack is switched, launch guest now.
	; At this moment, the vmrun instruction
	; behaves like vmlaunch in Intel VT-x.
	vmrun rax
	; As we goes here, VM-Exit occurs.
	; Jump to VM-Exit Handler.
	jmp nvc_svm_exit_handler_a
svm_launched:
	; At this moment, Guest is successfully launched.
	; Host rsp is saved and Guest rsp is switched
	; automatically by vmrun instruction.
	; Now, restore all registers.
	popaq
	; Return that we are successful (noir_virt_on)
	xor eax,eax
	inc eax
	; Restore Rflag at last.
	popfq
	ret

nvc_svm_subvert_processor_a endp

end