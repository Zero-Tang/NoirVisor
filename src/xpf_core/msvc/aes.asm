; NoirVisor - Hardware-Accelerated Hypervisor solution
; 
; Copyright 2018-2024, Zero Tang. All rights reserved.
;
; This file is AES-NI Engine of NoirVisor for Secure Virtualization.
;
; This program is distributed in the hope that it will be successful, but
; without any warranty (no matter implied warranty of merchantability or
; fitness for a particular purpose, etc.).
;
; File location: ./xpf_core/msvc/crc32.asm

; For reference, see https://github.com/kmcallister/aesni-examples

.code

noir_aes128_key_combine macro dest_reg,keygen_assist,fw0

	pshufd keygen_assist,keygen_assist,011111111b
	shufps fw0,dest_reg,000010000b
	pxor dest_reg,fw0
	shufps fw0,dest_reg,010001100b
	pxor dest_reg,fw0
	pxor dest_reg,keygen_assist

endm

noir_aes128_key_expand macro dest_reg,round_const,round_key,invert,fw0,keygen_assist

	aeskeygenassist keygen_assist,round_key,round_const
	noir_aes128_key_combine round_key,keygen_assist,fw0
	if invert
		aesimc dest_reg,round_key
	else
		movaps dest_reg,round_key
	endif

endm

noir_aes128_expand_key proc

	; Input Registers:
	; rcx: AES128 Key. Must-be aligned on 16-byte boundary.
	; rdx: Expand for Encryption.
	; r8: Expanded Keys.
	; Align the stack pointer for xmm saving
	push rbp
	mov rbp,rsp
	and rsp,0fffffffffffffff0h
	sub rsp,40h
	; Save xmm registers
	movaps xmmword ptr[rsp+00h],xmm0
	movaps xmmword ptr[rsp+10h],xmm1
	movaps xmmword ptr[rsp+20h],xmm2
	movaps xmmword ptr[rsp+30h],xmm3
	; Load the key
	movaps xmm0,xmmword ptr[rcx]
	test rdx,rdx
	jz expand_decryption
	noir_aes128_key_expand xmm3,1,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8],xmm3
	noir_aes128_key_expand xmm3,2,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+10h],xmm3
	noir_aes128_key_expand xmm3,4,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+20h],xmm3
	noir_aes128_key_expand xmm3,8,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+30h],xmm3
	noir_aes128_key_expand xmm3,16,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+40h],xmm3
	noir_aes128_key_expand xmm3,32,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+50h],xmm3
	noir_aes128_key_expand xmm3,64,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+60h],xmm3
	noir_aes128_key_expand xmm3,128,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+70h],xmm3
	noir_aes128_key_expand xmm3,27,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+80h],xmm3
	noir_aes128_key_expand xmm3,54,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+90h],xmm3
	jmp complete_expansion
expand_decryption:
	noir_aes128_key_expand xmm3,1,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8],xmm3
	noir_aes128_key_expand xmm3,2,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+10h],xmm3
	noir_aes128_key_expand xmm3,4,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+20h],xmm3
	noir_aes128_key_expand xmm3,8,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+30h],xmm3
	noir_aes128_key_expand xmm3,16,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+40h],xmm3
	noir_aes128_key_expand xmm3,32,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+50h],xmm3
	noir_aes128_key_expand xmm3,64,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+60h],xmm3
	noir_aes128_key_expand xmm3,128,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+70h],xmm3
	noir_aes128_key_expand xmm3,27,xmm0,1,xmm2,xmm1
	movaps xmmword ptr[r8+80h],xmm3
	noir_aes128_key_expand xmm3,54,xmm0,0,xmm2,xmm1
	movaps xmmword ptr[r8+90h],xmm3
complete_expansion:
	; Restore registers.
	movaps xmm0,xmmword ptr[rsp+00h]
	movaps xmm1,xmmword ptr[rsp+10h]
	movaps xmm2,xmmword ptr[rsp+20h]
	movaps xmm3,xmmword ptr[rsp+30h]
	mov rsp,rbp
	pop rbp
	ret

noir_aes128_expand_key endp

