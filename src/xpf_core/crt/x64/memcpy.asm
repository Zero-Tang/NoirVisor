; memcpy.asm - contains memcpy and memmove routines in NASM syntax
;
;		Copyright (c) Microsoft Corporation. All rights reserved. (Ported to NASM)
;
; Purpose:
;		memcpy() copies a source memory buffer to a destination buffer.
;		memmove() copies a source memory buffer to a destination buffer.
;		Overlapping buffers are treated specially, to avoid propagation.
;		AVX optimizations are removed to mitigate context saving/restoring.

bits 64
default rel

section .text

%define __FAVOR_ENFSTRG 1
%define KB 1024

extern __favor
extern __ImageBase

global memcpy
global memmove

; the code implementing memory copy via rep movsb (enhanced strings)
memcpy_repmovs:
	push     rdi
	push     rsi
	mov      rdi, rcx
	mov      rsi, rdx
	mov      rcx, r8
	rep      movsb
	pop      rsi
	pop      rdi
	ret

; Main memmove/memcpy routine implementation
memcpy:
memmove:
	mov      rax, rcx                                        ; pre-set the return value
	lea      r10, [rel __ImageBase]                          ; pre-set the image base to be used in table based branching
	cmp      r8, 15                                          ; dispatch to code handling block sizes of 16 bytes or more
	ja       MoveAbove15

; move blocks of less than 16 bytes in length
	align    16
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	and      r8, 0Fh
%endif
	mov      r9d, [r10 + r8*4 + MoveSmall wrt ..imagebase]   ; load the appropriate code label RVA from the table
	add      r9, r10                                         ; convert the RVA into the VA
	jmp      r9

MoveSmall0:
	ret

	align    16
MoveSmall15:
	mov      r8, [rdx]                                       ; handle 15 bytes (8+4+2+1)
	mov      ecx, [rdx + 8]
	movzx    r9d, word [rdx + 12]
	movzx    r10d, byte [rdx + 14]
	mov      [rax], r8
	mov      [rax + 8], ecx
	mov      [rax + 12], r9w
	mov      [rax + 14], r10b
	ret
MoveSmall11:
	mov      r8, [rdx]                                       ; handle 11 bytes (8+2+1)
	movzx    ecx, word [rdx + 8]
	movzx    r9d, byte [rdx + 10]
	mov      [rax], r8
	mov      [rax + 8], cx
	mov      [rax + 10], r9b
	ret
MoveSmall2:
	movzx    ecx, word [rdx]                                 ; handle 2 bytes (2)
	mov      [rax], cx
	ret

	align    16
MoveSmall7:
	mov      ecx, [rdx]                                      ; handle 7 bytes (4+2+1)
	movzx    r8d, word [rdx + 4]
	movzx    r9d, byte [rdx + 6]
	mov      [rax], ecx
	mov      [rax + 4], r8w
	mov      [rax + 6], r9b
	ret
MoveSmall14:
	mov      r8, [rdx]                                       ; handle 14 bytes (8+4+2)
	mov      ecx, [rdx + 8]
	movzx    r9d, word [rdx + 12]
	mov      [rax], r8
	mov      [rax + 8], ecx
	mov      [rax + 12], r9w
	ret
MoveSmall3:
	movzx    ecx, word [rdx]                                 ; handle 3 bytes (2+1)
	movzx    r8d, byte [rdx + 2]
	mov      [rax], cx
	mov      [rax + 2], r8b
	ret

	align    16
MoveSmall13:
	mov      r8, [rdx]                                       ; handle 13 bytes (8+4+1)
	mov      ecx, [rdx + 8]
	movzx    r9d, byte [rdx + 12]
	mov      [rax], r8
	mov      [rax + 8], ecx
	mov      [rax + 12], r9b
	ret
MoveSmall10:
	mov      r8, [rdx]                                       ; handle 10 bytes (8+2)
	movzx    ecx, word [rdx + 8]
	mov      [rax], r8
	mov      [rax + 8], cx
	ret
MoveSmall9:
	mov      r8, [rdx]                                       ; handle 9 bytes (8+1)
	movzx    ecx, byte [rdx + 8]
	mov      [rax], r8
	mov      [rax + 8], cl
	ret
MoveSmall12:
	mov      r8, [rdx]                                       ; handle 12 bytes (8+4)
	mov      ecx, [rdx + 8]
	mov      [rax], r8
	mov      [rax + 8], ecx
	ret

	align    16
MoveSmall6:
	mov      ecx, [rdx]                                      ; handle 6 bytes (4+2)
	movzx    r8d, word [rdx + 4]
	mov      [rax], ecx
	mov      [rax + 4], r8w
	ret
MoveSmall5:
	mov      ecx, [rdx]                                      ; handle 5 bytes (4+1)
	movzx    r8d, byte [rdx + 4]
	mov      [rax], ecx
	mov      [rax + 4], r8b
	ret
MoveSmall8:
	mov      rcx, [rdx]                                      ; handle 8 bytes (8)
	mov      [rax], rcx
	ret
