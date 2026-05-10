[bits 64]
section .text
global _start
extern main

_start:
    call main
    mov rax, 60
    xor rdi, rdi
    syscall
.halt:
    hlt
    jmp .halt
