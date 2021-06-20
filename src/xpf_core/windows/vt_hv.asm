; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2021, Zero Tang. All rights reserved.
;
; This file is part of NoirVisor VT Core written in assembly language.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/vt_hv.asm

ifdef _ia32
.686p
.model flat,stdcall
endif

.code

include noirhv.inc

extern nvc_vt_subvert_processor_i:proc
extern nvc_vt_exit_handler:proc

ifdef _amd64

;Implement VMX instructions

noir_vt_invept proc

	invept rcx,xmmword ptr [rdx]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_invept endp

noir_vt_invvpid proc

	invvpid rcx,xmmword ptr [rdx]
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

	; pushax
	pushaq
	; Load the Guest GPR State to the first parameter.
	mov rcx,rsp
	; Load vcpu to second parameter.
	mov rdx,qword ptr [rsp+80h]
	sub rsp,20h
	call nvc_vt_exit_handler
	add rsp,20h
	popaq
	; popax
	vmresume

nvc_vt_exit_handler_a endp

nvc_vt_subvert_processor_a proc

	pushfq
	xor rax,rax		; Make sure it would return zero if vmlaunch succeeds.
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

end