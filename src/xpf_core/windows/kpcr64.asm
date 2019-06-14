; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2019, Zero Tang. All rights reserved.
;
; This file is a supplementary library for 64-bit processor utility.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/kpcr64.asm

.code

; Implement SGDT instruction
__sgdt proc

	sgdt [rcx]
	ret

__sgdt endp

__sldt proc

	sldt ax
	ret

__sldt endp

__str proc

	str ax
	ret

__str endp

__readcr2 proc

	mov rax,cr2
	ret

__readcr2 endp

end