; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018, Zero Tang. All rights reserved.
;
; This file defines codes for MSR-Hook (LSTAR-Hook) on Win64.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/msrhook64.asm

extern orig_system_call:dq
extern IndexOf_NtOpenProcess:dword
extern ProtPID:dword

.code

noir_system_call proc

	pushfq								; Following code would destroy RFlags, so save it
	cmp eax,IndexOf_NtOpenProcess		; If the syscall index is hooked,
	je AsmHook_NtOpenProcess			; Then jump to our proxy function
	popfq								; Restore RFlags
	jmp orig_system_call				; Continue to KiSystemCall64

noir_system_call endp

AsmHook_NtOpenProcess proc

	; NtOpenProcess has four parameters:
	; - ProcessHandle, stored in r10 register as pointer
	; - DesiredAccess, stored in rdx register as data
	; - ObjectAttributes, stored in r8 register as pointer
	; - ClientId, stored in r9 register as pointer
	push rax					; We will use this register, so save it
	mov rax,qword ptr[r9]		; PID is stored in [r9]
	and eax,0FFFFFFFCh			; Filter invalid fields of PID
	cmp eax,ProtPID				; If the PID is not protected,
	jne Final					; Then jump to final operation
	xor rdx,rdx					; Clear DesiredAccess (No Access) if the PID is protected
Final:
	pop rax						; Restore rax register
	popfq						; Restore RFlags
	jmp orig_system_call		; Continue to KiSystemCall64

AsmHook_NtOpenProcess endp

end