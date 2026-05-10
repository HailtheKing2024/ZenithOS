.section .text.start, "ax"
.global _start
.extern main

_start:
    xor %rbp, %rbp
    call main
    mov %rax, %rdi
    mov $14, %rax
    syscall
1:
    hlt
    jmp 1b
