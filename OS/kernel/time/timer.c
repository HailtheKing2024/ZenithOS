#include "zenith/timer.h"

#include "zenith/scheduler.h"
#include "zenith/serial.h"
#include "zenith/vmm.h"

enum {
    APIC_BASE_MSR = 0x1b,
    APIC_BASE_ENABLE = 1 << 11,
    APIC_SVR = 0x0f0,
    APIC_EOI = 0x0b0,
    APIC_LVT_TIMER = 0x320,
    APIC_INITIAL_COUNT = 0x380,
    APIC_DIVIDE_CONFIG = 0x3e0,
    APIC_TIMER_PERIODIC = 1 << 17,
    APIC_TIMER_VECTOR = 32
};

#define LAPIC_VIRTUAL_BASE 0xffff920000000000ull

static volatile uint64_t tick_count;
static uint32_t timer_frequency_hz;
static volatile uint32_t *lapic;

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

static uint32_t lapic_read(uint32_t offset)
{
    return lapic[offset / sizeof(uint32_t)];
}

static void lapic_write(uint32_t offset, uint32_t value)
{
    lapic[offset / sizeof(uint32_t)] = value;
    (void)lapic_read(APIC_EOI);
}

void timer_init(uint32_t frequency_hz)
{
    if (frequency_hz == 0) {
        frequency_hz = 100;
    }

    uint64_t apic_base = read_msr(APIC_BASE_MSR);
    write_msr(APIC_BASE_MSR, apic_base | APIC_BASE_ENABLE);

    uint64_t apic_physical = apic_base & 0xfffff000ull;
    if (!vmm_map_page(LAPIC_VIRTUAL_BASE, apic_physical,
                      VMM_PAGE_WRITABLE | VMM_PAGE_WRITE_THROUGH | VMM_PAGE_CACHE_DISABLE)) {
        serial_write("[timer] lapic page already mapped\n");
    }

    lapic = (volatile uint32_t *)(uintptr_t)LAPIC_VIRTUAL_BASE;
    lapic_write(APIC_SVR, lapic_read(APIC_SVR) | 0x100 | 0xff);

    uint32_t initial_count = 10000000u / frequency_hz;
    if (initial_count < 10000u) {
        initial_count = 10000u;
    }

    timer_frequency_hz = frequency_hz;
    lapic_write(APIC_DIVIDE_CONFIG, 0x3);
    lapic_write(APIC_LVT_TIMER, APIC_TIMER_VECTOR | APIC_TIMER_PERIODIC);
    lapic_write(APIC_INITIAL_COUNT, initial_count);

    serial_write("[timer] lapic frequency_hz=");
    serial_write_u64_dec(timer_frequency_hz);
    serial_write(" initial_count=");
    serial_write_u64_dec(initial_count);
    serial_write("\n");
}

struct interrupt_frame *timer_handle_irq(struct interrupt_frame *frame)
{
    tick_count++;
    lapic_write(APIC_EOI, 0);
    return scheduler_on_timer_interrupt(frame);
}

uint64_t timer_ticks(void)
{
    return tick_count;
}

void timer_wait_ticks(uint64_t ticks)
{
    uint64_t target = tick_count + ticks;

    while (tick_count < target) {
        __asm__ volatile ("hlt");
    }
}

void timer_self_test(void)
{
    uint64_t before = timer_ticks();
    timer_wait_ticks(8);
    uint64_t after = timer_ticks();

    if (after < before + 8) {
        serial_write("[timer] self-test failed\n");
        return;
    }

    serial_write("[timer] self-test ok ticks=");
    serial_write_u64_dec(after);
    serial_write("\n");
}
