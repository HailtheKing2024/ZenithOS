#include "zenith/pmm.h"

#include "zenith/panic.h"
#include "zenith/serial.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    PMM_MAX_PHYSICAL_GIB = 16,
    PMM_LOW_MEMORY_RESERVE = 0x100000
};

#define PMM_MAX_FRAMES ((PMM_MAX_PHYSICAL_GIB * 1024ull * 1024ull * 1024ull) / PMM_PAGE_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8)

static uint8_t frame_bitmap[PMM_BITMAP_BYTES];
static uint8_t usable_bitmap[PMM_BITMAP_BYTES];
static uint64_t tracked_frames;
static uint64_t usable_frames;
static uint64_t free_frames;
static uint64_t highest_usable_address;
static uint64_t capped_frames;
static uint64_t next_search_frame;
static bool initialized;

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint64_t align_down(uint64_t value, uint64_t alignment)
{
    return value & ~(alignment - 1);
}

static uint64_t frame_index(uint64_t physical_address)
{
    return physical_address / PMM_PAGE_SIZE;
}

static uint64_t frame_address(uint64_t index)
{
    return index * PMM_PAGE_SIZE;
}

static uint8_t frame_mask(uint64_t index)
{
    return (uint8_t)(1u << (index & 7u));
}

static bool frame_is_used(uint64_t index)
{
    return (frame_bitmap[index / 8] & frame_mask(index)) != 0;
}

static bool frame_is_usable(uint64_t index)
{
    return (usable_bitmap[index / 8] & frame_mask(index)) != 0;
}

static void mark_frame_not_usable(uint64_t index)
{
    if (index >= tracked_frames || !frame_is_usable(index)) {
        return;
    }

    usable_bitmap[index / 8] &= (uint8_t)~frame_mask(index);
    usable_frames--;

    if (!frame_is_used(index)) {
        frame_bitmap[index / 8] |= frame_mask(index);
        free_frames--;
    }
}

static void mark_frame_used(uint64_t index)
{
    if (index >= tracked_frames) {
        return;
    }

    if (!frame_is_used(index)) {
        frame_bitmap[index / 8] |= frame_mask(index);
        free_frames--;
    }
}

static void mark_frame_free(uint64_t index)
{
    if (index >= tracked_frames) {
        return;
    }

    if (!frame_is_usable(index)) {
        usable_bitmap[index / 8] |= frame_mask(index);
        usable_frames++;
    }

    if (frame_is_used(index)) {
        frame_bitmap[index / 8] &= (uint8_t)~frame_mask(index);
        free_frames++;
    }
}

static void mark_range_free(uint64_t base, uint64_t length)
{
    uint64_t start = align_up(base, PMM_PAGE_SIZE);
    uint64_t end = align_down(base + length, PMM_PAGE_SIZE);

    if (end <= start) {
        return;
    }

    for (uint64_t address = start; address < end; address += PMM_PAGE_SIZE) {
        mark_frame_free(frame_index(address));
    }
}

static void mark_range_not_usable(uint64_t base, uint64_t length)
{
    uint64_t start = align_down(base, PMM_PAGE_SIZE);
    uint64_t end = align_up(base + length, PMM_PAGE_SIZE);

    if (end <= start) {
        return;
    }

    for (uint64_t address = start; address < end; address += PMM_PAGE_SIZE) {
        mark_frame_not_usable(frame_index(address));
    }
}

static void fill_bitmap_used(void)
{
    for (size_t i = 0; i < sizeof(frame_bitmap); i++) {
        frame_bitmap[i] = 0xff;
    }

    for (size_t i = 0; i < sizeof(usable_bitmap); i++) {
        usable_bitmap[i] = 0;
    }
}

static uint64_t memmap_highest_usable_address(struct limine_memmap_response *memmap)
{
    uint64_t highest = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t end = entry->base + entry->length;
        if (end > highest) {
            highest = end;
        }
    }

    return highest;
}

