; NoirVisor - Hardware-Accelerated Hypervisor Solution
;
; Copyright 2018-2021, Zero Tang. All rights reserved.
;
; This file saves processor states for UEFI.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/uefi/kpcr.asm

section .text

%ifdef _amd64

bits 64

; For NoirVisor running on UEFI, a minimal quasi-OS kernel should be implemented.
; Following are the interrupt handlers for NoirVisor in Host Mode.

global NoirUnexpectedInterruptHandler

NoirUnexpectedInterruptHandler:

	; In principle, there shouldn't be interrupts in host.
	; Simply return.
	iret


; Implementations of certain instructions.
global noir_xsetbv
noir_xsetbv:

	mov eax,edx
	shr rdx,32
	xsetbv
	ret

%endif