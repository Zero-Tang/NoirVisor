; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2022, Zero Tang. All rights reserved.
;
; This file is the host interrupt handler for NoirVisor.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/interrupt.asm

.code

noir_svm_host_nmi_handler proc

	; We do not handle NMI on ourself. NMI should be forwarded.
	; First of all, immediately disable interrupts globally.
	clgi
	; Do not use the iret instruction to return in that it would
	; remove the blocking of Non-Maskable Interrupts.
	; Simply emulate popping the rip. The rest will be handled later.
	ret 

noir_svm_host_nmi_handler endp

noir_svm_host_ready_nmi proc

	; This function prepares for handling interrupts.
	stgi
	; NMI should incur immediately after this instruction.
	; The rip is recovered by previous instruction
	add rsp,8		; Skip cs selector
	popfq			; Pop the rflags register
	; Now, restore the stack pointer.
	mov rsp,qword ptr [rsp]
	; The ss selector is skipped.
	; Stack is restored, return.
	ret

noir_svm_host_ready_nmi endp

end