void pmm_init(struct limine_memmap_response *memmap)
{
    if (memmap == NULL) {
        panic("PMM requires a Limine memory map");
    }

    fill_bitmap_used();

    highest_usable_address = memmap_highest_usable_address(memmap);
    if (highest_usable_address == 0) {
        panic("PMM found no usable memory");
    }

    uint64_t required_frames = align_up(highest_usable_address, PMM_PAGE_SIZE) / PMM_PAGE_SIZE;
    tracked_frames = required_frames;

    if (tracked_frames > PMM_MAX_FRAMES) {
        capped_frames = tracked_frames - PMM_MAX_FRAMES;
        tracked_frames = PMM_MAX_FRAMES;
    }

    usable_frames = 0;
    free_frames = 0;
    next_search_frame = frame_index(PMM_LOW_MEMORY_RESERVE);

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            mark_range_free(entry->base, entry->length);
        }
    }

    mark_range_not_usable(0, PMM_LOW_MEMORY_RESERVE);
    initialized = true;

    struct pmm_stats stats = pmm_get_stats();
    serial_write("[pmm] initialized frames=");
    serial_write_u64_dec(stats.total_tracked_frames);
    serial_write(" usable=");
    serial_write_u64_dec(stats.total_usable_frames);
    serial_write(" free=");
    serial_write_u64_dec(stats.free_frames);
    serial_write(" used=");
    serial_write_u64_dec(stats.used_frames);
    serial_write(" highest_usable=");
    serial_write_u64_hex(stats.highest_usable_address);
    if (stats.capped_frames != 0) {
        serial_write(" capped=");
        serial_write_u64_dec(stats.capped_frames);
    }
    serial_write("\n");
}

uint64_t pmm_alloc_frame(void)
{
    if (!initialized) {
        panic("PMM allocation before initialization");
    }

    if (free_frames == 0) {
        return 0;
    }

    for (uint64_t i = next_search_frame; i < tracked_frames; i++) {
        if (frame_is_usable(i) && !frame_is_used(i)) {
            mark_frame_used(i);
            next_search_frame = i + 1;
            return frame_address(i);
        }
    }

    for (uint64_t i = frame_index(PMM_LOW_MEMORY_RESERVE); i < next_search_frame; i++) {
        if (frame_is_usable(i) && !frame_is_used(i)) {
            mark_frame_used(i);
            next_search_frame = i + 1;
            return frame_address(i);
        }
    }

    return 0;
}

void pmm_free_frame(uint64_t physical_address)
{
    if (!initialized) {
        panic("PMM free before initialization");
    }

    if ((physical_address % PMM_PAGE_SIZE) != 0) {
        panic("PMM free requires a page-aligned address");
    }

    uint64_t index = frame_index(physical_address);
    if (index >= tracked_frames || !frame_is_usable(index)) {
        panic("PMM free outside managed memory");
    }

    if (!frame_is_used(index)) {
        panic("PMM double free detected");
    }

    mark_frame_free(index);

    if (index < next_search_frame) {
        next_search_frame = index;
    }
}

struct pmm_stats pmm_get_stats(void)
{
    struct pmm_stats stats = {
        .page_size = PMM_PAGE_SIZE,
        .highest_usable_address = highest_usable_address,
        .total_tracked_frames = tracked_frames,
        .total_usable_frames = usable_frames,
        .free_frames = free_frames,
        .used_frames = usable_frames - free_frames,
        .capped_frames = capped_frames
    };

    return stats;
}

void pmm_self_test(void)
{
    struct pmm_stats before = pmm_get_stats();
    uint64_t a = pmm_alloc_frame();
    uint64_t b = pmm_alloc_frame();
    uint64_t c = pmm_alloc_frame();

    if (a == 0 || b == 0 || c == 0) {
        panic("PMM self-test allocation failed");
    }

    if ((a % PMM_PAGE_SIZE) != 0 || (b % PMM_PAGE_SIZE) != 0 || (c % PMM_PAGE_SIZE) != 0) {
        panic("PMM self-test returned unaligned frame");
    }

    if (a == b || a == c || b == c) {
        panic("PMM self-test returned duplicate frame");
    }

    struct pmm_stats after_alloc = pmm_get_stats();
    if (after_alloc.free_frames + 3 != before.free_frames) {
        panic("PMM self-test free count did not decrease");
    }

    pmm_free_frame(c);
    pmm_free_frame(b);
    pmm_free_frame(a);

    struct pmm_stats after_free = pmm_get_stats();
    if (after_free.free_frames != before.free_frames) {
        panic("PMM self-test free count did not recover");
    }

    serial_write("[pmm] self-test ok frames=");
    serial_write_u64_hex(a);
    serial_write(",");
    serial_write_u64_hex(b);
    serial_write(",");
    serial_write_u64_hex(c);
    serial_write("\n");
}
