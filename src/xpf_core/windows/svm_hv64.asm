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

extern nvc_svm_subvert_processor_i:qword
extern host_rsp_list:qword

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

nvc_svm_exit_handler_a proc

	; At this moment, VM-Exit occured.
	; Save all GPRs, and pass to Exit Handler
	pushaq
	mov rcx,rsp
	; Pass processor index to Exit Handler
	movzx rdx,byte ptr gs:[184h]
	sub rsp,20h
	; Call Exit Handler
	add rsp,20h
	; Restore all the GPRs.
	; Certain context should be revised by VMM.
	popaq
	; After popaq, rax stores the physical
	; address of VMCB again.
	vmrun rax
	; VM-Exit occured again, jump back.
	jmp nvc_svm_exit_handler_a

nvc_svm_exit_handler_a endp

nvc_svm_subvert_processor_a proc

	pushfq
	pushaq
	mov rdx,rsp
	mov r8,svm_launched
	sub rsp,20h
	; First parameter is in rcx - vcpu
	; Second parameter is in rdx - guest rsp
	; Third parameter is in r8 - guest rip
	call nvc_svm_subvert_processor_i
	add rsp,20h
	; Now, rax stores the physical address of VMCB.
	; Save rsp from host-rsp list.
	mov rdx,host_rsp_list
	; gs:[184h] stores the processor index.
	movzx rcx,byte ptr gs:[184h]
	; Switch rsp to host stack.
	mov rsp,qword ptr[rdx+rcx*8]
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