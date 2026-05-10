#ifndef ZENITH_VMM_H
#define ZENITH_VMM_H

#include "zenith/limine.h"

#include <stdbool.h>
#include <stdint.h>

#define VMM_PAGE_PRESENT 0x001ull
#define VMM_PAGE_WRITABLE 0x002ull
#define VMM_PAGE_USER 0x004ull
#define VMM_PAGE_WRITE_THROUGH 0x008ull
#define VMM_PAGE_CACHE_DISABLE 0x010ull

struct vmm_address_space {
    uint64_t pml4_physical;
};

void vmm_init(struct limine_hhdm_response *hhdm);
void *vmm_phys_to_hhdm(uint64_t physical_address);
struct vmm_address_space vmm_kernel_address_space(void);
struct vmm_address_space vmm_create_address_space(void);
void vmm_switch_address_space(struct vmm_address_space address_space);
bool vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
bool vmm_map_page_in_space(struct vmm_address_space address_space,
                           uint64_t virtual_address,
                           uint64_t physical_address,
                           uint64_t flags);
bool vmm_unmap_page(uint64_t virtual_address);
uint64_t vmm_translate(uint64_t virtual_address);
void vmm_self_test(void);

#endif
