// NoirVisor - Hardware-Accelerated Hypervisor solution
//
// Copyright (c) Zero Tang, 2018-2026. All rights reserved.
//
// This file is part of NoirVisor SVM Core written in assembly language.
//
// This program is distributed in the hope that it will be successful, but
// without any warranty (no matter implied warranty of merchantability or
// fitness for a particular purpose, etc.).

.include "src/xpf_core/hv_host/asm_helper_x86.inc"

.extern nvc_svm_exit_handler
.extern nvc_svm_subvert_processor_i

STACKTOP_OFFSET_GUEST_GPR=0x20
STACKTOP_OFFSET_GUEST_FRAME=0xA0
STACKTOP_OFFSET_XSAVE_STATE=0xD0
STACKTOP_OFFSET_GVMCB_PA=0xD8
STACKTOP_OFFSET_HVMCB_PA=0xE0
STACKTOP_OFFSET_VCPU_PTR=0xE8
STACKTOP_OFFSET_CVCPU_PTR=0xF0
STACKTOP_OFFSET_NVCPU_PTR=0xF8
STACKTOP_OFFSET_PROC_ID=0x100
STACKTOP_OFFSET_GUEST_XCR0=0x108
STACKTOP_OFFSET_HOST_XCR0=0x110

.global nvc_svm_return
nvc_svm_return:
	// Switch the stack where state is saved.
	mov rsp,rcx
	popaq_fast 0
	// In the restored GPR layout, we have:
	// rax=rip, rcx=rflags, rdx=rsp
	push rcx
	popfq
	mov rsp,rdx
	jmp rax

.global nvc_svm_guest_start
.seh_proc nvc_svm_guest_start
nvc_svm_guest_start:
	.seh_stackalloc 0x90
	.seh_endprologue
	// At this moment, guest is successfully launched.
	// Host rsp is saved and guest rsp is switched by vmrun instruction.
	// Restore all General-Purpose Registers.
	popaq_fast 0
	add rsp,0x80
	// Restore rflags
	popfq
	ret
.seh_endproc

.global nvc_svm_exit_handler_a
.seh_proc nvc_svm_exit_handler_a
nvc_svm_exit_handler_a:
	// At this moment, VM-Exit occured.
	// Add a trap frame so that WinDbg may display stack trace for the guest.
	.seh_pushframe @code
	.seh_stackalloc 0xF0
	// Save all GPRs.
	pushaq_fast STACKTOP_OFFSET_GUEST_GPR
	.seh_endprologue
	// Save processor's hidden state.
	vmsave rax
	// Load processor's hidden state.
	mov rax,qword ptr [rsp+STACKTOP_OFFSET_HVMCB_PA]
	vmload rax
	// Save the guest XCR0.
	xor ecx,ecx
	xgetbv
	shl rdx,32
	or rax,rdx
	mov qword ptr [rsp+STACKTOP_OFFSET_GUEST_XCR0],rax
	// The xsetbv instruction may cause VM-Exits. Avoid it if Guest/Host XCR0 equals.
	cmp rax,qword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0]
	je svm_precall_gh_xcr0_equal
	// Guest's XCR0 does not equal to host's. Load Host XCR0.
	mov eax,dword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0+0]
	mov edx,dword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0+4]
	xsetbv
svm_precall_gh_xcr0_equal:
	// Save all volatile XMM registers.
	mov rax,qword ptr [rsp+STACKTOP_OFFSET_XSAVE_STATE]
	save_volatile_xmm rax
	// Pass the stack to the exit-handler.
	mov rcx,rsp
	call nvc_svm_exit_handler
	// Restore all volatile XMM registers.
	mov rax,[rsp+STACKTOP_OFFSET_XSAVE_STATE]
	restore_volatile_xmm rax
	// Switch back to the guest XCR0.
	mov rax,qword ptr [rsp+STACKTOP_OFFSET_GUEST_XCR0]
	// If guest XCR0 equals to host XCR0, there's no need to switch XCR0.
	cmp rax,qword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0]
	je svm_post_gh_xcr0_equal
	mov rdx,rax
	xor ecx,ecx
	shr rdx,32
	xsetbv
svm_post_gh_xcr0_equal:
	// Restore all GPRs.
	popaq_fast STACKTOP_OFFSET_GUEST_GPR
	// The rax register should already contain the VMCB physical address.
	vmload rax
	vmrun rax
	// VM-Exit occured again. Jump back.
	jmp nvc_svm_exit_handler_a
.seh_endproc

.global nvc_svm_subvert_processor_a
.seh_proc nvc_svm_subvert_processor_a
nvc_svm_subvert_processor_a:
	// Enter Atomic Execution State.
	clgi
	// Save register state.
	pushfq
	.seh_stackalloc 8
	sub rsp,0xA8
	.seh_stackalloc 0xA8
	pushaq_fast 0x28
	.seh_endprologue
	// Load arguments for processor subverter.
	mov rcx,qword ptr [rcx+STACKTOP_OFFSET_VCPU_PTR]
	lea rdx,[rsp+0x28]
	rdsspq r8
	call nvc_svm_subvert_processor_i
	// Return value is physical address of VMCB.
	// Switch the stack pointer to host stack.
	mov rsp,qword ptr [rsp+0x30]
	// Stack is switched. Launch the guest now.
	vmrun rax
	// VM-Exit occurs here. Jump to VM-Exit Handler.
	jmp nvc_svm_exit_handler_a

.seh_endproc