save_crypto_xmm macro

	; Align the stack for saving XMM registers...
	push rbp
	mov rbp,rsp
	and rsp,0fffffffffffffff0h
	sub rsp,0c0h
	; Save XMM registers...
	movaps xmmword ptr[rsp+000h],xmm0
	movaps xmmword ptr[rsp+010h],xmm1
	movaps xmmword ptr[rsp+020h],xmm2
	movaps xmmword ptr[rsp+030h],xmm3
	movaps xmmword ptr[rsp+040h],xmm4
	movaps xmmword ptr[rsp+050h],xmm5
	movaps xmmword ptr[rsp+060h],xmm6
	movaps xmmword ptr[rsp+070h],xmm7
	movaps xmmword ptr[rsp+080h],xmm8
	movaps xmmword ptr[rsp+090h],xmm9
	movaps xmmword ptr[rsp+0A0h],xmm10
	movaps xmmword ptr[rsp+0B0h],xmm11

endm

restore_crypto_xmm macro

	movaps xmm0,xmmword ptr[rsp+000h]
	movaps xmm1,xmmword ptr[rsp+010h]
	movaps xmm2,xmmword ptr[rsp+020h]
	movaps xmm3,xmmword ptr[rsp+030h]
	movaps xmm4,xmmword ptr[rsp+040h]
	movaps xmm5,xmmword ptr[rsp+050h]
	movaps xmm6,xmmword ptr[rsp+060h]
	movaps xmm7,xmmword ptr[rsp+070h]
	movaps xmm8,xmmword ptr[rsp+080h]
	movaps xmm9,xmmword ptr[rsp+090h]
	movaps xmm10,xmmword ptr[rsp+0A0h]
	movaps xmm11,xmmword ptr[rsp+0B0h]
	; Restore the stack and return.
	mov rsp,rbp
	pop rbp

endm

load_expanded_keys macro source

	movaps xmm1,[source+00h]
	movaps xmm2,[source+10h]
	movaps xmm3,[source+20h]
	movaps xmm4,[source+30h]
	movaps xmm5,[source+40h]
	movaps xmm6,[source+50h]
	movaps xmm7,[source+60h]
	movaps xmm8,[source+70h]
	movaps xmm9,[source+80h]
	movaps xmm10,[source+90h]

endm

noir_aes128_encrypt_pages proc

	;  Input Registers:
	; rcx: Page Base
	; rdx: Expanded Keys
	; r8: Number of Pages
	; r9: The key
	shl r8,12	; Get the size of encryption
	xor eax,eax	; Offset to base.
	; Save XMM registers...
	save_crypto_xmm
	; Load Keys...
	load_expanded_keys rdx
	movaps xmm11,xmmword ptr[r9]
	; Perform Encryption...
encrypt_loop:
	; Load a 16-byte block
	movaps xmm0,xmmword ptr[rcx+rax]
	; Encrypt the block. Note that AES-128 takes 10 rounds.
	pxor xmm0,xmm11
	aesenc xmm0,xmm1
	aesenc xmm0,xmm2
	aesenc xmm0,xmm3
	aesenc xmm0,xmm4
	aesenc xmm0,xmm5
	aesenc xmm0,xmm6
	aesenc xmm0,xmm7
	aesenc xmm0,xmm8
	aesenc xmm0,xmm9
	aesenclast xmm0,xmm10
	; Store the ciphertext
	movaps xmmword ptr[rcx+rax],xmm0
	; Increment the counter.
	add rax,10h
	cmp rax,r8
	jne encrypt_loop
	; Restore XMM registers...
	restore_crypto_xmm
	ret

noir_aes128_encrypt_pages endp

noir_aes128_decrypt_pages proc

	;  Input Registers:
	; rcx: Page Base
	; rdx: Expanded Keys
	; r8: Number of Pages
	; r9: The key
	shl r8,12	; Get the size of decryption
	xor eax,eax	; Offset to base.
	; Save XMM registers...
	save_crypto_xmm
	; Load Keys...
	load_expanded_keys rdx
	movaps xmm11,xmmword ptr[r9]
	; Perform Decryption...
decrypt_loop:
	; Load a 16-byte block
	movaps xmm0,xmmword ptr[rcx+rax]
	; Decrypt the block. Note that AES-128 takes 10 rounds.
	pxor xmm0,xmm10
	aesdec xmm0,xmm9
	aesdec xmm0,xmm8
	aesdec xmm0,xmm7
	aesdec xmm0,xmm6
	aesdec xmm0,xmm5
	aesdec xmm0,xmm4
	aesdec xmm0,xmm3
	aesdec xmm0,xmm2
	aesdec xmm0,xmm1
	aesdeclast xmm0,xmm11
	; Store the plaintext
	movaps xmmword ptr[rcx+rax],xmm0
	; Increment the counter.
	add rax,10h
	cmp rax,r8
	jne decrypt_loop
	; Restore XMM registers...
	restore_crypto_xmm
	ret

noir_aes128_decrypt_pages endp

end