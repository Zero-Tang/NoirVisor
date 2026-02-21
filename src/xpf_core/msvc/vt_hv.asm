; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2025, Zero Tang. All rights reserved.
;
; This file is part of NoirVisor VT Core written in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/msvc/vt_hv.asm

ifdef _ia32
.686p
.model flat,stdcall
endif

.code

hvtext segment readonly align(4096) read execute nopage

include noirhv.inc

extern nvc_vt_subvert_processor_i:proc
extern nvc_vt_exit_handler:proc
extern nvc_vt_resume_failure:proc

ifdef _amd64

stacktop_offset_volatile_xmms equ 20h
stacktop_offset_guest_gpr equ 090h
stacktop_offset_guest_frame equ 110h
stacktop_offset_vcpu_ptr equ 140h
stacktop_offset_flags equ 15Ch
stacktop_offset_guest_xcr0 equ 160h
stacktop_offset_host_xcr0 equ 168h

nvc_vt_resume_without_entry proc

	mov rsp,rcx
	popaq_fast 0
	; In the restored GPR layout, we have:
	; rax=rip
	; rcx=rflags
	; rdx=rsp
	mov rsp,rdx		; Restore stack pointer
	push rcx
	popfq			; Restore flags register
	jmp rax			; Restore instruction pointer

nvc_vt_resume_without_entry endp

nvc_vt_exit_handler_a proc frame

	; eb NoirVisor!nvc_vt_exit_handler_a cc
	nop			; Change to int 3 in order to debug-break.
	; Add a trap frame so WinDbg may display stack trace in Guest.
	.pushframe code
	; Save all GPRs, and pass to Exit Handler.
	pushaq_fast stacktop_offset_guest_gpr
	; Before saving volatile XMM state, save and restore xcr0.
	xor ecx,ecx
	xgetbv
	mov dword ptr [rsp+stacktop_offset_guest_xcr0+0],eax
	mov dword ptr [rsp+stacktop_offset_guest_xcr0+4],edx
	; The xsetbv unconditionally causes VM-Exits, so avoid it if guest/host xcr0 equals.
	shl rdx,32
	or rax,rdx
	cmp rax,qword ptr [rsp+stacktop_offset_host_xcr0]
	je precall_gh_xcr0_equal
	mov eax,dword ptr [rsp+stacktop_offset_host_xcr0+0]
	mov edx,dword ptr [rsp+stacktop_offset_host_xcr0+4]
	xsetbv
precall_gh_xcr0_equal:
	; Save volatile XMM State.
	pushax_volatile_fast stacktop_offset_volatile_xmms
	.allocstack 20h
	.endprolog
	; Just pass the stack to the exit handler.
	mov rcx,rsp
	; Call exit handler
	call nvc_vt_exit_handler
resume_guest:
	; Restore volatile XMM State.
	popax_volatile_fast stacktop_offset_volatile_xmms
	; After restoring volatile XMM state, load guest xcr0.
	; The xsetbv unconditionally causes VM-Exits, so avoid it if guest/host xcr0 equals.
	mov rax,qword ptr [rsp+stacktop_offset_guest_xcr0]
	cmp rax,qword ptr [rsp+stacktop_offset_host_xcr0]
	je postcall_gh_xcr0_equal
	mov rdx,rax
	shr rdx,32
	xor ecx,ecx
	xsetbv
postcall_gh_xcr0_equal:
	; Restore all GPRs.
	popaq_fast stacktop_offset_guest_gpr
	; Check if the VMCS is launched.
	btr dword ptr [rsp+stacktop_offset_flags],0
	jc launch_initial_vmcs
	vmresume
	jmp vmentry_failure
launch_initial_vmcs:
	vmlaunch
vmentry_failure:
	; Usually we won't be here, unless the VM-Entry fails.
	; We will call the special procedure to handle this situation.
	; First Parameter: the parameter
	mov rcx,rsp
	; Second Parameter: the VMX instruction status.
	setz dl
	setc al
	adc dl,al
	call nvc_vt_resume_failure
	; Try to resume again.
	jmp resume_guest

nvc_vt_exit_handler_a endp

nvc_vt_subvert_processor_a proc frame

	pushfq
	.allocstack 8
	xor rax,rax		; Make sure it would return zero if vmlaunch succeeds.
	pushaq
	mov rdx,rsp
	rdsspq r8
	sub rsp,20h
	.allocstack 20h
	.endprolog
	cli
	call nvc_vt_subvert_processor_i
	; At this moment, VM-Entry resulted failure.
	add rsp,20h
	mov qword ptr [rsp],rax
	jmp nvc_vt_guest_start

nvc_vt_subvert_processor_a endp

nvc_vt_guest_start proc

	; There are four conditions why we are here:
	; 1. al=0. VM-Entry is successful (Expected condition)
	; 2. al=1. VM-Entry failed by valid vm-fail
	; 3. al=2. VM-Entry failed by invalid vm-fail (Not possible, though)
	; 4. al=3. VM-Entry failed due to invalid guest state
	popaq
	popfq
	; Things will be handled in code written in C.
	ret

nvc_vt_guest_start endp

else

noir_vt_invept proc inv_type:dword,descriptor:dword

	mov ecx,dword ptr [inv_type]
	mov edx,dword ptr [descriptor]
	invept xmmword ptr [edx],ecx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_invept endp

noir_vt_invvpid proc inv_type:dword,descriptor:dword

	mov ecx,dword ptr [inv_type]
	mov edx,dword ptr [descriptor]
	invvpid xmmword ptr [edx],ecx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_invvpid endp

noir_vt_vmcall proc index:dword,context:dword

	mov ecx,dword ptr [index]
	mov edx,dword ptr [context]
	vmcall
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmcall endp

noir_vt_vmxon proc vmxon_region:dword

	vmxon qword ptr[vmxon_region]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmxon endp

noir_vt_vmptrld proc vmcs_pa:dword

	vmptrld qword ptr[vmcs_pa]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmptrld endp

noir_vt_vmclear proc vmcs_pa:dword

	vmclear qword ptr[vmcs_pa]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmclear endp

noir_vt_vmread proc field:dword,value:dword

	mov ecx,dword ptr[field]
	vmread dword ptr[value],ecx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmread endp

noir_vt_vmwrite proc field:dword,value:dword

	mov ecx,dword ptr[field]
	vmwrite ecx,dword ptr[value]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmwrite endp

noir_vt_vmread64 proc field:dword,value:dword

	mov ecx,dword ptr[field]
	vmread dword ptr[value],ecx
	inc ecx
	vmread dword ptr[value+4],edx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmread64 endp

noir_vt_vmwrite64 proc field:dword,val_lo:dword,val_hi:dword

	mov ecx,dword ptr[field]
	vmwrite ecx,dword ptr[val_lo]
	inc ecx
	vmwrite ecx,dword ptr[val_hi]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmwrite64 endp

noir_vt_vmlaunch proc

	vmlaunch
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmlaunch endp

noir_vt_vmresume proc

	vmresume
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmresume endp

endif

hvtext ends

end