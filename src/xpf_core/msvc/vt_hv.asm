; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2024, Zero Tang. All rights reserved.
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

ifdef _llvm

noir_vt_vmxon proc

	vmxon qword ptr [rcx]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmxon endp

noir_vt_vmxoff proc

	vmxoff
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmxoff endp

noir_vt_vmptrld proc

	vmptrld qword ptr [rcx]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmptrld endp

noir_vt_vmptrst proc

	vmptrst qword ptr [rcx]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmptrst endp

noir_vt_vmclear proc

	vmclear qword ptr [rcx]
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmclear endp


noir_vt_vmread proc

	vmread qword ptr [rdx],rcx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmread endp

noir_vt_vmwrite proc

	vmwrite rcx,rdx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmwrite endp

noir_vt_vmread64 proc

	vmread qword ptr [rdx],rcx
	setc al
	setz cl
	adc al,cl
	ret

noir_vt_vmread64 endp

noir_vt_vmwrite64 proc

	vmwrite rcx,rdx
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

nvc_vt_exit_handler_a proc frame

	; eb NoirVisor!nvc_vt_exit_handler_a cc
	nop			; Change to int 3 in order to debug-break.
	; Add a trap frame so WinDbg may display stack trace in Guest.
	.pushframe
	sub rsp,ktrap_frame_size-mach_frame_size+gpr_stack_size+20h
	.allocstack ktrap_frame_size-mach_frame_size+108h
	; Save all GPRs
	pushaq_fast 20h
	; Load the Guest GPR State to the first parameter.
	lea rcx,[rsp+20h]
	; Load vcpu to second parameter.
	mov rdx,qword ptr [rsp+ktrap_frame_size-mach_frame_size+gpr_stack_size+20h]
	; End of Prologue...
	.endprolog
	call nvc_vt_exit_handler
	; Restore GPR state.
	popaq_fast 20h
	; We don't have to increment stack here in
	; that the host rsp is already set in VMCS.
	; Check if the VMCS is launched.
	btr dword ptr [rsp+ktrap_frame_size-mach_frame_size+gpr_stack_size+20h+14h],0
	jc launch_initial_vmcs
	vmresume
	jmp vmentry_failure
launch_initial_vmcs:
	vmlaunch
vmentry_failure:
	; Usually we won't be here, unless the VM-Entry fails.
	; We will call the special procedure to handle this situation.
	; First Parameter: the GPR state of the guest.
	lea rcx,[rsp+20h]
	; Second Parameter: the vCPU.
	mov rdx,qword ptr [rsp+ktrap_frame_size-mach_frame_size+gpr_stack_size+20h]
	; Third Parameter: the VMX instruction status.
	setz r8b
	setc al
	adc r8b,al
	call nvc_vt_resume_failure
	; If it was Guest vCPU which failed to run, we may return to host.
	popaq_fast 20h
	vmresume
	; Use dead loop if failed again, albeit this is unlikely to happen.
	jmp vmentry_failure

nvc_vt_exit_handler_a endp

nvc_vt_subvert_processor_a proc

	pushfq
	xor rax,rax		; Make sure it would return zero if vmlaunch succeeds.
	pushaq
	mov r8,rsp
	sub rsp,20h
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