; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2020, Zero Tang. All rights reserved.
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

extern nvc_vt_subvert_processor_i:proc
extern nvc_vt_exit_handler:proc

ifdef _amd64

; Macro for pushing all GPRs to stack.
pushaq macro
	
	sub rsp,80h
	mov qword ptr [rsp+00h],rax
	mov qword ptr [rsp+08h],rcx
	mov qword ptr [rsp+10h],rdx
	mov qword ptr [rsp+18h],rbx
	mov qword ptr [rsp+28h],rbp
	mov qword ptr [rsp+30h],rsi
	mov qword ptr [rsp+38h],rdi
	mov qword ptr [rsp+40h],r8
	mov qword ptr [rsp+48h],r9
	mov qword ptr [rsp+50h],r10
	mov qword ptr [rsp+58h],r11
	mov qword ptr [rsp+60h],r12
	mov qword ptr [rsp+68h],r13
	mov qword ptr [rsp+70h],r14
	mov qword ptr [rsp+78h],r15
	
endm

; Macro for poping all GPRs from stack.
popaq macro

	mov rax,qword ptr [rsp]
	mov rcx,qword ptr [rsp+8]
	mov rdx,qword ptr [rsp+10h]
	mov rbx,qword ptr [rsp+18h]
	mov rbp,qword ptr [rsp+28h]
	mov rsi,qword ptr [rsp+30h]
	mov rdi,qword ptr [rsp+38h]
	mov r8, qword ptr [rsp+40h]
	mov r9, qword ptr [rsp+48h]
	mov r10,qword ptr [rsp+50h]
	mov r11,qword ptr [rsp+58h]
	mov r12,qword ptr [rsp+60h]
	mov r13,qword ptr [rsp+68h]
	mov r14,qword ptr [rsp+70h]
	mov r15,qword ptr [rsp+78h]
	add rsp,80h

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

else

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

noir_vt_vmwrite64 proc field:dword,value:dword

	mov ecx,dword ptr[field]
	vmwrite ecx,dword ptr[value]
	inc ecx
	vmwrite ecx,dword ptr[value+4]
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