; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2023, Zero Tang. All rights reserved.
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
extern NoirInvalidOpcodeFaultHandler:proc
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

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirDivideErrorFaultHandler
	popaq
	iretq
	
AsmDivideErrorFaultHandler endp

AsmDebugFaultTrapHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirDebugFaultTrapHandler
	popaq
	iretq

AsmDebugFaultTrapHandler endp

AsmBreakpointTrapHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirBreakpointTrapHandler
	popaq
	iretq

AsmBreakpointTrapHandler endp

AsmOverflowTrapHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirOverflowTrapHandler
	popaq
	iretq

AsmOverflowTrapHandler endp

AsmBoundRangeFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirBoundRangeFaultHandler
	popaq
	iretq

AsmBoundRangeFaultHandler endp

AsmInvalidOpcodeFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirInvalidOpcodeFaultHandler
	popaq
	iretq

AsmInvalidOpcodeFaultHandler endp

AsmUnavailableDeviceFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirUnavailableDeviceFaultHandler
	popaq
	iretq

AsmUnavailableDeviceFaultHandler endp

AsmDoubleFaultAbortHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirDoubleFaultAbortHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmDoubleFaultAbortHandler endp

AsmInvalidTssFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirInvalidTssFaultHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmInvalidTssFaultHandler endp

AsmAbsentSegmentFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirAbsentSegmentFaultHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmAbsentSegmentFaultHandler endp

AsmStackFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirStackFaultHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmStackFaultHandler endp

AsmGeneralProtectionFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirGeneralProtectionFaultHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmGeneralProtectionFaultHandler endp

AsmPageFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirPageFaultHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmPageFaultHandler endp

AsmFloatingPointFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirFloatingPointFaultHandler
	popaq
	iretq

AsmFloatingPointFaultHandler endp

AsmAlignmentCheckFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirAlignmentCheckFaultHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmAlignmentCheckFaultHandler endp

AsmMachineCheckAbortHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirMachineCheckAbortHandler
	popaq
	iretq

AsmMachineCheckAbortHandler endp

AsmSimdFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirSimdFaultHandler
	popaq
	iretq

AsmSimdFaultHandler endp

AsmControlProtectionFaultHandler proc

	pushaq
	mov rcx,rsp
	lea rdx,[rsp+gpr_stack_size]
	call NoirControlProtectionFaultHandler
	popaq
	add rsp,8	; There is an error code for this exception.
	iretq

AsmControlProtectionFaultHandler endp

end