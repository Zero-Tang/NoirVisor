; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2021, Zero Tang. All rights reserved.
;
; This file saves processor states.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/windows/kpcr.asm

ifdef _ia32
.686p
.model flat,stdcall
endif

.code

ifdef _amd64

noir_xmmsave proc

	movaps xmmword ptr [rcx],xmm0
	movaps xmmword ptr [rcx+10h],xmm1
	movaps xmmword ptr [rcx+20h],xmm2
	movaps xmmword ptr [rcx+30h],xmm3
	movaps xmmword ptr [rcx+40h],xmm4
	movaps xmmword ptr [rcx+50h],xmm5
	movaps xmmword ptr [rcx+60h],xmm6
	movaps xmmword ptr [rcx+70h],xmm7
	movaps xmmword ptr [rcx+80h],xmm8
	movaps xmmword ptr [rcx+90h],xmm9
	movaps xmmword ptr [rcx+0A0h],xmm10
	movaps xmmword ptr [rcx+0B0h],xmm10
	movaps xmmword ptr [rcx+0C0h],xmm10
	movaps xmmword ptr [rcx+0D0h],xmm10
	movaps xmmword ptr [rcx+0E0h],xmm10
	movaps xmmword ptr [rcx+0F0h],xmm10
	ret

noir_xmmsave endp

noir_xmmrestore proc

	movaps xmm0,xmmword ptr[rcx]
	movaps xmm1,xmmword ptr[rcx+10h]
	movaps xmm2,xmmword ptr[rcx+20h]
	movaps xmm3,xmmword ptr[rcx+30h]
	movaps xmm4,xmmword ptr[rcx+40h]
	movaps xmm5,xmmword ptr[rcx+50h]
	movaps xmm6,xmmword ptr[rcx+60h]
	movaps xmm7,xmmword ptr[rcx+70h]
	movaps xmm8,xmmword ptr[rcx+80h]
	movaps xmm9,xmmword ptr[rcx+90h]
	movaps xmm10,xmmword ptr[rcx+0A0h]
	movaps xmm11,xmmword ptr[rcx+0B0h]
	movaps xmm12,xmmword ptr[rcx+0C0h]
	movaps xmm13,xmmword ptr[rcx+0D0h]
	movaps xmm14,xmmword ptr[rcx+0E0h]
	movaps xmm15,xmmword ptr[rcx+0F0h]
	ret

noir_xmmrestore endp

noir_ymmsave proc

	vmovaps ymmword ptr [rcx+000h],ymm0
	vmovaps ymmword ptr [rcx+020h],ymm1
	vmovaps ymmword ptr [rcx+040h],ymm2
	vmovaps ymmword ptr [rcx+060h],ymm3
	vmovaps ymmword ptr [rcx+080h],ymm4
	vmovaps ymmword ptr [rcx+0A0h],ymm5
	vmovaps ymmword ptr [rcx+0C0h],ymm6
	vmovaps ymmword ptr [rcx+0E0h],ymm7
	vmovaps ymmword ptr [rcx+100h],ymm8
	vmovaps ymmword ptr [rcx+120h],ymm9
	vmovaps ymmword ptr [rcx+140h],ymm10
	vmovaps ymmword ptr [rcx+160h],ymm11
	vmovaps ymmword ptr [rcx+180h],ymm12
	vmovaps ymmword ptr [rcx+1A0h],ymm13
	vmovaps ymmword ptr [rcx+1C0h],ymm14
	vmovaps ymmword ptr [rcx+1E0h],ymm15
	ret

noir_ymmsave endp

noir_ymmrestore proc

	vmovaps ymm0,ymmword ptr[rcx]
	vmovaps ymm1,ymmword ptr[rcx+020h]
	vmovaps ymm2,ymmword ptr[rcx+040h]
	vmovaps ymm3,ymmword ptr[rcx+060h]
	vmovaps ymm4,ymmword ptr[rcx+080h]
	vmovaps ymm5,ymmword ptr[rcx+0A0h]
	vmovaps ymm6,ymmword ptr[rcx+0C0h]
	vmovaps ymm7,ymmword ptr[rcx+0E0h]
	vmovaps ymm8,ymmword ptr[rcx+100h]
	vmovaps ymm9,ymmword ptr[rcx+120h]
	vmovaps ymm10,ymmword ptr[rcx+140h]
	vmovaps ymm11,ymmword ptr[rcx+160h]
	vmovaps ymm12,ymmword ptr[rcx+180h]
	vmovaps ymm13,ymmword ptr[rcx+1A0h]
	vmovaps ymm14,ymmword ptr[rcx+1C0h]
	vmovaps ymm15,ymmword ptr[rcx+1E0h]
	ret

