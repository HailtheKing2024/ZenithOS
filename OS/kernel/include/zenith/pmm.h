#ifndef ZENITH_PMM_H
#define ZENITH_PMM_H

#include "zenith/limine.h"

#include <stdint.h>

#define PMM_PAGE_SIZE 4096ull

struct pmm_stats {
    uint64_t page_size;
    uint64_t highest_usable_address;
    uint64_t total_tracked_frames;
    uint64_t total_usable_frames;
    uint64_t free_frames;
    uint64_t used_frames;
    uint64_t capped_frames;
};

void pmm_init(struct limine_memmap_response *memmap);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t physical_address);
struct pmm_stats pmm_get_stats(void);
void pmm_self_test(void);

#endif
