// NoirVisor - Hardware-Accelerated Hypervisor solution
//
// Copyright (c) Zero Tang, 2018-2026. All rights reserved.
//
// This file is part of NoirVisor VT Core written in assembly language.
//
// This program is distributed in the hope that it will be successful, but
// without any warranty (no matter implied warranty of merchantability or
// fitness for a particular purpose, etc.).

.include "src/xpf_core/hv_host/asm_helper_x86.inc"

.extern nvc_vt_exit_handler
.extern nvc_vt_subvert_processor_i
.extern nvc_vt_resume_failure
.extern nvc_vt_inject_nmi_to_subverted_host

STACKTOP_OFFSET_GUEST_GPR=0x20
STACKTOP_OFFSET_GUEST_FRAME=0xA0
STACKTOP_OFFSET_XSAVE_STATE=0xD0
STACKTOP_OFFSET_VCPU_PTR=0xD8
STACKTOP_OFFSET_FLAGS=0xF4
STACKTOP_OFFSET_GUEST_XCR0=0xF8
STACKTOP_OFFSET_HOST_XCR0=0x100

.global nvc_vt_resume_without_entry
nvc_vt_resume_without_entry:
	mov rsp,rcx
	popaq_fast 0
	// In the restored GPR layout, we have:
	// rax=rip, rcx=rflags, rdx=rsp
	mov rsp,rdx
	push rcx
	popfq
	jmp rax

.global nvc_vt_guest_start
nvc_vt_guest_start:
	popaq_fast 0
	add rsp,0x80
	popfq
	ret

.global nvc_vt_subvert_processor_a
.seh_proc nvc_vt_subvert_processor_a
nvc_vt_subvert_processor_a:
	pushfq
	.seh_stackalloc 8
	cli
	xor eax,eax
	sub rsp,0xA0
	.seh_stackalloc 0xA0
	pushaq_fast 0x20
	.seh_endprologue
	lea rdx,[rsp+0x20]
	rdsspq r8
	call nvc_vt_subvert_processor_i
	// At this moment, VM-Entry resulted failure.
	add rsp,0x28
	mov qword ptr [rsp],rax
	jmp nvc_vt_guest_start
.seh_endproc

.global nvc_vt_exit_handler_a
.seh_proc nvc_vt_exit_handler_a
nvc_vt_exit_handler_a:
	// Put a trap frame so that WinDbg may display stack trace in Guest.
	.seh_pushframe @code
	.seh_stackalloc 0xE0
	pushaq_fast STACKTOP_OFFSET_GUEST_GPR
	.seh_endprologue
	// Before saving volatile XMM state, save and restore xcr0.
	xor ecx,ecx
	xgetbv
	shl rdx,32
	or rax,rdx
	mov qword ptr [rsp+STACKTOP_OFFSET_GUEST_XCR0],rax
	// The xsetbv unconditionally causes VM-Exits, so avoid it if guest/host xcr0 equals.
	cmp rax,qword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0]
	je precall_gh_xcr0_equal
	mov eax,dword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0+0]
	mov edx,dword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0+4]
	xsetbv
precall_gh_xcr0_equal:
	// Save volatile XMM state.
	mov rax,qword ptr [rsp+STACKTOP_OFFSET_XSAVE_STATE]
	save_volatile_xmm rax
	// Pass the stack to the exit handler.
	mov rcx,rsp
	call nvc_vt_exit_handler
resume_guest:
	// Restore the volatile XMM state.
	mov rax,qword ptr [rsp+STACKTOP_OFFSET_XSAVE_STATE]
	restore_volatile_xmm rax
	// After restoring the volatile XMM state, load guest XCR0.
	mov rax,qword ptr [rsp+STACKTOP_OFFSET_GUEST_XCR0]
	cmp rax,qword ptr [rsp+STACKTOP_OFFSET_HOST_XCR0]
	je post_gh_xcr0_equal
	mov rdx,rax
	xor ecx,ecx
	shr rdx,32
	xsetbv
post_gh_xcr0_equal:
	// Restore all GPRs.
	popaq_fast STACKTOP_OFFSET_GUEST_GPR
	// Check if the VMCS is launched.
	btr dword ptr [rsp+STACKTOP_OFFSET_FLAGS],0
	jc launch_initial_vmcs
	vmresume
	jmp vmentry_failure
launch_initial_vmcs:
	vmlaunch
vmentry_failure:
	// Usually we won't be here, unless VM-Entry fails.
	// Call the special procedure to handle this situation.
	mov rcx,rsp
	setz dl
	setc al
	adc dl,al
	call nvc_vt_resume_failure
	// Try to resume again.
	jmp resume_guest
.seh_endproc

.global nvc_vt_host_nmi_handler
.seh_proc nvc_vt_host_nmi_handler
nvc_vt_host_nmi_handler:
	.seh_pushframe
	sub rsp,0xA8
	.seh_stackalloc 0xA8
	pushaq_fast 0x28
	.seh_endprologue
	call nvc_vt_inject_nmi_to_subverted_host
	popaq_fast 0x28
	add rsp,0xA8
	// Return from NMI but do not unblock NMIs.
	nmiret
.seh_endproc