; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2019, Zero Tang. All rights reserved.
;
; This file saves processor states for 64-bit processors.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/kpcr64.asm

.code

noir_get_segment_attributes proc

	and rdx,0fffffffffffffff8h
	add rcx,rdx
	mov ax,word ptr[rcx+5]
	and ax,0f0ffh
	ret

noir_get_segment_attributes endp

noir_save_processor_state proc

	; Function start. Initialize shadow space on stack.
	sub rsp,20h
	push rbx
	mov rbx,rcx

	; Initialize the Structure with zero.
	push rdi
	cld
	mov rdi,rcx
	mov rcx,2ch
	xor rax,rax
	rep stosq
	pop rdi
	
	; Save cs,ds,es,fs,gs,ss Selectors
	mov word ptr[rbx],cs
	mov word ptr[rbx+10h],ds
	mov word ptr[rbx+20h],es
	mov word ptr[rbx+30h],fs
	mov word ptr[rbx+40h],gs
	mov word ptr[rbx+50h],ss

	; Save cs,ds,es,fs,gs,ss Limits
	lsl eax,word ptr[rbx]
	mov dword ptr[rbx+04h],eax
	lsl eax,word ptr[rbx+10h]
	mov dword ptr[rbx+14h],eax
	lsl eax,word ptr[rbx+20h]
	mov dword ptr[rbx+24h],eax
	lsl eax,word ptr[rbx+30h]
	mov dword ptr[rbx+34h],eax
	lsl eax,word ptr[rbx+40h]
	mov dword ptr[rbx+44h],eax
	lsl eax,word ptr[rbx+50h]
	mov dword ptr[rbx+54h],eax

	; Save Task Register State
	str word ptr[rbx+60h]
	lsl eax,word ptr[rbx+60h]
	mov dword ptr[rbx+64h],eax
	mov rax,qword ptr gs:[8h]
	mov qword ptr[rbx+68h],rax

	; Save Global Descriptor Table Register
	sgdt fword ptr[rbx+76h]
	shr dword ptr[rbx+74h],16

	; Save Interrupt Descriptor Table Register
	sidt fword ptr[rbx+86h]
	shr dword ptr[rbx+84h],16

	; Save Segment Attributes - CS
	mov rcx,qword ptr[rbx+78h]
	mov dx,word ptr[rbx]
	call noir_get_segment_attributes
	mov word ptr[rbx+2h],ax

	; Save Segment Attributes - DS
	mov rcx,qword ptr[rbx+78h]
	mov dx,word ptr[rbx+10h]
	call noir_get_segment_attributes
	mov word ptr[rbx+12h],ax

	; Save Segment Attributes - ES
	mov rcx,qword ptr[rbx+78h]
	mov dx,word ptr[rbx+20h]
	call noir_get_segment_attributes
	mov word ptr[rbx+22h],ax

	; Save Segment Attributes - FS
	mov rcx,qword ptr[rbx+78h]
	mov dx,word ptr[rbx+30h]
	call noir_get_segment_attributes
	mov word ptr[rbx+32h],ax

	; Save Segment Attributes - GS
	mov rcx,qword ptr[rbx+78h]
	mov dx,word ptr[rbx+40h]
	call noir_get_segment_attributes
	mov word ptr[rbx+42h],ax

	; Save Segment Attributes - SS
	mov rcx,qword ptr[rbx+78h]
	mov dx,word ptr[rbx+50h]
	call noir_get_segment_attributes
	mov word ptr[rbx+52h],ax

	; Save Segment Attributes - TR
	mov rcx,qword ptr[rbx+78h]
	mov dx,word ptr[rbx+60h]
	call noir_get_segment_attributes
	mov word ptr[rbx+62h],ax

	; Save LDT Register Selector
	sldt word ptr[rbx+90h]

	; Save Control Registers
	mov rax,cr0
	mov qword ptr[rbx+0a0h],rax
	mov rax,cr2
	mov qword ptr[rbx+0a8h],rax
	mov rax,cr3
	mov qword ptr[rbx+0b0h],rax
	mov rax,cr4
	mov qword ptr[rbx+0b8h],rax
	mov rax,cr8
	mov qword ptr[rbx+0c0h],rax

	; Save Debug Registers
	mov rax,dr0
	mov qword ptr[rbx+0c8h],rax
	mov rax,dr1
	mov qword ptr[rbx+0d0h],rax
	mov rax,dr2
	mov qword ptr[rbx+0d8h],rax
	mov rax,dr3
	mov qword ptr[rbx+0e0h],rax
	mov rax,dr6
	mov qword ptr[rbx+0e8h],rax
	mov rax,dr7
	mov qword ptr[rbx+0f0h],rax

	; Save Model Specific Registers
	; Save SysEnter_CS
	mov ecx,174h
	rdmsr
	mov dword ptr[rbx+0f8h],eax
	mov dword ptr[rbx+0fch],edx

	; Save SysEnter_ESP
	inc ecx
	rdmsr
	mov dword ptr[rbx+100h],eax
	mov dword ptr[rbx+104h],edx

	; Save SysEnter_EIP
	inc ecx
	rdmsr
	mov dword ptr[rbx+108h],eax
	mov dword ptr[rbx+10ch],edx

	; Save Debug Control MSR
	mov ecx,1d9h
	rdmsr
	mov dword ptr[rbx+110h],eax
	mov dword ptr[rbx+114h],edx

	; Save PAT
	mov ecx,277h
	rdmsr
	mov dword ptr[rbx+118h],eax
	mov dword ptr[rbx+11ch],edx

	; Save EFER
	mov ecx,0c0000080h
	rdmsr
	mov dword ptr[rbx+120h],eax
	mov dword ptr[rbx+124h],edx

	; Save STAR
	inc ecx
	rdmsr
	mov dword ptr[rbx+128h],eax
	mov dword ptr[rbx+12ch],edx

	; Save LSTAR
	inc ecx
	rdmsr
	mov dword ptr[rbx+130h],eax
	mov dword ptr[rbx+134h],edx

	; Save CSTAR
	inc ecx
	rdmsr
	mov dword ptr[rbx+138h],eax
	mov dword ptr[rbx+13ch],edx

	; Save SFMASK
	inc ecx
	rdmsr
	mov dword ptr[rbx+144h],edx
	mov dword ptr[rbx+140h],eax

	; Save FS Base
	mov ecx,0c0000100h
	rdmsr
	shl rdx,32
	or rdx,rax
	mov qword ptr[rbx+148h],rdx	; Save to MSR-State Area
	mov qword ptr[rbx+38h],rdx	; Save to Segment State Area

	; Save GS Base
	inc ecx
	rdmsr
	shl rdx,32
	or rdx,rax
	mov qword ptr[rbx+150h],rdx	; Save to MSR-State Area
	mov qword ptr[rbx+48h],rdx	; Save to Segment State Area

	; Save GS Swap
	inc ecx
	rdmsr
	mov dword ptr[rbx+158h],eax
	mov dword ptr[rbx+15ch],edx

	; MSR Saving is Over
	pop rbx
	; Function end. Finalize shadow space on stack.
	add rsp,20h
	ret

noir_save_processor_state endp

end