noir_ymmrestore endp

noir_ffxsave proc

	fxsave [rcx]
	ret

noir_ffxsave endp

noir_ffxrestore proc

	fxrstor [rcx]
	ret

noir_ffxrestore endp

noir_fxsave proc

	fxsave [rcx]
	; Determine if FFXSR is enabled.
	cmp qword ptr[rcx+464],0
	jz non_ffxsr
	; FFXSR is enabled. Save XMMs manually.
	add rcx,160
	call noir_xmmsave
non_ffxsr:
	ret

noir_fxsave endp

noir_fxrestore proc

	fxrstor [rcx]
	; Determine if FFXSR is enabled.
	cmp qword ptr[rcx+464],0
	jz non_ffxsr
	; FFXSR is enabled. Restore XMMs manually.
	add rcx,160
	call noir_xmmrestore
non_ffxsr:
	ret

noir_fxrestore endp

noir_get_segment_attributes proc

	and rdx,0fff8h
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

noir_xsetbv proc

	mov eax,edx
	shr rdx,32
	xsetbv
	ret

noir_xsetbv endp

else

assume fs:nothing

noir_get_segment_attributes proc gdt_base:dword,selector:word

	mov ecx,dword ptr[gdt_base]
	mov dx,word ptr[selector]
	and dx,0fff8h
	add ecx,edx
	and ax,0f0ffh
	ret

noir_get_segment_attributes endp

