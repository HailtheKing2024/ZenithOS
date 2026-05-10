#include "zenith/interrupts.h"

#include "zenith/compiler.h"
#include "zenith/interrupt_frame.h"
#include "zenith/keyboard.h"
#include "zenith/mouse.h"
#include "zenith/panic.h"
#include "zenith/serial.h"
#include "zenith/timer.h"

#include "io.h"

#include <stdint.h>

enum {
    IDT_ENTRY_COUNT = 256,
    KERNEL_INTERRUPT_GATE = 0x8e,
    IRQ0_VECTOR = 32,
    IRQ1_VECTOR = 33,
    IRQ12_VECTOR = 44,
    PIC1_COMMAND = 0x20,
    PIC1_DATA = 0x21,
    PIC2_COMMAND = 0xa0,
    PIC2_DATA = 0xa1,
    PIC_EOI = 0x20
};

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} ZENITH_PACKED;

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} ZENITH_PACKED;

extern void (*x86_64_exception_stub_table[32])(void);
extern void x86_64_irq0_stub(void);
extern void x86_64_irq1_stub(void);
extern void x86_64_irq12_stub(void);

static struct idt_entry idt[IDT_ENTRY_COUNT];

static uint16_t read_cs(void)
{
    uint16_t value;
    __asm__ volatile ("movw %%cs, %0" : "=r"(value));
    return value;
}

static void load_idt(const struct idt_pointer *pointer)
{
    __asm__ volatile ("lidt %0" : : "m"(*pointer) : "memory");
}

static void idt_set_gate(uint8_t vector, void (*handler)(void), uint16_t selector)
{
    uint64_t address = (uint64_t)(uintptr_t)handler;

    idt[vector].offset_low = (uint16_t)(address & 0xffff);
    idt[vector].selector = selector;
    idt[vector].ist = 0;
    idt[vector].type_attributes = KERNEL_INTERRUPT_GATE;
    idt[vector].offset_mid = (uint16_t)((address >> 16) & 0xffff);
    idt[vector].offset_high = (uint32_t)((address >> 32) & 0xffffffff);
    idt[vector].reserved = 0;
}

static const char *exception_name(uint64_t vector)
{
    static const char *const names[32] = {
        "divide error",
        "debug",
        "non-maskable interrupt",
        "breakpoint",
        "overflow",
        "bound range exceeded",
        "invalid opcode",
        "device not available",
        "double fault",
        "coprocessor segment overrun",
        "invalid tss",
        "segment not present",
        "stack-segment fault",
        "general protection fault",
        "page fault",
        "reserved",
        "x87 floating-point exception",
        "alignment check",
        "machine check",
        "simd floating-point exception",
        "virtualization exception",
        "control protection exception",
        "reserved",
        "reserved",
        "reserved",
        "reserved",
        "reserved",
        "reserved",
        "hypervisor injection exception",
        "vmm communication exception",
        "security exception",
        "reserved"
    };

    if (vector < 32) {
        return names[vector];
    }

    return "unknown exception";
}

static void pic_remap_and_mask_all(void)
{
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    outb(PIC1_DATA, 0xff);
    io_wait();
    outb(PIC2_DATA, 0xff);
    io_wait();
}

void interrupts_init(void)
{
    uint16_t kernel_cs = read_cs();

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_gate(vector, x86_64_exception_stub_table[vector], kernel_cs);
    }

    idt_set_gate(IRQ0_VECTOR, x86_64_irq0_stub, kernel_cs);
    idt_set_gate(IRQ1_VECTOR, x86_64_irq1_stub, kernel_cs);
    idt_set_gate(IRQ12_VECTOR, x86_64_irq12_stub, kernel_cs);
    pic_remap_and_mask_all();

    struct idt_pointer pointer = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)(uintptr_t)idt
    };

    load_idt(&pointer);
    serial_write("[idt] exception, timer, keyboard, and mouse gates installed\n");
}

void interrupts_enable(void)
{
    __asm__ volatile ("sti");
    serial_write("[idt] external interrupts enabled\n");
}

void interrupts_self_test(void)
{
    serial_write("[idt] breakpoint self-test begin\n");
    __asm__ volatile ("int3");
    serial_write("[idt] breakpoint self-test ok\n");
}

struct interrupt_frame *x86_64_interrupt_dispatch(struct interrupt_frame *frame)
{
    if (frame->vector == IRQ0_VECTOR) {
        return timer_handle_irq(frame);
    }

    if (frame->vector == IRQ1_VECTOR) {
        return keyboard_handle_irq(frame);
    }

    if (frame->vector == IRQ12_VECTOR) {
        return mouse_handle_irq(frame);
    }

    if (frame->vector == 3) {
        serial_write("[trap] breakpoint handled\n");
        return frame;
    }

    serial_write("[trap] fatal exception vector=");
    serial_write_u64_dec(frame->vector);
    serial_write(" name=");
    serial_write(exception_name(frame->vector));
    serial_write(" error=");
    serial_write_u64_hex(frame->error_code);
    serial_write(" rip=");
    serial_write_u64_hex(frame->rip);
    serial_write(" rflags=");
    serial_write_u64_hex(frame->rflags);
    serial_write("\n");

    panic("unhandled CPU exception");
}
