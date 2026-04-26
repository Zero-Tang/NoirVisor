; memset.nasm - contains memset routine in NASM syntax
;
;		Copyright (c) Microsoft Corporation. All rights reserved. (Ported to NASM)
;
; Purpose:
;		sets all bytes in a memory block to a value.
;		AVX optimizations are removed to mitigate context saving/restoring.

bits 64
default rel

section .text

%define __FAVOR_ENFSTRG 1

extern __favor
extern __ImageBase
extern __memset_fast_string_threshold

global memset

; the code implementing memory set via rep stosb (enhanced strings)
memset_repstos:
	push     rdi
	mov      eax, edx
	mov      rdi, rcx
	mov      rcx, r8
	rep      stosb
	mov      rax, r9
	pop      rdi
	ret

; Main memset routine implementation
memset:
	mov      rax, rcx                                        ; pre-set the return value
	mov      r9, rcx                                         ; save the block address for setting the return value
	lea      r10, [rel __ImageBase]                          ; pre-set the image base
	movzx    edx, dl                                         ; set the byte fill pattern
	mov      r11, 0x0101010101010101                         ; set replication mask
	imul     r11, rdx                                        ; expand into 64-bit fill value
	movq     xmm0, r11                                       ; expand into 64-bit XMM fill value
	cmp      r8, 15                                          ; dispatch to code handling block sizes of 16 bytes or more
	ja       SetAbove15

; set blocks of less than 16 bytes in length
	align    16
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	and      r8, 0Fh
%endif
	add      rcx, r8
	mov      r9d, [r10 + r8*4 + SetSmall wrt ..imagebase]    ; load image-relative offset from table
	add      r9, r10                                         ; convert to VA
	jmp      r9

SetSmall15:
	mov      [rcx - 15], r11                                 ; handle 15 bytes (8+4+2+1)
SetSmall7:
	mov      [rcx - 7], r11d                                 ; handle 7 bytes (4+2+1)
SetSmall3:
	mov      [rcx - 3], r11w                                 ; handle 3 bytes (2+1)
SetSmall1:
	mov      [rcx - 1], r11b                                 ; handle 1 byte
SetSmall0:
	ret

SetSmall14:
	mov      [rcx - 14], r11                                 ; handle 14 bytes (8+4+2)
SetSmall6:
	mov      [rcx - 6], r11d                                 ; handle 6 bytes (4+2)
SetSmall2:
	mov      [rcx - 2], r11w                                 ; handle 2 bytes (2)
	ret

	align    16
SetSmall13:
	mov      [rcx - 13], r11                                 ; handle 13 bytes (8+4+1)
SetSmall5:
	mov      [rcx - 5], r11d                                 ; handle 5 bytes (4+1)
	mov      [rcx - 1], r11b
	ret

	align    16
SetSmall12:
	mov      [rcx - 12], r11                                 ; handle 12 bytes (8+4)
SetSmall4:
	mov      [rcx - 4], r11d                                 ; handle 4 bytes (4)
	ret

SetSmall11:
	mov      [rcx - 11], r11                                 ; handle 11 bytes (8+2+1)
	mov      [rcx - 3], r11w
	mov      [rcx - 1], r11b
	ret

SetSmall9:
	mov      [rcx - 9], r11                                  ; handle 9 bytes (8+1)
	mov      [rcx - 1], r11b
	ret

	align    16
SetSmall10:
	mov      [rcx - 10], r11                                 ; handle 10 bytes (8+2)
	mov      [rcx - 2], r11w
	ret

SetSmall8:
	mov      [rcx - 8], r11                                  ; handle 8 bytes (8)
	ret

; set blocks of 16 bytes or more in length
	align    16
SetAbove15:
	punpcklqdq xmm0, xmm0                                    ; expand into 128-bit fill pattern
	cmp      r8, 32
	ja       SetAbove32

	movdqu   [rcx], xmm0
	movdqu   [rcx + r8 - 16], xmm0
	ret

; set blocks of 32 bytes or more (AVX logic removed, goes straight to NoAVX equivalent)
SetAbove32:
	%define __SSE_LEN_BIT 4
	%define __SSE_STEP_LEN (1 << __SSE_LEN_BIT)
	%define __SSE_LOOP_LEN (__SSE_STEP_LEN * 8)

	cmp      r8, [rel __memset_fast_string_threshold]
	jbe      SetWithXMM

	test     byte [rel __favor], (1 << __FAVOR_ENFSTRG)
	jnz      memset_repstos

