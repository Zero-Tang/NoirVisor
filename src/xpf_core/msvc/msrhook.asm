; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2022, Zero Tang. All rights reserved.
;
; This file defines codes for MSR-Hook.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/msrhook.asm

ifdef _ia32
.686p
.model flat,stdcall
endif

.code

extern orig_system_call:dq
extern IndexOf_NtOpenProcess:dword
extern ProtPID:dword

ifdef _amd64

noir_system_call proc

	cmp eax,IndexOf_NtOpenProcess		; If the syscall index is hooked,
	je AsmHook_NtOpenProcess			; Then jump to our proxy function
	jmp orig_system_call				; Continue to KiSystemCall64

noir_system_call endp

AsmHook_NtOpenProcess proc

	; Later versions of Windows could have CR4.SMAP enabled.
	; The workaround is simple: set the RFlags.AC bit.
	mov rax,cr4
	bt rax,21						; Save the CR4.SMAP bit to RFLAGS.CF bit.
	jnc ComparePID
	stac							; The stac/clac instruction requires SMAP support.
	; NtOpenProcess has four parameters:
	; - ProcessHandle, stored in r10 register as pointer
	; - DesiredAccess, stored in rdx register as data
	; - ObjectAttributes, stored in r8 register as pointer
	; - ClientId, stored in r9 register as pointer
ComparePID:
	pushfq							; Save the RFLAGS
	mov rax,qword ptr[r9]			; PID is stored in [r9]
	and eax,0FFFFFFFCh				; Filter invalid fields of PID
	cmp eax,ProtPID					; If the PID is not protected,
	jne Final						; Then jump to final operation.
	xor rdx,rdx						; Clear DesiredAccess (No Access) if the PID is protected
Final:
	btr qword ptr [rsp],18			; Remove RFLAGS.AC bit in the saved RFLAGS.
	popfq							; Restore the RFLAGS.
	mov eax,IndexOf_NtOpenProcess	; Restore rax register
	jmp orig_system_call			; Continue to KiSystemCall64(Shadow)

AsmHook_NtOpenProcess endp

else

noir_system_call proc

	; On Win32, due to stdcall calling convention,
	; sysenter parameters are stored on user stack.
	; Stack pointer is stored in edx instead of esp.
	; Stack Layout should look like the following:
	; stack+0: Return address
	; stack+4: Secondary Caller
	; stack+8: Parameters...
	pushfd
	cmp eax,IndexOf_NtOpenProcess
	je AsmHook_NtOpenProcess
	popfd
	jmp orig_system_call

noir_system_call endp

AsmHook_NtOpenProcess proc

	; For NtOpenProcess, stack layout for parameters is:
	; [edx+08h] - ProcessHandle
	; [edx+0Ch] - DesiredAccess
	; [edx+10h] - ObjectAttributes
	; [edx+14h] - ClientId
	pushfd
	pushad
	; Get PID from parameter.
	mov ecx,dword ptr[edx+14h]
	mov ecx,dword ptr[ecx]
	and ecx,0FFFFFFFCh
	; Make comparison
	cmp ecx,ProtPID
	jne Final	; PID does not match
	popad
	popfd
	; return STATUS_DEVICE_PAPER_EMPTY;
	mov eax,08000000Eh
	mov ecx,edx
	mov edx,dword ptr[edx]
	sysexit
Final:
	popad
	popfd
	jmp orig_system_call

AsmHook_NtOpenProcess endp

endif

end