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
; File location: ./xpf_core/windows/svm_hv.asm

ifdef _ia32
.686p
.model flat
endif

.code

include noirhv.inc

extern nvc_svm_subvert_processor_i:proc
extern nvc_svm_exit_handler:proc

ifdef _amd64

extern system_cr3:qword

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
	; Load processor's hidden state for Host.
	mov rax,qword ptr[rsp+8]
	vmload rax
	mov rax,qword ptr[rsp]
	; Save all GPRs, and pass to Exit Handler
	; pushax
	pushaq
	mov rcx,rsp
	mov rdx,qword ptr[rsp+90h]
	sub rsp,20h
	; Call Exit Handler
	call nvc_svm_exit_handler
	add rsp,20h
	; Restore all the GPRs.
	; Certain context should be revised by VMM.
	popaq
	; popax
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
	mov rcx,qword ptr[rcx+10h]
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

else

assume fs:nothing

extern system_cr3:dword

; A simple implementation for vmmcall instruction.
noir_svm_vmmcall proc index:dword,context:dword

	mov ecx,dword ptr [index]
	mov edx,dword ptr [context]
	vmmcall
	ret

noir_svm_vmmcall endp

nvc_svm_return proc stack:dword

	mov ecx,dword ptr [stack]
	; Switch the stack where state is saved.
	mov esp,ecx
	popad
	; In the restored GPR layout, we have:
	; eax=eip
	; ecx=eflags
	; edx=esp
	push ecx
	popfd			; Restore flags register
	mov esp,edx		; Restore stack pointer
	jmp eax			; Restore instruction pointer

nvc_svm_return endp

nvc_svm_exit_handler_a proc

	; At this moment, VM-Exit occured.
	; Save processor hidden state.
	vmsave eax
	; Save GPR state.
	pushad
	mov ecx,esp
	movzx edx,byte ptr fs:[51h]
	; Invoke VM-Exit Handler
	call nvc_svm_exit_handler
	; Restore Exit Handler
	popad
	; After popad, eax contains VMCB.
	; Load processor hidden state.
	vmload eax
	; Resume guest.
	vmrun eax
	; VM-Exit occurs again, jump back.
	jmp nvc_svm_exit_handler_a

nvc_svm_exit_handler_a endp

nvc_svm_subvert_processor_a proc

	pushfd
	pushad
	mov edx,esp
	push ecx
	; Invoke nvc_svm_subvert_processor_i
	push svm_launched
	push edx
	push dword ptr[ecx+4]
	call nvc_svm_subvert_processor_i
	; Switch Page Table to System Page Table
	mov ecx,dword ptr[system_cr3]
	mov cr3,eax
	; Now, eax stores the physical address of VMCB.
	; Switch stack pointer to host stack.
	pop ecx
	mov esp,ecx
	; Stack is switch, launch the guest.
	vmrun eax
	; Now, VM-Exit occurs. Jump to handlers.
	jmp nvc_svm_exit_handler_a
svm_launched:
	; At this moment, Guest is successfully launched.
	; Host esp is saved and Guest esp is switched
	; automatically by vmrun instruction
	; Now, restore all registers.
	popad
	; Return that we are successful (noir_virt_on)
	xor eax,eax
	inc eax
	popfd
	ret

nvc_svm_subvert_processor_a endp

endif

end