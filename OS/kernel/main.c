#include "zenith/compiler.h"
#include "zenith/cpu.h"
#include "zenith/framebuffer.h"
#include "zenith/halt.h"
#include "zenith/initramfs.h"
#include "zenith/input.h"
#include "zenith/interrupts.h"
#include "zenith/ipc.h"
#include "zenith/keyboard.h"
#include "zenith/kheap.h"
#include "zenith/limine.h"
#include "zenith/mouse.h"
#include "zenith/panic.h"
#include "zenith/pmm.h"
#include "zenith/scheduler.h"
#include "zenith/serial.h"
#include "zenith/syscall.h"
#include "zenith/timer.h"
#include "zenith/ui.h"
#include "zenith/user_loader.h"
#include "zenith/vmm.h"

#include <stddef.h>
#include <stdint.h>

ZENITH_USED ZENITH_SECTION(".limine_requests_start")
static volatile uint64_t limine_requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

ZENITH_USED ZENITH_SECTION(".limine_requests")
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(6);

ZENITH_USED ZENITH_SECTION(".limine_requests")
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

ZENITH_USED ZENITH_SECTION(".limine_requests")
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

ZENITH_USED ZENITH_SECTION(".limine_requests")
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

ZENITH_USED ZENITH_SECTION(".limine_requests")
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

ZENITH_USED ZENITH_SECTION(".limine_requests_end")
static volatile uint64_t limine_requests_end_marker[2] = LIMINE_REQUESTS_END_MARKER;

static uint64_t kib(uint64_t bytes)
{
    return bytes / 1024;
}

static void log_memmap(void)
{
    struct limine_memmap_response *response = memmap_request.response;
    uint64_t usable_bytes = 0;

    if (response == NULL) {
        serial_write("[mem] no memory map response\n");
        return;
    }

    serial_write("[mem] entries=");
    serial_write_u64_dec(response->entry_count);
    serial_write("\n");

    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry *entry = response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            usable_bytes += entry->length;
        }
    }

    serial_write("[mem] usable_kib=");
    serial_write_u64_dec(kib(usable_bytes));
    serial_write("\n");
}

static void log_acpi(void)
{
    struct limine_rsdp_response *response = rsdp_request.response;

    if (response == NULL || response->address == NULL) {
        serial_write("[acpi] no rsdp response\n");
        return;
    }

    serial_write("[acpi] rsdp=");
    serial_write_u64_hex((uint64_t)(uintptr_t)response->address);
    serial_write("\n");
}

static void log_hhdm(void)
{
    struct limine_hhdm_response *response = hhdm_request.response;

    if (response == NULL || response->offset == 0) {
        serial_write("[vmm] no hhdm response\n");
        return;
    }

    serial_write("[vmm] hhdm_offset=");
    serial_write_u64_hex(response->offset);
    serial_write("\n");
}

static void log_framebuffer(void)
{
    struct limine_framebuffer_response *response = framebuffer_request.response;

    if (response == NULL || response->framebuffer_count == 0) {
        serial_write("[gfx] no framebuffer response\n");
        return;
    }

    struct limine_framebuffer *fb = response->framebuffers[0];
    serial_write("[gfx] framebuffer ");
    serial_write_u64_dec(fb->width);
    serial_write("x");
    serial_write_u64_dec(fb->height);
    serial_write(" pitch=");
    serial_write_u64_dec(fb->pitch);
    serial_write(" bpp=");
    serial_write_u64_dec(fb->bpp);
    serial_write("\n");

    panic_set_framebuffer(fb);
    framebuffer_boot_screen(fb);
}

static void print_boot_logo(void)
{
    serial_write("\n");
    serial_write("============================================================\n");
    serial_write("ZZZZZ  EEEEE  N   N  III  TTTTT  H   H\n");
    serial_write("   Z   E      NN  N   I     T    H   H\n");
    serial_write("  Z    EEEE   N N N   I     T    HHHHH\n");
    serial_write(" Z     E      N  NN   I     T    H   H\n");
    serial_write("ZZZZZ  EEEEE  N   N  III    T    H   H\n");
    serial_write("============================================================\n");
    serial_write("ZenithOS workstation bootstrap\n\n");
}

ZENITH_NORETURN void zenith_kernel_main(void)
{
    serial_init();
    print_boot_logo();
    serial_write("[zenith] kernel entry reached\n");

    if (limine_base_revision[2] != 0) {
        serial_write("[boot] Limine base revision unsupported\n");
        zenith_halt();
    }

    serial_write("[boot] Limine base revision accepted\n");
    x86_64_cpu_init();
    interrupts_init();
    interrupts_self_test();
    log_memmap();
    log_acpi();
    log_hhdm();
    log_framebuffer();
    pmm_init(memmap_request.response);
    pmm_self_test();
    vmm_init(hhdm_request.response);
    vmm_self_test();
    kheap_init();
    kheap_self_test();
    input_init();
    input_self_test();
    keyboard_init();
    mouse_init();
    initramfs_init();
    initramfs_self_test();
    ipc_init();
    ipc_self_test();
    ui_init();
    ui_self_test();
    syscall_init();
    syscall_self_test();
    scheduler_init();
    scheduler_start_thread_self_test();
    syscall_start_user_self_test();
    timer_init(100);
    user_loader_start_init();
    user_loader_self_test();
    interrupts_enable();
    timer_self_test();
    scheduler_self_test();
    timer_wait_ticks(40);
    syscall_user_self_test();

    serial_write("[zenith] bootstrap complete; halting BSP\n");
    zenith_halt();
}
