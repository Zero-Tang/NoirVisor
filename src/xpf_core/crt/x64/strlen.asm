;
;strlen.nasm - contains strlen() routine in NASM syntax
;
;		Copyright (c) Microsoft Corporation. All rights reserved. (Ported to NASM)
;
;Purpose:
;		strlen returns the length of a null-terminated string,
;		not including the null byte itself.

bits 64
default rel

section .text

global strlen

strlen:
	mov   rax, rcx
	neg   rcx          ; save negative of original pointer for length calculation
	test  rax, 7
	jz    main_loop_entry

byte_loop_begin:
	mov   dl, [rax]
	inc   rax
	test  dl, dl
	jz    return_byte_7
	test  al, 7
	jnz   byte_loop_begin

main_loop_entry:
	mov   r8, 0x7efefefefefefeff
	mov   r11, 0x8101010101010100

main_loop_begin:
	mov   rdx, [rax]

	mov   r9,  r8
	add   rax, 8
	add   r9,  rdx
	not   rdx
	xor   rdx, r9
	and   rdx, r11
	je    main_loop_begin

main_loop_end:
	mov   rdx, [rax - 8]

	test  dl, dl
	jz    return_byte_0
	test  dh, dh
	jz    return_byte_1
	shr   rdx, 16
	test  dl, dl
	jz    return_byte_2
	test  dh, dh
	jz    return_byte_3
	shr   rdx, 16
	test  dl, dl
	jz    return_byte_4
	test  dh, dh
	jz    return_byte_5
	shr   edx, 16
	test  dl, dl
	jz    return_byte_6
	test  dh, dh
	jnz   main_loop_begin

return_byte_7:
	lea   rax, [rax + rcx - 1]
	ret
return_byte_6:
	lea   rax, [rax + rcx - 2]
	ret
return_byte_5:
	lea   rax, [rax + rcx - 3]
	ret
return_byte_4:
	lea   rax, [rax + rcx - 4]
	ret
return_byte_3:
	lea   rax, [rax + rcx - 5]
	ret
return_byte_2:
	lea   rax, [rax + rcx - 6]
	ret
return_byte_1:
	lea   rax, [rax + rcx - 7]
	ret
return_byte_0:
	lea   rax, [rax + rcx - 8]
	ret
