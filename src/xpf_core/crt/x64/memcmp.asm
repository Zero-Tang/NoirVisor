;***
;memcmp.nasm - compare two blocks of memory in NASM syntax
;
;       Copyright (c) Microsoft Corporation. All rights reserved. (Ported to NASM)
;
;Purpose:
;       defines memcmp() - compare two memory blocks lexically and
;       find their order.
;
;*******************************************************************************

bits 64
default rel

section .text

global memcmp

;***
;int memcmp(buf1, buf2, count) - compare memory for lexical order
;
;Entry:
;       rcx - buf1 (pointer to memory section 1)
;       rdx - buf2 (pointer to memory section 2)
;       r8  - count (length of sections to compare)
;
;Exit:
;       returns -1 if buf1 < buf2
;       returns  0 if buf1 == buf2
;       returns +1 if buf1 > buf2
;*******************************************************************************

memcmp:
    sub     rdx, rcx                ; compute source address difference
    cmp     r8, 8                   ; check if at least 8 bytes to compare
    jb      short mcmp30             ; if less than 8, handle byte by byte
    test    cl, 7                   ; test if buf1 is quadword aligned
    jz      short mcmp20            ; if aligned, skip alignment loop

; Align buf1 to an 8-byte boundary
    align   16
mcmp10:
    mov     al, [rcx]               ; compare single byte
    cmp     al, [rcx + rdx]         ;
    jne     short mcmp_not_equal    ; mismatch found
    inc     rcx                     ; increment buf1 address
    dec     r8                      ; decrement count
    test    cl, 7                   ; test alignment
    jne     short mcmp10            ; repeat until aligned

; Check for 8-byte blocks
mcmp20:
    mov     r9, r8                  ; copy count
    shr     r9, 3                   ; compute number of 8-byte blocks
    jnz     short mcmp50            ; if non-zero, process blocks

; Compare remaining bytes
mcmp30:
    test    r8, r8                  ; any bytes remaining?
    jz      short mcmp_equal        ; finished with no mismatch

mcmp40:
    mov     al, [rcx]               ; compare single byte
    cmp     al, [rcx + rdx]         ;
    jne     short mcmp_not_equal    ; mismatch found
    inc     rcx                     ; advance
    dec     r8                      ; decrement
    jnz     short mcmp40            ; repeat if bytes left

mcmp_equal:
    xor     rax, rax                ; return 0
    ret

mcmp_not_equal:
    sbb     eax, eax                ; AX=-1 if CY=1, AX=0 if CY=0
    sbb     eax, -1                 ; set to -1 or 1
    ret

; Compare 32-byte blocks
    align 16
mcmp50:
    shr     r9, 2                   ; compute number of 32-byte blocks
    jz      short mcmp70            ; if none, skip to 8-byte blocks

mcmp60:
    mov     rax, [rcx]              ; check first 8 bytes
    cmp     rax, [rcx + rdx]        ;
    jne     mcmp_adjust0            ;
    mov     rax, [rcx + 8]          ; check second 8 bytes
    cmp     rax, [rcx + rdx + 8]    ;
    jne     mcmp_adjust8            ;
    mov     rax, [rcx + 16]         ; check third 8 bytes
    cmp     rax, [rcx + rdx + 16]   ;
    jne     mcmp_adjust16           ;
    mov     rax, [rcx + 24]         ; check fourth 8 bytes
    cmp     rax, [rcx + rdx + 24]   ;
    jne     mcmp_adjust24           ;
    add     rcx, 32                 ; advance 32 bytes
    dec     r9                      ; loop counter
    jnz     short mcmp60            ;
    and     r8, 31                  ; update remaining byte count

; Compare 8-byte blocks
mcmp70:
    mov     r9, r8
    shr     r9, 3
    jz      short mcmp30            ; handle tail bytes
mcmp80:
    mov     rax, [rcx]
    cmp     rax, [rcx + rdx]
    jne     short mcmp_adjust0
    add     rcx, 8
    dec     r9
    jnz     short mcmp80
    and     r8, 7
    jmp     mcmp30

; Adjust logic to determine lexical sign (+1 or -1) for quadword mismatch
mcmp_adjust24:
    add     rcx, 8
mcmp_adjust16:
    add     rcx, 8
mcmp_adjust8:
    add     rcx, 8
mcmp_adjust0:
    mov     rcx, [rdx + rcx]        ; get the value from buf2
    bswap   rax                     ; swap to big endian for lexical comparison
    bswap   rcx                     ;
    cmp     rax, rcx                ; set carry flag
    sbb     eax, eax                ; result based on carry
    sbb     eax, -1                 ; return -1 if rax < rcx, else 1
    ret