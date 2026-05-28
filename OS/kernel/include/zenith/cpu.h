#ifndef ZENITH_CPU_H
#define ZENITH_CPU_H

#include "zenith/compiler.h"

#include <stdint.h>

#define X86_64_KERNEL_CS UINT64_C(0x08)
#define X86_64_KERNEL_DS UINT64_C(0x10)
#define X86_64_USER_DS UINT64_C(0x1b)
#define X86_64_USER_CS UINT64_C(0x23)

void x86_64_cpu_init(void);
void x86_64_syscall_enable(void);
void x86_64_set_kernel_stack(uint64_t stack_top);
ZENITH_NORETURN void x86_64_reboot(void);
ZENITH_NORETURN void x86_64_enter_user_mode(uint64_t entry, uint64_t user_stack_top);

#endif
