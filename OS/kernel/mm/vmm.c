#include "zenith/vmm.h"

#include "zenith/panic.h"
#include "zenith/pmm.h"
#include "zenith/serial.h"

#include <stddef.h>

enum {
    PAGE_ENTRY_COUNT = 512,
    PAGE_OFFSET_MASK = 0xfff
};

static const uint64_t PAGE_ADDRESS_MASK = 0x000ffffffffff000ull;

static uint64_t hhdm_offset;
static uint64_t kernel_pml4_physical;
static uint64_t *kernel_pml4;
static bool initialized;

static uint64_t read_cr3(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr3, %0" : "=r"(value));
    return value & PAGE_ADDRESS_MASK;
}

static void invlpg(uint64_t virtual_address)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

static void memory_zero(void *ptr, size_t size)
{
    uint8_t *bytes = ptr;

    for (size_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

static uint16_t pml4_index(uint64_t virtual_address)
{
    return (uint16_t)((virtual_address >> 39) & 0x1ff);
}

static uint16_t pdpt_index(uint64_t virtual_address)
{
    return (uint16_t)((virtual_address >> 30) & 0x1ff);
}

static uint16_t pd_index(uint64_t virtual_address)
{
    return (uint16_t)((virtual_address >> 21) & 0x1ff);
}

static uint16_t pt_index(uint64_t virtual_address)
{
    return (uint16_t)((virtual_address >> 12) & 0x1ff);
}

void *vmm_phys_to_hhdm(uint64_t physical_address)
{
    if (hhdm_offset == 0) {
        panic("HHDM used before VMM initialization");
    }

    return (void *)(uintptr_t)(physical_address + hhdm_offset);
}

static uint64_t *table_from_entry(uint64_t entry)
{
    return vmm_phys_to_hhdm(entry & PAGE_ADDRESS_MASK);
}

static uint64_t make_table(uint64_t flags)
{
    uint64_t frame = pmm_alloc_frame();
    if (frame == 0) {
        panic("VMM could not allocate a page table");
    }

    memory_zero(vmm_phys_to_hhdm(frame), PMM_PAGE_SIZE);
    return frame | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | (flags & VMM_PAGE_USER);
}

static uint64_t *walk_to_pt(uint64_t *pml4, uint64_t virtual_address, bool create, uint64_t flags)
{
    uint64_t *pdpt;
    uint64_t *pd;

    uint16_t pml4i = pml4_index(virtual_address);
    uint16_t pdpti = pdpt_index(virtual_address);
    uint16_t pdi = pd_index(virtual_address);

    if ((pml4[pml4i] & VMM_PAGE_PRESENT) == 0) {
        if (!create) {
            return NULL;
        }
        pml4[pml4i] = make_table(flags);
    } else if ((flags & VMM_PAGE_USER) != 0) {
        pml4[pml4i] |= VMM_PAGE_USER;
    }

    pdpt = table_from_entry(pml4[pml4i]);
    if ((pdpt[pdpti] & VMM_PAGE_PRESENT) == 0) {
        if (!create) {
            return NULL;
        }
        pdpt[pdpti] = make_table(flags);
    } else if ((flags & VMM_PAGE_USER) != 0) {
        pdpt[pdpti] |= VMM_PAGE_USER;
    }

    pd = table_from_entry(pdpt[pdpti]);
    if ((pd[pdi] & VMM_PAGE_PRESENT) == 0) {
        if (!create) {
            return NULL;
        }
        pd[pdi] = make_table(flags);
    } else if ((flags & VMM_PAGE_USER) != 0) {
        pd[pdi] |= VMM_PAGE_USER;
    }

    return table_from_entry(pd[pdi]);
}

void vmm_init(struct limine_hhdm_response *hhdm)
{
    if (hhdm == NULL || hhdm->offset == 0) {
        panic("VMM requires Limine HHDM");
    }

    hhdm_offset = hhdm->offset;
    kernel_pml4_physical = read_cr3();
    kernel_pml4 = vmm_phys_to_hhdm(kernel_pml4_physical);
    initialized = true;

    serial_write("[vmm] hhdm=");
    serial_write_u64_hex(hhdm_offset);
    serial_write(" pml4=");
    serial_write_u64_hex(kernel_pml4_physical);
    serial_write("\n");
}

struct vmm_address_space vmm_kernel_address_space(void)
{
    if (!initialized) {
        panic("VMM kernel address space before initialization");
    }

    return (struct vmm_address_space){
        .pml4_physical = kernel_pml4_physical
    };
}

struct vmm_address_space vmm_create_address_space(void)
{
    if (!initialized) {
        panic("VMM address space create before initialization");
    }

    uint64_t pml4_physical = pmm_alloc_frame();
    if (pml4_physical == 0) {
        panic("VMM could not allocate PML4");
    }

    uint64_t *pml4 = vmm_phys_to_hhdm(pml4_physical);
    memory_zero(pml4, PMM_PAGE_SIZE);

    for (size_t i = PAGE_ENTRY_COUNT / 2; i < PAGE_ENTRY_COUNT; i++) {
        pml4[i] = kernel_pml4[i];
    }

    return (struct vmm_address_space){
        .pml4_physical = pml4_physical
    };
}

void vmm_switch_address_space(struct vmm_address_space address_space)
{
    if (!initialized || address_space.pml4_physical == 0) {
        panic("VMM switch to invalid address space");
    }

    __asm__ volatile ("movq %0, %%cr3" : : "r"(address_space.pml4_physical) : "memory");
}

bool vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags)
{
    if (!initialized) {
        panic("VMM map before initialization");
    }

    if ((virtual_address & PAGE_OFFSET_MASK) != 0 || (physical_address & PAGE_OFFSET_MASK) != 0) {
        panic("VMM map requires aligned addresses");
    }

    return vmm_map_page_in_space(vmm_kernel_address_space(), virtual_address, physical_address, flags);
}

bool vmm_map_page_in_space(struct vmm_address_space address_space,
                           uint64_t virtual_address,
                           uint64_t physical_address,
                           uint64_t flags)
{
    if (!initialized) {
        panic("VMM map before initialization");
    }

    if (address_space.pml4_physical == 0) {
        panic("VMM map requires valid address space");
    }

    if ((virtual_address & PAGE_OFFSET_MASK) != 0 || (physical_address & PAGE_OFFSET_MASK) != 0) {
        panic("VMM map requires aligned addresses");
    }

    uint64_t *pml4 = vmm_phys_to_hhdm(address_space.pml4_physical);
    uint64_t *pt = walk_to_pt(pml4, virtual_address, true, flags);
    uint16_t pti = pt_index(virtual_address);

    if ((pt[pti] & VMM_PAGE_PRESENT) != 0) {
        return false;
    }

    pt[pti] = (physical_address & PAGE_ADDRESS_MASK) | flags | VMM_PAGE_PRESENT;
    if (address_space.pml4_physical == read_cr3()) {
        invlpg(virtual_address);
    }
    return true;
}

bool vmm_unmap_page(uint64_t virtual_address)
{
    if (!initialized) {
        panic("VMM unmap before initialization");
    }

    if ((virtual_address & PAGE_OFFSET_MASK) != 0) {
        panic("VMM unmap requires an aligned address");
    }

    uint64_t *pt = walk_to_pt(kernel_pml4, virtual_address, false, 0);
    if (pt == NULL) {
        return false;
    }

    uint16_t pti = pt_index(virtual_address);
    if ((pt[pti] & VMM_PAGE_PRESENT) == 0) {
        return false;
    }

    pt[pti] = 0;
    invlpg(virtual_address);
    return true;
}

uint64_t vmm_translate(uint64_t virtual_address)
{
    if (!initialized) {
        panic("VMM translate before initialization");
    }

    uint64_t *pt = walk_to_pt(kernel_pml4, virtual_address, false, 0);
    if (pt == NULL) {
        return 0;
    }

    uint64_t entry = pt[pt_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return 0;
    }

    return (entry & PAGE_ADDRESS_MASK) | (virtual_address & PAGE_OFFSET_MASK);
}

void vmm_self_test(void)
{
    enum {
        TEST_PATTERN = 0x5a
    };

    uint64_t frame = pmm_alloc_frame();
    uint64_t virtual_address = 0xffff910000000000ull;

    if (frame == 0) {
        panic("VMM self-test could not allocate frame");
    }

    if (!vmm_map_page(virtual_address, frame, VMM_PAGE_WRITABLE)) {
        panic("VMM self-test could not map page");
    }

    volatile uint8_t *mapped = (volatile uint8_t *)(uintptr_t)virtual_address;
    mapped[0] = TEST_PATTERN;
    mapped[PMM_PAGE_SIZE - 1] = TEST_PATTERN + 1;

    uint8_t *direct = vmm_phys_to_hhdm(frame);
    if (direct[0] != TEST_PATTERN || direct[PMM_PAGE_SIZE - 1] != TEST_PATTERN + 1) {
        panic("VMM self-test HHDM verification failed");
    }

    if (vmm_translate(virtual_address) != frame) {
        panic("VMM self-test translation failed");
    }

    if (!vmm_unmap_page(virtual_address)) {
        panic("VMM self-test could not unmap page");
    }

    pmm_free_frame(frame);
    serial_write("[vmm] self-test ok\n");
}
