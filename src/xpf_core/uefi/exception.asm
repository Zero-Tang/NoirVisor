; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2022, Zero Tang. All rights reserved.
;
; This file is the host exception handler for NoirVisor.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/uefi/exception.asm

.code

include noirhv.inc

extern NoirDivideErrorFaultHandler:proc
extern NoirDebugFaultTrapHandler:proc
; NMIs should be handled by specific hypervisor core.
extern NoirBreakpointTrapHandler:proc
extern NoirOverflowTrapHandler:proc
extern NoirBoundRangeFaultHandler:proc
extern NoirInvalidOpcodeFaultHandle:proc
extern NoirUnavailableDeviceFaultHandler:proc
extern NoirDoubleFaultAbortHandler:proc
extern NoirInvalidTssFaultHandler:proc
extern NoirAbsentSegmentFaultHandler:proc
extern NoirStackFaultHandler:proc
extern NoirGeneralProtectionFaultHandler:proc
extern NoirPageFaultHandler:proc
extern NoirFloatingPointFaultHandler:proc
extern NoirAlignmentCheckFaultHandler:proc
extern NoirMachineCheckAbortHandler:proc
extern NoirSimdFaultHandler:proc
extern NoirControlProtectionFaultHandler:proc

AsmDivideErrorFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirDivideErrorFaultHandler
	popaq
	popfq
	iret
	
AsmDivideErrorFaultHandler endp

AsmDebugFaultTrapHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirDebugFaultTrapHandler
	popaq
	popfq
	iret

AsmDebugFaultTrapHandler endp

AsmBreakpointTrapHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirBreakpointTrapHandler
	popaq
	popfq
	iret

AsmBreakpointTrapHandler endp

AsmOverflowTrapHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirOverflowTrapHandler
	popaq
	popfq
	iret

AsmOverflowTrapHandler endp

AsmBoundRangeFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirBoundRangeFaultHandler
	popaq
	popfq
	iret

AsmBoundRangeFaultHandler endp

AsmInvalidOpcodeFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirInvalidOpcodeFaultHandler
	popaq
	popfq
	iret

AsmInvalidOpcodeFaultHandler endp

AsmUnavailableDeviceFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirUnavailableDeviceFaultHandler
	popaq
	popfq
	iret

AsmUnavailableDeviceFaultHandler endp

AsmDoubleFaultAbortHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirDoubleFaultAbortHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmDoubleFaultAbortHandler endp

AsmInvalidTssFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirInvalidTssFaultHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmInvalidTssFaultHandler endp

AsmAbsentSegmentFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirAbsentSegmentFaultHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmAbsentSegmentFaultHandler endp

AsmStackFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirStackFaultHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmStackFaultHandler endp

AsmGeneralProtectionFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirGeneralProtectionFaultHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmGeneralProtectionFaultHandler endp

AsmPageFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirPageFaultHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmPageFaultHandler endp

AsmFloatingPointFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirFloatingPointFaultHandler
	popaq
	popfq
	iret

AsmFloatingPointFaultHandler endp

AsmAlignmentCheckFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirAlignmentCheckFaultHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmAlignmentCheckFaultHandler endp

AsmMachineCheckAbortHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirMachineCheckAbortHandler
	popaq
	popfq
	iret

AsmMachineCheckAbortHandler endp

AsmSimdFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirSimdFaultHandler
	popaq
	popfq
	iret

AsmSimdFaultHandler endp

AsmControlProtectionFaultHandler proc

	pushfq
	pushaq
	mov rcx,rsp
	lea rdx,[rsp+88h]
	call NoirControlProtectionFaultHandler
	popaq
	popfq
	add rsp,8	; There is an error code for this exception.
	iret

AsmControlProtectionFaultHandler endp

end