#include "zenith/user_loader.h"

#include "zenith/initramfs.h"
#include "zenith/panic.h"
#include "zenith/pmm.h"
#include "zenith/scheduler.h"
#include "zenith/serial.h"
#include "zenith/vmm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool init_started;

struct loaded_user_program {
    const char *name;
    struct vmm_address_space address_space;
    uint64_t entry;
    uint64_t user_stack_top;
    uint64_t arg0;
};

static void memory_zero(void *ptr, size_t size)
{
    uint8_t *bytes = ptr;

    for (size_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

static void copy_bytes(uint8_t *dest, const uint8_t *src, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        dest[i] = src[i];
    }
}

static void copy_arg_string(uint8_t *dest, const char *arg)
{
    if (arg == NULL) {
        dest[0] = 0;
        return;
    }

    size_t i = 0;
    while (i + 1 < PMM_PAGE_SIZE && arg[i] != '\0') {
        dest[i] = (uint8_t)arg[i];
        i++;
    }

    dest[i] = 0;
}

static bool load_user_program(const char *path, const char *arg, struct loaded_user_program *program_out)
{
    if (path == NULL || program_out == NULL) {
        return false;
    }

    const struct initramfs_file *file = initramfs_lookup(path);
    if (file == NULL || file->size < sizeof(struct user_program_header)) {
        return false;
    }

    const struct user_program_header *header = (const struct user_program_header *)file->data;
    if (header->magic != USER_PROGRAM_MAGIC ||
        header->header_size < sizeof(*header) ||
        header->entry_offset >= header->image_size ||
        header->image_size == 0 ||
        header->image_size > USER_PROGRAM_MAX_IMAGE_SIZE ||
        header->stack_pages != 1 ||
        file->size < header->header_size + header->image_size) {
        return false;
    }

    struct vmm_address_space address_space = vmm_create_address_space();
    uint64_t image_pages = (header->image_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    uint64_t image_frames[USER_PROGRAM_MAX_IMAGE_PAGES] = {0};

    uint64_t stack_frame = pmm_alloc_frame();
    uint64_t arg_frame = pmm_alloc_frame();
    if (stack_frame == 0 || arg_frame == 0) {
        return false;
    }

    for (uint64_t i = 0; i < image_pages; i++) {
        image_frames[i] = pmm_alloc_frame();
        if (image_frames[i] == 0) {
            return false;
        }
    }

    memory_zero(vmm_phys_to_hhdm(stack_frame), PMM_PAGE_SIZE);
    memory_zero(vmm_phys_to_hhdm(arg_frame), PMM_PAGE_SIZE);
    copy_arg_string(vmm_phys_to_hhdm(arg_frame), arg);

    for (uint64_t i = 0; i < image_pages; i++) {
        uint8_t *code = vmm_phys_to_hhdm(image_frames[i]);
        uint64_t copied = i * PMM_PAGE_SIZE;
        uint64_t copy_size = header->image_size - copied;
        if (copy_size > PMM_PAGE_SIZE) {
            copy_size = PMM_PAGE_SIZE;
        }

        memory_zero(code, PMM_PAGE_SIZE);
        copy_bytes(code, file->data + header->header_size + copied, (size_t)copy_size);

        if (!vmm_map_page_in_space(address_space,
                                   USER_PROGRAM_BASE + i * PMM_PAGE_SIZE,
                                   image_frames[i],
                                   VMM_PAGE_USER | VMM_PAGE_WRITABLE)) {
            return false;
        }
    }

    if (!vmm_map_page_in_space(address_space,
                               USER_STACK_BASE,
                               stack_frame,
                               VMM_PAGE_USER | VMM_PAGE_WRITABLE)) {
        return false;
    }

    if (!vmm_map_page_in_space(address_space,
                               USER_ARG_BASE,
                               arg_frame,
                               VMM_PAGE_USER | VMM_PAGE_WRITABLE)) {
        return false;
    }

    *program_out = (struct loaded_user_program){
        .name = file->path,
        .address_space = address_space,
        .entry = USER_PROGRAM_BASE + header->entry_offset,
        .user_stack_top = USER_STACK_BASE + PMM_PAGE_SIZE,
        .arg0 = USER_ARG_BASE
    };

    return true;
}

bool user_loader_spawn(const char *path, const char *arg, uint64_t parent_id, uint64_t *pid_out)
{
    struct loaded_user_program program;
    if (!load_user_program(path, arg, &program)) {
        return false;
    }

    uint64_t pid = scheduler_create_user_process(program.name,
                                                 program.address_space,
                                                 program.entry,
                                                 program.user_stack_top,
                                                 program.arg0,
                                                 0,
                                                 parent_id);
    if (pid == 0) {
        return false;
    }

    if (pid_out != NULL) {
        *pid_out = pid;
    }

    serial_write("[loader] ");
    serial_write(path);
    serial_write(" loaded entry=");
    serial_write_u64_hex(program.entry);
    serial_write("\n");

    return true;
}

struct interrupt_frame *user_loader_exec_current_from_frame(const char *path,
                                                            const char *arg,
                                                            struct interrupt_frame *frame)
{
    struct loaded_user_program program;
    if (!load_user_program(path, arg, &program)) {
        return NULL;
    }

    serial_write("[loader] exec ");
    serial_write(path);
    serial_write(" entry=");
    serial_write_u64_hex(program.entry);
    serial_write("\n");

    return scheduler_exec_current_from_frame(frame,
                                             program.name,
                                             program.address_space,
                                             program.entry,
                                             program.user_stack_top,
                                             program.arg0,
                                             0);
}

void user_loader_start_init(void)
{
    if (!user_loader_spawn("/bin/init", NULL, 0, NULL)) {
        panic("loader could not start /bin/init");
    }

    init_started = true;
}

void user_loader_self_test(void)
{
    if (!init_started) {
        panic("loader self-test init not started");
    }

    serial_write("[loader] self-test ok\n");
}
