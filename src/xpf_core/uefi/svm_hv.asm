; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2022, Zero Tang. All rights reserved.
;
; This file is part of NoirVisor SVM Core written in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/uefi/svm_hv.asm

%include "noirhv.inc"

section .text

extern nvc_svm_subvert_processor_i
extern nvc_svm_exit_handler
extern system_cr3

%ifdef _amd64

bits 64

global noir_svm_vmmcall
noir_svm_vmmcall:

	vmmcall
	ret

nvc_svm_exit_handler_a:
	
	; At this moment, VM-Exit occured.
	; Save hidden state for Guest.
	vmsave
	; Load hidden state for Host.
	mov rax,qword[rsp+8]
	vmload
	mov rax,qword[rsp+8]
	; Save all GPRs and pass to Exit Handler
	pushaq
	mov rcx,rsp
	mov rdx,qword[rsp+90h]
	sub rsp,20h
	; Call Exit Handler
	call nvc_svm_exit_handler
	add rsp,20h
	; Restore all GPRs
	popaq
	; Load hidden state for Guest.
	vmload
	; Resume the guest.
	vmrun
	; VM-Exit occured again, jump back.
	jmp nvc_svm_exit_handler_a

global nvc_svm_subvert_processor_a
nvc_svm_subvert_processor_a:

	pushfq
	pushaq
	mov rdx,rsp
	mov r8,svm_launched
	push rcx
	mov rcx,qword[rcx+10h]
	sub rsp,20h
	; Parameters: rcx=vcpu, rdx=guest rsp, r8=guest rip.
	call nvc_svm_subvert_processor_i
	add rsp,20h
	; Switch Paging Table for Host. (UEFI Runtime Paging)
	mov rcx,qword[system_cr3]
	mov cr3,rcx
	pop rcx
	; Switch stack for Host.
	mov rsp,rcx
	; Launch guest. rax automatically stores vmcb.
	vmrun
	; At this moment, VM-Exit occured.
	; Jump to VM-Exit Handler.
	jmp nvc_svm_exit_handler_a
svm_launched:
	; At this moment, Guest is launched successfully.
	; Restore all registers.
	popaq
	; Return that subversion is successful. (noir_virt_on)
	xor eax,eax
	inc eax
	; Restore RFlags finally.
	popfq
	ret

%endif