// NoirVisor CVM Scheduler Driver for Windows
// This file handles SEH for x64 Windows.

.text

.extern seh_excp_handler

.globl call_within_seh
.seh_proc call_within_seh
.seh_handler seh_excp_handler,@unwind,@except
call_within_seh:
	lea rax,[rip+seh_call_ra]
	push rax
	.seh_stackalloc 8
	.seh_endprologue
	xchg rcx,rdx
	call rdx
seh_call_ra:
	add rsp,8
	xor eax,eax
	ret
.seh_endproc