MoveSmall1:
	movzx    ecx, byte [rdx]                                 ; handle 1 byte (1)
	mov      [rax], cl
	ret
MoveSmall4:
	mov      ecx, [rdx]                                      ; handle 4 bytes (4)
	mov      [rax], ecx
	ret

; move blocks of 16 bytes or more in length
	align    16
MoveAbove15:
	cmp      r8, 32                                          ; dispatch to code handling blocks over 32 bytes
	ja       MoveAbove32

	movdqu   xmm1, [rdx]
	movdqu   xmm2, [rdx + r8 - 16]
	movdqu   [rcx], xmm1
	movdqu   [rcx + r8 - 16], xmm2
	ret

; move blocks of 32 bytes or more in length
MoveAbove32:
	lea      r9, [rdx + r8]                                  ; check if the destination block starts in the middle of the source block
	cmp      rcx, rdx
	cmovbe   r9, rcx
	cmp      rcx, r9
	jb       CopyDown                                        ; if it overlaps (dst > src), use copy down implementation

; SSE based implementation (AVX logic removed)
NoAVX:
	%define __SSE_LEN_BIT 4
	%define __SSE_STEP_LEN (1 << __SSE_LEN_BIT)
	%define __SSE_LOOP_LEN (__SSE_STEP_LEN * 8)
	%define __FAST_STRING_SSE_THRESHOLD (2 * KB)

	cmp      r8, __FAST_STRING_SSE_THRESHOLD
	jbe      MoveWithXMM

	test     byte [rel __favor], (1 << __FAVOR_ENFSTRG)
	jnz      memcpy_repmovs

MoveWithXMM:
	movdqu   xmm0, [rdx]                                     ; save first 16 bytes for overlap/alignment safety
	movdqu   xmm5, [rdx + r8 - __SSE_STEP_LEN]               ; save last 16 bytes
	cmp      r8, __SSE_LOOP_LEN
	jbe      MovUpTo128WithXMM

	mov      r9, rcx
	and      r9, __SSE_STEP_LEN - 1
	sub      r9, __SSE_STEP_LEN
	sub      rcx, r9
	sub      rdx, r9
	add      r8, r9
	cmp      r8, __SSE_LOOP_LEN
	jbe      MovUpTo128WithXMM

	align    16
XmmLoop:
	movdqu   xmm1, [rdx + __SSE_STEP_LEN*0]
	movdqu   xmm2, [rdx + __SSE_STEP_LEN*1]
	movdqu   xmm3, [rdx + __SSE_STEP_LEN*2]
	movdqu   xmm4, [rdx + __SSE_STEP_LEN*3]
	movdqa   [rcx + __SSE_STEP_LEN*0], xmm1
	movdqa   [rcx + __SSE_STEP_LEN*1], xmm2
	movdqa   [rcx + __SSE_STEP_LEN*2], xmm3
	movdqa   [rcx + __SSE_STEP_LEN*3], xmm4
	movdqu   xmm1, [rdx + __SSE_STEP_LEN*4]
	movdqu   xmm2, [rdx + __SSE_STEP_LEN*5]
	movdqu   xmm3, [rdx + __SSE_STEP_LEN*6]
	movdqu   xmm4, [rdx + __SSE_STEP_LEN*7]
	movdqa   [rcx + __SSE_STEP_LEN*4], xmm1
	movdqa   [rcx + __SSE_STEP_LEN*5], xmm2
	movdqa   [rcx + __SSE_STEP_LEN*6], xmm3
	movdqa   [rcx + __SSE_STEP_LEN*7], xmm4
	add      rcx, __SSE_LOOP_LEN
	add      rdx, __SSE_LOOP_LEN
	sub      r8, __SSE_LOOP_LEN
	cmp      r8, __SSE_LOOP_LEN
	jae      XmmLoop

MovUpTo128WithXMM:
	lea      r9, [r8 + __SSE_STEP_LEN - 1]
	and      r9, -__SSE_STEP_LEN
	mov      r11, r9
	shr      r11, __SSE_LEN_BIT
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	and      r11, 0Fh
%endif
	mov      r11d, [r10 + r11*4 + MoveSmallXmm wrt ..imagebase]
	add      r11, r10
	jmp      r11

Mov8XmmBlocks:
	movdqu   xmm1, [rdx + r9 - __SSE_STEP_LEN*8]
	movdqu   [rcx + r9 - __SSE_STEP_LEN*8], xmm1
Mov7XmmBlocks:
	movdqu   xmm1, [rdx + r9 - __SSE_STEP_LEN*7]
	movdqu   [rcx + r9 - __SSE_STEP_LEN*7], xmm1
Mov6XmmBlocks:
	movdqu   xmm1, [rdx + r9 - __SSE_STEP_LEN*6]
	movdqu   [rcx + r9 - __SSE_STEP_LEN*6], xmm1
Mov5XmmBlocks:
	movdqu   xmm1, [rdx + r9 - __SSE_STEP_LEN*5]
	movdqu   [rcx + r9 - __SSE_STEP_LEN*5], xmm1
