.section .text

.macro EXCEPTION_NO_ERROR vector
.global x86_64_exception_stub_\vector
.type x86_64_exception_stub_\vector, @function
x86_64_exception_stub_\vector:
    pushq $0
    pushq $\vector
    jmp x86_64_exception_common
.endm

.macro EXCEPTION_WITH_ERROR vector
.global x86_64_exception_stub_\vector
.type x86_64_exception_stub_\vector, @function
x86_64_exception_stub_\vector:
    pushq $\vector
    jmp x86_64_exception_common
.endm

x86_64_exception_common:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdi
    pushq %rsi
    pushq %rbp
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    movq %rsp, %rdi
    andq $-16, %rsp
    call x86_64_interrupt_dispatch
    movq %rax, %rsp

.global x86_64_restore_interrupt_frame
.type x86_64_restore_interrupt_frame, @function
x86_64_restore_interrupt_frame:
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rbp
    popq %rsi
    popq %rdi
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    addq $16, %rsp
    iretq

EXCEPTION_NO_ERROR 0
EXCEPTION_NO_ERROR 1
EXCEPTION_NO_ERROR 2
EXCEPTION_NO_ERROR 3
EXCEPTION_NO_ERROR 4
EXCEPTION_NO_ERROR 5
EXCEPTION_NO_ERROR 6
EXCEPTION_NO_ERROR 7
EXCEPTION_WITH_ERROR 8
EXCEPTION_NO_ERROR 9
EXCEPTION_WITH_ERROR 10
EXCEPTION_WITH_ERROR 11
EXCEPTION_WITH_ERROR 12
EXCEPTION_WITH_ERROR 13
EXCEPTION_WITH_ERROR 14
EXCEPTION_NO_ERROR 15
EXCEPTION_NO_ERROR 16
EXCEPTION_WITH_ERROR 17
EXCEPTION_NO_ERROR 18
EXCEPTION_NO_ERROR 19
EXCEPTION_NO_ERROR 20
EXCEPTION_WITH_ERROR 21
EXCEPTION_NO_ERROR 22
EXCEPTION_NO_ERROR 23
EXCEPTION_NO_ERROR 24
EXCEPTION_NO_ERROR 25
EXCEPTION_NO_ERROR 26
EXCEPTION_NO_ERROR 27
EXCEPTION_NO_ERROR 28
EXCEPTION_WITH_ERROR 29
EXCEPTION_WITH_ERROR 30
EXCEPTION_NO_ERROR 31

.global x86_64_irq0_stub
.type x86_64_irq0_stub, @function
x86_64_irq0_stub:
    pushq $0
    pushq $32
    jmp x86_64_exception_common

.global x86_64_irq1_stub
.type x86_64_irq1_stub, @function
x86_64_irq1_stub:
    pushq $0
    pushq $33
    jmp x86_64_exception_common

.global x86_64_irq12_stub
.type x86_64_irq12_stub, @function
x86_64_irq12_stub:
    pushq $0
    pushq $44
    jmp x86_64_exception_common

.global x86_64_thread_entry
.type x86_64_thread_entry, @function
x86_64_thread_entry:
    pushq $0
    movq %rdi, %rax
    movq %rsi, %rdi
    call *%rax
    call scheduler_exit_current_thread
1:
    hlt
    jmp 1b

.global x86_64_load_gdt_tss
.type x86_64_load_gdt_tss, @function
x86_64_load_gdt_tss:
    lgdt (%rdi)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    movw %si, %ax
    ltr %ax
    ret

.global x86_64_enter_user_mode
.type x86_64_enter_user_mode, @function
x86_64_enter_user_mode:
    movw $0x1b, %ax
    movw %ax, %ds
    movw %ax, %es
    pushq $0x1b
    pushq %rsi
    pushfq
    popq %rax
    orq $0x200, %rax
    pushq %rax
    pushq $0x23
    pushq %rdi
    iretq

.global x86_64_syscall_entry
.type x86_64_syscall_entry, @function
x86_64_syscall_entry:
    movq %rsp, x86_64_syscall_user_rsp(%rip)
    movq x86_64_syscall_kernel_stack_top(%rip), %rsp
    subq $176, %rsp

    movq %rax, 0(%rsp)
    movq %rbx, 8(%rsp)
    movq %rcx, 16(%rsp)
    movq %rdx, 24(%rsp)
    movq %rbp, 32(%rsp)
    movq %rsi, 40(%rsp)
    movq %rdi, 48(%rsp)
    movq %r8, 56(%rsp)
    movq %r9, 64(%rsp)
    movq %r10, 72(%rsp)
    movq %r11, 80(%rsp)
    movq %r12, 88(%rsp)
    movq %r13, 96(%rsp)
    movq %r14, 104(%rsp)
    movq %r15, 112(%rsp)
    movq $0x80, 120(%rsp)
    movq $0, 128(%rsp)
    movq %rcx, 136(%rsp)
    movq $0x23, 144(%rsp)
    movq %r11, 152(%rsp)
    movq x86_64_syscall_user_rsp(%rip), %rax
    movq %rax, 160(%rsp)
    movq $0x1b, 168(%rsp)

    movq %rsp, %rdi
    call syscall_handle_frame
    movq %rax, %rsp
    jmp x86_64_restore_interrupt_frame

.section .rodata
.global x86_64_exception_stub_table
.type x86_64_exception_stub_table, @object
x86_64_exception_stub_table:
    .quad x86_64_exception_stub_0
    .quad x86_64_exception_stub_1
    .quad x86_64_exception_stub_2
    .quad x86_64_exception_stub_3
    .quad x86_64_exception_stub_4
    .quad x86_64_exception_stub_5
    .quad x86_64_exception_stub_6
    .quad x86_64_exception_stub_7
    .quad x86_64_exception_stub_8
    .quad x86_64_exception_stub_9
    .quad x86_64_exception_stub_10
    .quad x86_64_exception_stub_11
    .quad x86_64_exception_stub_12
    .quad x86_64_exception_stub_13
    .quad x86_64_exception_stub_14
    .quad x86_64_exception_stub_15
    .quad x86_64_exception_stub_16
    .quad x86_64_exception_stub_17
    .quad x86_64_exception_stub_18
    .quad x86_64_exception_stub_19
    .quad x86_64_exception_stub_20
    .quad x86_64_exception_stub_21
    .quad x86_64_exception_stub_22
    .quad x86_64_exception_stub_23
    .quad x86_64_exception_stub_24
    .quad x86_64_exception_stub_25
    .quad x86_64_exception_stub_26
    .quad x86_64_exception_stub_27
    .quad x86_64_exception_stub_28
    .quad x86_64_exception_stub_29
    .quad x86_64_exception_stub_30
    .quad x86_64_exception_stub_31
