; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2022, Zero Tang. All rights reserved.
;
; This file is SSE4.2-Accelerated CRC32C Computation.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/msvc/crc32.asm

ifdef _ia32
.686p
.model flat,stdcall
endif

.code

ifdef _amd64

noir_check_sse42 proc

	xor eax,eax
	inc eax
	push rbx		; ebx is volatile
	cpuid
	bt ecx,20		; check flags
	pop rbx			; restore ebx
	setc al
	movzx eax,al
	ret

noir_check_sse42 endp

; Code Integrity is a performance-critical component.
; Thus SSE4.2 version of CRC32C is written in assembly.
noir_crc32_page_sse proc

	; Load relevant parameters.
	xchg rsi,rcx	; rsi is volatile
	mov rdx,rcx		; Save rsi to rdx
	xor r8,r8		; Initialize CRC checksum.
	mov ecx,512		; There are 512 8-byte blocks in a page.
	cld				; Ensure correct direction.
	; Use lods-loop combination for best performance.
loop_crc:
	lodsq
	crc32 r8,rax
	loop loop_crc
	xchg rsi,rdx	; Restore esi
	mov eax,r8d
	ret

noir_crc32_page_sse endp

else

noir_check_sse42 proc

	xor eax,eax
	inc eax
	push ebx		; ebx is volatile
	cpuid
	bt ecx,20		; check flags
	pop ebx			; restore ebx
	setc al
	movzx eax,al
	ret

noir_check_sse42 endp

; Code Integrity is a performance-critical component.
; Thus SSE4.2 version of CRC32C is written in assembly.
noir_crc32_page_sse proc uses esi p:dword

	; Load relevant parameters.
	mov esi,dword ptr [p]
	xor edx,edx		; Initialize CRC checksum.
	mov ecx,1024	; There are 1024 4-byte blocks in a page.
	cld				; Ensure correct direction.
	; Use lods-loop combination for best performance.
loop_crc:
	lodsd
	crc32 edx,eax
	loop loop_crc
	mov eax,edx
	ret

noir_crc32_page_sse endp

endif

end