SetWithXMM:
	mov      r9, rcx
	and      r9, __SSE_STEP_LEN - 1
	sub      r9, __SSE_STEP_LEN
	sub      rcx, r9
	sub      rdx, r9
	add      r8, r9
	cmp      r8, __SSE_LOOP_LEN
	jbe      SetUpTo128WithXMM

	align    16
XmmLoop:
	movdqa   [rcx + __SSE_STEP_LEN*0], xmm0
	movdqa   [rcx + __SSE_STEP_LEN*1], xmm0
	movdqa   [rcx + __SSE_STEP_LEN*2], xmm0
	movdqa   [rcx + __SSE_STEP_LEN*3], xmm0
	movdqa   [rcx + __SSE_STEP_LEN*4], xmm0
	movdqa   [rcx + __SSE_STEP_LEN*5], xmm0
	movdqa   [rcx + __SSE_STEP_LEN*6], xmm0
	movdqa   [rcx + __SSE_STEP_LEN*7], xmm0
	add      rcx, __SSE_LOOP_LEN
	sub      r8, __SSE_LOOP_LEN
	cmp      r8, __SSE_LOOP_LEN
	jae      XmmLoop

SetUpTo128WithXMM:
	lea      r9, [r8 + __SSE_STEP_LEN - 1]
	and      r9, -__SSE_STEP_LEN
	mov      r11, r9
	shr      r11, __SSE_LEN_BIT
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	and      r11, 0Fh
%endif
	mov      r11d, [r10 + r11*4 + SetSmallXmm wrt ..imagebase]
	add      r11, r10
	jmp      r11

Set8XmmBlocks:
	movdqu   [rcx + r9 - __SSE_STEP_LEN*8], xmm0
Set7XmmBlocks:
	movdqu   [rcx + r9 - __SSE_STEP_LEN*7], xmm0
Set6XmmBlocks:
	movdqu   [rcx + r9 - __SSE_STEP_LEN*6], xmm0
Set5XmmBlocks:
	movdqu   [rcx + r9 - __SSE_STEP_LEN*5], xmm0
Set4XmmBlocks:
	movdqu   [rcx + r9 - __SSE_STEP_LEN*4], xmm0
Set3XmmBlocks:
	movdqu   [rcx + r9 - __SSE_STEP_LEN*3], xmm0
Set2XmmBlocks:
	movdqu   [rcx + r9 - __SSE_STEP_LEN*2], xmm0
Set1XmmBlocks:
	movdqu   [rcx + r8 - __SSE_STEP_LEN*1], xmm0
Set0XmmBlocks:
	movdqu   [rax], xmm0
	ret

section .rdata

SetSmall:
	dd SetSmall0 wrt ..imagebase
	dd SetSmall1 wrt ..imagebase
	dd SetSmall2 wrt ..imagebase
	dd SetSmall3 wrt ..imagebase
	dd SetSmall4 wrt ..imagebase
	dd SetSmall5 wrt ..imagebase
	dd SetSmall6 wrt ..imagebase
	dd SetSmall7 wrt ..imagebase
	dd SetSmall8 wrt ..imagebase
	dd SetSmall9 wrt ..imagebase
	dd SetSmall10 wrt ..imagebase
	dd SetSmall11 wrt ..imagebase
	dd SetSmall12 wrt ..imagebase
	dd SetSmall13 wrt ..imagebase
	dd SetSmall14 wrt ..imagebase
	dd SetSmall15 wrt ..imagebase
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	dd 0, 0, 0, 0 ; padding
%endif

SetSmallXmm:
	dd Set0XmmBlocks wrt ..imagebase
	dd Set1XmmBlocks wrt ..imagebase
	dd Set2XmmBlocks wrt ..imagebase
	dd Set3XmmBlocks wrt ..imagebase
	dd Set4XmmBlocks wrt ..imagebase
	dd Set5XmmBlocks wrt ..imagebase
	dd Set6XmmBlocks wrt ..imagebase
	dd Set7XmmBlocks wrt ..imagebase
	dd Set8XmmBlocks wrt ..imagebase
%ifdef _VCRUNTIME_BUILD_QSPECTRE
	dd 0, 0, 0, 0, 0, 0, 0
%endif