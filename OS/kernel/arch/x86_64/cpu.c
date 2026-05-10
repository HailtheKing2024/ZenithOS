#include "zenith/cpu.h"

#include "zenith/compiler.h"
#include "zenith/serial.h"

#include <stddef.h>
#include <stdint.h>

enum {
    GDT_ENTRY_COUNT = 7,
    TSS_SELECTOR = 0x28,
    PRIVILEGE_STACK_SIZE = 32768,
    EFER_SYSCALL_ENABLE = 1
};

#define IA32_EFER UINT32_C(0xc0000080)
#define IA32_STAR UINT32_C(0xc0000081)
#define IA32_LSTAR UINT32_C(0xc0000082)
#define IA32_FMASK UINT32_C(0xc0000084)
#define SYSCALL_RFLAGS_MASK UINT64_C(0x47700)

struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} ZENITH_PACKED;

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} ZENITH_PACKED;

extern void x86_64_load_gdt_tss(const struct gdt_pointer *pointer, uint16_t tss_selector);
extern void x86_64_syscall_entry(void);

uint64_t x86_64_syscall_kernel_stack_top;
uint64_t x86_64_syscall_user_rsp;
uint64_t x86_64_syscall_user_rip;
uint64_t x86_64_syscall_user_rflags;

static uint64_t gdt[GDT_ENTRY_COUNT] ZENITH_ALIGN(16);
static struct tss64 tss ZENITH_ALIGN(16);
static uint8_t privilege_stack[PRIVILEGE_STACK_SIZE] ZENITH_ALIGN(16);

static uint64_t read_msr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static void write_msr(uint32_t msr, uint64_t value)
{
    uint32_t low = (uint32_t)(value & 0xffffffff);
    uint32_t high = (uint32_t)(value >> 32);

    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static void write_tss_descriptor(uint64_t base, uint32_t limit)
{
    uint64_t low = 0;

    low |= limit & 0xffffull;
    low |= (base & 0xffffffull) << 16;
    low |= 0x89ull << 40;
    low |= ((uint64_t)((limit >> 16) & 0xf)) << 48;
    low |= ((base >> 24) & 0xffull) << 56;

    gdt[TSS_SELECTOR / 8] = low;
    gdt[(TSS_SELECTOR / 8) + 1] = base >> 32;
}

void x86_64_cpu_init(void)
{
    uint64_t stack_top = ((uint64_t)(uintptr_t)privilege_stack + PRIVILEGE_STACK_SIZE) & ~0xfull;

    gdt[0] = 0;
    gdt[1] = 0x00af9a000000ffffull;
    gdt[2] = 0x00cf92000000ffffull;
    gdt[3] = 0x00cff2000000ffffull;
    gdt[4] = 0x00affa000000ffffull;

    tss.rsp0 = stack_top;
    tss.iopb_offset = sizeof(tss);
    write_tss_descriptor((uint64_t)(uintptr_t)&tss, sizeof(tss) - 1);

    const struct gdt_pointer pointer = {
        .limit = sizeof(gdt) - 1,
        .base = (uint64_t)(uintptr_t)gdt
    };

    x86_64_load_gdt_tss(&pointer, TSS_SELECTOR);
    x86_64_syscall_kernel_stack_top = stack_top;

    serial_write("[cpu] gdt/tss initialized rsp0=");
    serial_write_u64_hex(stack_top);
    serial_write("\n");
}

void x86_64_syscall_enable(void)
{
    uint64_t efer = read_msr(IA32_EFER);
    write_msr(IA32_EFER, efer | EFER_SYSCALL_ENABLE);

    uint64_t star = ((uint64_t)X86_64_KERNEL_DS << 48) | ((uint64_t)X86_64_KERNEL_CS << 32);
    write_msr(IA32_STAR, star);
    write_msr(IA32_LSTAR, (uint64_t)(uintptr_t)x86_64_syscall_entry);
    write_msr(IA32_FMASK, SYSCALL_RFLAGS_MASK);

    serial_write("[syscall] cpu entry enabled lstar=");
    serial_write_u64_hex((uint64_t)(uintptr_t)x86_64_syscall_entry);
    serial_write("\n");
}

void x86_64_set_kernel_stack(uint64_t stack_top)
{
    tss.rsp0 = stack_top;
    x86_64_syscall_kernel_stack_top = stack_top;
}
