#include "zenith/kheap.h"

#include "zenith/panic.h"
#include "zenith/pmm.h"
#include "zenith/serial.h"
#include "zenith/vmm.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    KHEAP_INITIAL_PAGES = 16,
    KHEAP_MAX_PAGES = 4096
};

#define KHEAP_BASE 0xffff900000000000ull
#define KHEAP_LIMIT (KHEAP_BASE + (KHEAP_MAX_PAGES * PMM_PAGE_SIZE))

static uint64_t heap_cursor;
static uint64_t heap_mapped_end;
static bool initialized;

static uint64_t align_up_u64(uint64_t value, uint64_t alignment)
{
    if (alignment == 0) {
        return value;
    }

    return (value + alignment - 1) & ~(alignment - 1);
}

static void map_heap_page(uint64_t virtual_address)
{
    uint64_t frame = pmm_alloc_frame();
    if (frame == 0) {
        panic("kernel heap could not allocate a frame");
    }

    if (!vmm_map_page(virtual_address, frame, VMM_PAGE_WRITABLE)) {
        panic("kernel heap map collision");
    }
}

static void ensure_mapped(uint64_t end_address)
{
    while (heap_mapped_end < end_address) {
        if (heap_mapped_end >= KHEAP_LIMIT) {
            panic("kernel heap exhausted");
        }

        map_heap_page(heap_mapped_end);
        heap_mapped_end += PMM_PAGE_SIZE;
    }
}

void kheap_init(void)
{
    heap_cursor = KHEAP_BASE;
    heap_mapped_end = KHEAP_BASE;
    ensure_mapped(KHEAP_BASE + (KHEAP_INITIAL_PAGES * PMM_PAGE_SIZE));
    initialized = true;

    serial_write("[heap] base=");
    serial_write_u64_hex(KHEAP_BASE);
    serial_write(" mapped_kib=");
    serial_write_u64_dec((heap_mapped_end - KHEAP_BASE) / 1024);
    serial_write("\n");
}

void *kmalloc_aligned(size_t size, size_t alignment)
{
    if (!initialized) {
        panic("kmalloc before heap initialization");
    }

    if (size == 0) {
        return NULL;
    }

    if (alignment < sizeof(uintptr_t)) {
        alignment = sizeof(uintptr_t);
    }

    uint64_t start = align_up_u64(heap_cursor, alignment);
    uint64_t end = start + size;

    if (end < start) {
        panic("kernel heap allocation overflow");
    }

    ensure_mapped(end);
    heap_cursor = end;
    return (void *)(uintptr_t)start;
}

void *kmalloc(size_t size)
{
    return kmalloc_aligned(size, sizeof(uintptr_t));
}

void kheap_self_test(void)
{
    uint8_t *small = kmalloc(32);
    uint8_t *page_aligned = kmalloc_aligned(128, PMM_PAGE_SIZE);

    if (small == NULL || page_aligned == NULL) {
        panic("kernel heap self-test allocation failed");
    }

    if (((uint64_t)(uintptr_t)page_aligned % PMM_PAGE_SIZE) != 0) {
        panic("kernel heap self-test alignment failed");
    }

    for (size_t i = 0; i < 32; i++) {
        small[i] = (uint8_t)i;
    }

    for (size_t i = 0; i < 32; i++) {
        if (small[i] != (uint8_t)i) {
            panic("kernel heap self-test write verification failed");
        }
    }

    serial_write("[heap] self-test ok\n");
}