Mov4XmmBlocks:
	movdqu   xmm1, [rdx + r9 - __SSE_STEP_LEN*4]
	movdqu   [rcx + r9 - __SSE_STEP_LEN*4], xmm1
Mov3XmmBlocks:
	movdqu   xmm1, [rdx + r9 - __SSE_STEP_LEN*3]
	movdqu   [rcx + r9 - __SSE_STEP_LEN*3], xmm1
Mov2XmmBlocks:
	movdqu   xmm1, [rdx + r9 - __SSE_STEP_LEN*2]
	movdqu   [rcx + r9 - __SSE_STEP_LEN*2], xmm1
Mov1XmmBlocks:
	movdqu   [rcx + r8 - __SSE_STEP_LEN*1], xmm5
Mov0XmmBlocks:
	movdqu   [rax], xmm0
	ret

; memmove: Copy Down implementation
	align    16
CopyDown:
	movups   xmm2, [rdx]
	sub      rdx, rcx
	add      rcx, r8
	movups   xmm0, [rcx + rdx - 16]
	sub      rcx, 16
	sub      r8, 16

	test     cl, 0Fh
	jz       XmmMovLargeTest

XmmMovAlign:
	mov      r9, rcx
	and      rcx, -16
	movups   xmm1, xmm0
	movups   xmm0, [rcx + rdx]
	movups   [r9], xmm1
	mov      r8, rcx
	sub      r8, rax

XmmMovLargeTest:
	mov      r9, r8
	shr      r9, 7
	jz       XmmMovSmallTest
	movaps   [rcx], xmm0
	jmp      XmmMovLargeInner

	align    16
XmmMovLargeOuter:
	movaps   [rcx + 128 - 112], xmm0
	movaps   [rcx + 128 - 128], xmm1
XmmMovLargeInner:
	movups   xmm0, [rcx + rdx - 16]
	movups   xmm1, [rcx + rdx - 32]
	sub      rcx, 128
	movaps   [rcx + 128 - 16], xmm0
	movaps   [rcx + 128 - 32], xmm1
	movups   xmm0, [rcx + rdx + 128 - 48]
	movups   xmm1, [rcx + rdx + 128 - 64]
	dec      r9
	movaps   [rcx + 128 - 48], xmm0
	movaps   [rcx + 128 - 64], xmm1
	movups   xmm0, [rcx + rdx + 128 - 80]
	movups   xmm1, [rcx + rdx + 128 - 96]
	movaps   [rcx + 128 - 80], xmm0
	movaps   [rcx + 128 - 96], xmm1
	movups   xmm0, [rcx + rdx + 128 - 112]
	movups   xmm1, [rcx + rdx + 128 - 128]
	jnz      XmmMovLargeOuter

	movaps   [rcx + 128 - 112], xmm0
	and      r8, 7Fh
	movaps   xmm0, xmm1

XmmMovSmallTest:
	mov      r9, r8
	shr      r9, 4
	jz       XmmMovTrailing

	align    16
XmmMovSmallLoop:
	movups   [rcx], xmm0
	sub      rcx, 16
	movups   xmm0, [rcx + rdx]
	dec      r9
	jnz      XmmMovSmallLoop

XmmMovTrailing:
	and      r8, 0Fh
	jz       XmmMovReturn
	movups   [rax], xmm2

XmmMovReturn:
	movups   [rcx], xmm0
	ret

section .rdata

MoveSmall:
	dd MoveSmall0 wrt ..imagebase
	dd MoveSmall1 wrt ..imagebase
	dd MoveSmall2 wrt ..imagebase
	dd MoveSmall3 wrt ..imagebase
	dd MoveSmall4 wrt ..imagebase
	dd MoveSmall5 wrt ..imagebase
	dd MoveSmall6 wrt ..imagebase
	dd MoveSmall7 wrt ..imagebase
	dd MoveSmall8 wrt ..imagebase
	dd MoveSmall9 wrt ..imagebase
	dd MoveSmall10 wrt ..imagebase
	dd MoveSmall11 wrt ..imagebase
	dd MoveSmall12 wrt ..imagebase
	dd MoveSmall13 wrt ..imagebase
	dd MoveSmall14 wrt ..imagebase
	dd MoveSmall15 wrt ..imagebase
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	dd 0, 0, 0, 0 ; padding
%endif

MoveSmallXmm:
	dd Mov0XmmBlocks wrt ..imagebase
	dd Mov1XmmBlocks wrt ..imagebase
	dd Mov2XmmBlocks wrt ..imagebase
	dd Mov3XmmBlocks wrt ..imagebase
	dd Mov4XmmBlocks wrt ..imagebase
	dd Mov5XmmBlocks wrt ..imagebase
	dd Mov6XmmBlocks wrt ..imagebase
	dd Mov7XmmBlocks wrt ..imagebase
	dd Mov8XmmBlocks wrt ..imagebase
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	dd 0, 0, 0, 0, 0, 0, 0
%endif