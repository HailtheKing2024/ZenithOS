.section .bss
.align 16
bootstrap_stack_bottom:
    .skip 32768
bootstrap_stack_top:

.section .text
.global _start
.type _start, @function
_start:
    cli
    lea bootstrap_stack_top(%rip), %rsp
    and $-16, %rsp
    call zenith_kernel_main

.global zenith_halt
.type zenith_halt, @function
zenith_halt:
1:
    hlt
    jmp 1b

