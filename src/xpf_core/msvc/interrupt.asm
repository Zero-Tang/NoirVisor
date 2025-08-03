; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2025, Zero Tang. All rights reserved.
;
; This file is the host interrupt handler for NoirVisor.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/msvc/interrupt.asm

.code

hvtext segment readonly align(4096) read execute nopage

include noirhv.inc

; extern nvc_vt_inject_nmi_to_subverted_host:proc

extern noir_divide_error_fault_handler:proc
extern noir_debug_fault_trap_handler:proc
extern noir_breakpoint_trap_handler:proc
extern noir_overflow_trap_handler:proc
extern noir_bound_range_fault_handler:proc
extern noir_invalid_opcode_fault_handler:proc
extern noir_device_not_available_fault_handler:proc
extern noir_double_fault_abort_handler:proc
extern noir_invalid_tss_fault_handler:proc
extern noir_segment_not_present_fault_handler:proc
extern noir_stack_fault_handler:proc
extern noir_general_protection_fault_handler:proc
extern noir_page_fault_handler:proc
extern noir_x87_floating_point_fault_handler:proc
extern noir_alignment_check_fault_handler:proc
extern noir_machine_check_abort_handler:proc
extern noir_simd_floating_point_fault_handler:proc
extern noir_control_protection_fault_handler:proc

nvc_call_try_task proc frame

	sub rsp,28h
	.allocstack 28h
	.endprolog
	mov gs:[0h],rsp
	mov rax,end_of_try
	mov gs:[8h],rax
	mov rax,rcx
	mov rcx,rdx
	call rax
	xor eax,eax
end_of_try:
	add rsp,28h
	ret

nvc_call_try_task endp

__chkstk proc

	; In Rust, sorting a Vec will call __chkstk.
	; The ntoskrnl's implementation of __chkstk does nothing and returns.
	ret

__chkstk endp

nvc_vt_host_nmi_handler proc frame

	; In Intel VT-x, the NMI is not blocked while in Host Context.
	; Transfer the NMI to the guest if the host receives an NMI.
	; In other words, no registers can be destroyed in NMI handler of Intel VT-x.
	.pushframe
	pushaq
	sub rsp,28h
	.endprolog
	; call nvc_vt_inject_nmi_to_subverted_host
	add rsp,28h
	popaq
	; Use a special macro to return from NMI but do not unblock NMIs.
	nmiret

nvc_vt_host_nmi_handler endp

nvc_svm_host_nmi_handler proc frame

	.pushframe
	; We do not handle NMI on ourself. NMI should be forwarded.
	; First of all, immediately disable interrupts globally.
	.endprolog
	clgi
	; Use a special macro to return from NMI but do not unblock NMIs.
	nmiret
	; The arriving NMI is kept pending even if GIF is set later.

nvc_svm_host_nmi_handler endp

nvc_svm_host_ready_nmi proc frame

	; Set the GIF to unblock the NMI due to GIF.
	.endprolog
	stgi
	; NMI should occur immediately after this instruction.
	; NMI has completed without unblocking NMIs here.
	; The return value is the stack.
	; Hypervisor must inject NMI to the guest once the
	; nested hypervisor enters vGIF=1 state.
	ret

nvc_svm_host_ready_nmi endp

stacktop_offset_volatile_xmms equ 20h
stacktop_offset_gpr_state equ 90h
stacktop_offset_stack_frame equ 110h
interrupt_stacktop_size equ 110h

interrupt_handler_prologue_with_code macro

	.pushframe code
	sub rsp,interrupt_stacktop_size
	pushaq_fast stacktop_offset_gpr_state
	pushax_volatile_fast stacktop_offset_volatile_xmms
	; Construct parameters for the exception handler.
	lea rcx,[rsp+stacktop_offset_stack_frame]	; The first parameter stores the exception frame.
	lea rdx,[rsp+stacktop_offset_gpr_state]		; The second parameter stores the GPR state.
	.allocstack 20h
	.endprolog
	; Prologue is over. Call the handler.

endm

interrupt_handler_epilogue_with_code macro

	popax_volatile_fast stacktop_offset_volatile_xmms
	popaq_fast stacktop_offset_gpr_state
	; This exception has an error code.
	; It must be popped out before the exception returns.
	add rsp,interrupt_stacktop_size+8
	iretq

