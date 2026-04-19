; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2026, Zero Tang. All rights reserved.
;
; This file is part of NoirVisor SVM Core written in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/msvc/svm_hv.asm

ifdef _ia32
.686p
.model flat
endif

include noirhv.inc

.code

hvtext segment readonly align(4096) read execute nopage

extern nvc_svm_subvert_processor_i:proc
extern nvc_svm_exit_handler:proc

ifdef _amd64

stacktop_offset_guest_gpr equ 020h
stacktop_offset_guest_frame equ 0A0h
stacktop_offset_xsave_state equ 0D0h
stacktop_offset_gvmcb_pa equ 0D8h
stacktop_offset_hvmcb_pa equ 0E0h
stacktop_offset_vcpu_ptr equ 0E8h
stacktop_offset_cvcpu_ptr equ 0F0h
stacktop_offset_nvcpu_ptr equ 0F8h
stacktop_offset_proc_id equ 100h
stacktop_offset_guest_xcr0 equ 108h
stacktop_offset_host_xcr0 equ 110h

nvc_svm_return proc

	; Switch the stack where state is saved.
	mov rsp,rcx
	popaq_fast 0
	; In the restored GPR layout, we have:
	; rax=rip
	; rcx=rflags
	; rdx=rsp
	push rcx
	popfq			; Restore flags register
	mov rsp,rdx		; Restore stack pointer
	jmp rax			; Restore instruction pointer

nvc_svm_return endp

nvc_svm_exit_handler_a proc frame

	; At this moment, VM-Exit occured.
	; Add a trap frame so WinDbg may display stack trace in Guest.
	; To add a debug-break, enter following command to WinDbg.
	; ed NoirVisor!nvc_svm_exit_handler_a ccdc010f
	nop dword ptr [rax+20h]		; This is a purposeful four-byte nop instruction.
	; No need to subtract the stack pointer.
	; Save stack trace.
	.pushframe code
	; Save all GPRs, and pass to Exit Handler.
	pushaq_fast stacktop_offset_guest_gpr
	; Save processor's hidden state for Guest.
	vmsave rax
	; Load processor's hidden state for Host.
	mov rax,qword ptr[rsp+stacktop_offset_hvmcb_pa]
	vmload rax
	; Save the guest XCR0 and switch to host XCR0.
	xor ecx,ecx
	xgetbv
	mov dword ptr [rsp+stacktop_offset_guest_xcr0+0],eax
	mov dword ptr [rsp+stacktop_offset_guest_xcr0+4],edx
	; The xsetbv may cause VM-Exits, so avoid it if guest/host xcr0 equals.
	shl rdx,32
	or rax,rdx
	cmp rax,qword ptr [rsp+stacktop_offset_host_xcr0]
	je precall_gh_xcr0_equal
	mov eax,dword ptr [rsp+stacktop_offset_host_xcr0+0]
	mov edx,dword ptr [rsp+stacktop_offset_host_xcr0+4]
	xsetbv
precall_gh_xcr0_equal:
	; Save all volatile XMMs.
	mov rax,[rsp+stacktop_offset_xsave_state]
	save_volatile_xmm rax
	.allocstack 20h
	.endprolog
	; Just pass the stack to the handler
	mov rcx,rsp
	; End of Prologue...
	; Call Exit Handler
	call nvc_svm_exit_handler
	; Restore all volatile XMMs.
	mov rax,[rsp+stacktop_offset_xsave_state]
	restore_volatile_xmm rax
	; Switch back to the guest XCR0.
	mov rax,qword ptr [rsp+stacktop_offset_guest_xcr0]
	; If guest xcr0 equals to host xcr0, there's no need to switch xcr0.
	cmp rax,qword ptr [rsp+stacktop_offset_host_xcr0]
	je postcall_gh_xcr0_equal
	mov rdx,rax
	xor ecx,ecx
	shr rdx,32
	xsetbv
postcall_gh_xcr0_equal:
	; Restore all the GPRs.
	; Certain context should be revised by VMM.
	popaq_fast stacktop_offset_guest_gpr
	; After popaq, rax stores the physical
	; address of VMCB again.
	vmload rax
	vmrun rax
	; VM-Exit occured again, jump back.
	jmp nvc_svm_exit_handler_a

nvc_svm_exit_handler_a endp

nvc_svm_subvert_processor_a proc frame

	clgi		; Enter Atomic Execution State.
	pushfq
	.allocstack 8h
	pushaq
	mov rdx,rsp
	push rcx
	.pushreg rcx
	mov rcx,qword ptr[rcx+stacktop_offset_vcpu_ptr]
	rdsspq r8
	sub rsp,28h
	.allocstack 28h
	; First parameter is in rcx - vcpu
	; Second parameter is in rdx - guest rsp
	.endprolog
	call nvc_svm_subvert_processor_i
	; Now, rax stores the physical address of VMCB.
	; Switch stack pointer to host stack now.
	mov rsp,qword ptr[rsp+28h]
	; Stack is switched, launch guest now.
	; At this moment, the vmrun instruction
	; behaves like vmlaunch in Intel VT-x.
	vmrun rax
	; As the code goes here, VM-Exit occurs.
	; Jump to VM-Exit Handler.
	jmp nvc_svm_exit_handler_a

nvc_svm_subvert_processor_a endp

nvc_svm_guest_start proc frame

	.allocstack 0B0h
	.endprolog
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

nvc_svm_guest_start endp

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

hvtext ends

end