noir_save_processor_state proc uses edi state:dword

	; Load Parameter and Initialize.
	mov edi,dword ptr[state]
	mov ecx,4ch
	xor eax,eax
	cld
	rep stosd

	; Save cs,ds,es,fs,gs,ss Selectors
	mov word ptr[edi],cs
	mov word ptr[edi+10h],ds
	mov word ptr[edi+20h],es
	mov word ptr[edi+30h],fs
	mov word ptr[edi+40h],gs
	mov word ptr[edi+50h],ss

	; Save cs,ds,es,fs,gs,ss Limits
	lsl eax,word ptr[edi]
	mov dword ptr[edi+04h],eax
	lsl eax,word ptr[edi+10h]
	mov dword ptr[edi+14h],eax
	lsl eax,word ptr[edi+20h]
	mov dword ptr[edi+24h],eax
	lsl eax,word ptr[edi+30h]
	mov dword ptr[edi+34h],eax
	lsl eax,word ptr[edi+40h]
	mov dword ptr[edi+44h],eax
	lsl eax,word ptr[edi+50h]
	mov dword ptr[edi+54h],eax

	; Save Task Register State
	str word ptr[edi+60h]
	lsl eax,word ptr[edi+60h]
	mov dword ptr[edi+64h],eax
	mov eax,dword ptr fs:[40h]
	mov dword ptr[edi+68h],eax

	; Save Global Descriptor Table Register
	sgdt fword ptr[edi+76h]
	shr dword ptr[edi+74h],16

	; Save Interrupt Descriptor Table Register
	sidt fword ptr[edi+86h]
	shr dword ptr[edi+84h],16

	; Save Segment Attributes - CS
	push cs
	push dword ptr[edi+78h]
	call noir_get_segment_attributes
	mov word ptr[edi+2h],ax

	; Save Segment Attributes - DS
	push ds
	push dword ptr[edi+78h]
	call noir_get_segment_attributes
	mov word ptr[edi+12h],ax

	; Save Segment Attributes - ES
	push es
	push dword ptr[edi+78h]
	call noir_get_segment_attributes
	mov word ptr[edi+22h],ax

	; Save Segment Attributes - FS
	push fs
	push dword ptr[edi+78h]
	call noir_get_segment_attributes
	mov word ptr[edi+32h],ax

	; Save Segment Attributes - GS
	push gs
	push dword ptr[edi+78h]
	call noir_get_segment_attributes
	mov word ptr[edi+42h],ax

	; Save Segment Attributes - SS
	push ss
	push dword ptr[edi+78h]
	call noir_get_segment_attributes
	mov word ptr[edi+52h],ax

	; Save Segment Attributes - TR
	push dword ptr[edi+60h]
	push dword ptr[edi+78h]
	call noir_get_segment_attributes
	mov word ptr[edi+62h],ax

	; Save LDT Register Selector
	sldt word ptr[edi+90h]

	; Save Control Registers
	mov eax,cr0
	mov dword ptr[edi+0a0h],eax
	mov eax,cr2
	mov dword ptr[edi+0a4h],eax
	mov eax,cr3
	mov dword ptr[edi+0a8h],eax
	mov eax,cr4
	mov dword ptr[edi+0ach],eax

	; Save Debug Registers
	mov eax,dr0
	mov dword ptr[edi+0b0h],eax
	mov eax,dr1
	mov dword ptr[edi+0b4h],eax
	mov eax,dr2
	mov dword ptr[edi+0b8h],eax
	mov eax,dr3
	mov dword ptr[edi+0bch],eax
	mov eax,dr6
	mov dword ptr[edi+0c0h],eax
	mov eax,dr7
	mov dword ptr[edi+0c4h],eax

	; Save Model-Specific Registers
	; Save SysEnter_CS
	mov ecx,174h
	rdmsr
	mov dword ptr[ebx+0c8h],eax
	mov dword ptr[ebx+0cch],edx

	; Save SysEnter_ESP
	inc ecx
	rdmsr
	mov dword ptr[ebx+0d0h],eax
	mov dword ptr[ebx+0d4h],edx

	; Save SysEnter_EIP
	inc ecx
	rdmsr
	mov dword ptr[ebx+0d8h],eax
	mov dword ptr[ebx+0dch],edx

	; Save Debug Control MSR
	mov ecx,1d9h
	rdmsr
	mov dword ptr[ebx+0e0h],eax
	mov dword ptr[ebx+0e4h],edx

	; Save PAT
	mov ecx,277h
	rdmsr
	mov dword ptr[ebx+0e8h],eax
	mov dword ptr[ebx+0ech],edx

	; Save EFER
	mov ecx,0c0000080h
	rdmsr
	mov dword ptr[ebx+0f0h],eax
	mov dword ptr[ebx+0f4h],edx

	; Save STAR
	inc ecx
	rdmsr
	mov dword ptr[ebx+0f8h],eax
	mov dword ptr[ebx+0fch],edx

	; Save LSTAR
	inc ecx
	rdmsr
	mov dword ptr[ebx+100h],eax
	mov dword ptr[ebx+104h],edx

	; Save CSTAR
	inc ecx
	rdmsr
	mov dword ptr[ebx+108h],eax
	mov dword ptr[ebx+10ch],edx

	; Save SFMASK
	inc ecx
	rdmsr
	mov dword ptr[ebx+110h],eax
	mov dword ptr[ebx+114h],edx

	; Save FS Base
	mov ecx,0c0000100h
	rdmsr
	; Save to MSR State
	mov dword ptr[edi+118h],eax
	mov dword ptr[edi+11ch],edx
	; Save to Segment State
	mov dword ptr[ebx+38h],eax
	mov dword ptr[edi+3ch],edx

	; Save GS Base
	inc ecx
	rdmsr
	; Save to MSR State
	mov dword ptr[edi+120h],eax
	mov dword ptr[edi+124h],edx
	; Save to Segment State
	mov dword ptr[ebx+48h],eax
	mov dword ptr[edi+4ch],edx

	; Save GS Swap
	inc ecx
	rdmsr
	mov dword ptr[ebx+128h],eax
	mov dword ptr[ebx+12ch],edx

	; MSR Saving is Over
	ret

noir_save_processor_state endp

noir_xsetbv proc xcr_id:dword,val_lo:dword,val_hi:dword

	mov eax,dword ptr [val_lo]
	mov edx,dword ptr [val_hi]
	mov ecx,dword ptr [xcr_id]
	xsetbv
	ret

noir_xsetbv endp

endif

end