#ifndef ZENITH_USER_LOADER_H
#define ZENITH_USER_LOADER_H

#include "zenith/interrupt_frame.h"

#include <stdbool.h>
#include <stdint.h>

#define USER_PROGRAM_MAGIC UINT32_C(0x314e425a)
#define USER_PROGRAM_BASE UINT64_C(0x0000000000400000)
#define USER_STACK_BASE UINT64_C(0x0000000000800000)
#define USER_HEAP_BASE UINT64_C(0x0000000000900000)
#define USER_ARG_BASE UINT64_C(0x0000000000a00000)
#define USER_PROGRAM_MAX_IMAGE_PAGES UINT64_C(8)
#define USER_PROGRAM_MAX_IMAGE_SIZE (USER_PROGRAM_MAX_IMAGE_PAGES * UINT64_C(4096))

struct user_program_header {
    uint32_t magic;
    uint32_t header_size;
    uint64_t entry_offset;
    uint64_t image_size;
    uint64_t stack_pages;
};

void user_loader_start_init(void);
bool user_loader_spawn(const char *path, const char *arg, uint64_t parent_id, uint64_t *pid_out);
struct interrupt_frame *user_loader_exec_current_from_frame(const char *path,
                                                            const char *arg,
                                                            struct interrupt_frame *frame);
void user_loader_self_test(void);

#endif