endm

interrupt_handler_prologue macro

	.pushframe
	sub rsp,interrupt_stacktop_size+8
	.allocstack 8
	pushaq_fast stacktop_offset_gpr_state
	pushax_volatile_fast stacktop_offset_volatile_xmms
	; Construct parameters for the exception handler.
	lea rcx,[rsp+stacktop_offset_stack_frame+8]	; The first parameter stores the exception frame.
	lea rdx,[rsp+stacktop_offset_gpr_state]		; The second parameter stores the GPR state.
	.allocstack 20h
	.endprolog
	; Prologue is over. Call the handler.

endm

interrupt_handler_epilogue macro

	popax_volatile_fast stacktop_offset_volatile_xmms
	popaq_fast stacktop_offset_gpr_state
	add rsp,interrupt_stacktop_size
	iretq

endm

noir_divide_error_fault_handler_a proc frame

	interrupt_handler_prologue
	call noir_divide_error_fault_handler
	interrupt_handler_epilogue

noir_divide_error_fault_handler_a endp

noir_debug_fault_trap_handler_a proc frame

	interrupt_handler_prologue
	call noir_debug_fault_trap_handler
	interrupt_handler_epilogue

noir_debug_fault_trap_handler_a endp

noir_breakpoint_trap_handler_a proc frame

	interrupt_handler_prologue
	call noir_breakpoint_trap_handler
	interrupt_handler_epilogue

noir_breakpoint_trap_handler_a endp

noir_overflow_trap_handler_a proc frame

	interrupt_handler_prologue
	call noir_overflow_trap_handler
	interrupt_handler_epilogue

noir_overflow_trap_handler_a endp

noir_bound_range_fault_handler_a proc frame

	interrupt_handler_prologue
	call noir_bound_range_fault_handler
	interrupt_handler_epilogue

noir_bound_range_fault_handler_a endp

noir_invalid_opcode_fault_handler_a proc frame

	interrupt_handler_prologue
	call noir_invalid_opcode_fault_handler
	interrupt_handler_epilogue

noir_invalid_opcode_fault_handler_a endp

noir_device_not_available_fault_handler_a proc frame

	interrupt_handler_prologue
	call noir_device_not_available_fault_handler
	interrupt_handler_epilogue

noir_device_not_available_fault_handler_a endp

noir_double_fault_abort_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_double_fault_abort_handler
	interrupt_handler_epilogue_with_code

noir_double_fault_abort_handler_a endp

noir_invalid_tss_fault_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_invalid_tss_fault_handler
	interrupt_handler_epilogue_with_code

noir_invalid_tss_fault_handler_a endp

noir_segment_not_present_fault_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_segment_not_present_fault_handler
	interrupt_handler_epilogue_with_code

noir_segment_not_present_fault_handler_a endp

noir_stack_fault_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_stack_fault_handler
	interrupt_handler_epilogue_with_code

noir_stack_fault_handler_a endp

noir_general_protection_fault_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_general_protection_fault_handler
	interrupt_handler_epilogue_with_code

noir_general_protection_fault_handler_a endp

noir_page_fault_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_page_fault_handler
	interrupt_handler_epilogue_with_code

noir_page_fault_handler_a endp

noir_x87_floating_point_fault_handler_a proc frame

	interrupt_handler_prologue
	call noir_x87_floating_point_fault_handler
	interrupt_handler_epilogue

noir_x87_floating_point_fault_handler_a endp

noir_alignment_check_fault_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_alignment_check_fault_handler
	interrupt_handler_epilogue_with_code

noir_alignment_check_fault_handler_a endp

noir_machine_check_abort_handler_a proc frame

	interrupt_handler_prologue
	call noir_machine_check_abort_handler
	interrupt_handler_epilogue

noir_machine_check_abort_handler_a endp

noir_simd_floating_point_fault_handler_a proc frame

	interrupt_handler_prologue
	call noir_simd_floating_point_fault_handler
	interrupt_handler_epilogue

noir_simd_floating_point_fault_handler_a endp

noir_control_protection_fault_handler_a proc frame

	interrupt_handler_prologue_with_code
	call noir_control_protection_fault_handler
	interrupt_handler_epilogue_with_code

noir_control_protection_fault_handler_a endp

hvtext ends

end