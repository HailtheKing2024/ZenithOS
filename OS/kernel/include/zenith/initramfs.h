#ifndef ZENITH_INITRAMFS_H
#define ZENITH_INITRAMFS_H

#include <stddef.h>
#include <stdint.h>

struct initramfs_file {
    const char *path;
    const uint8_t *data;
    uint64_t size;
};

void initramfs_init(void);
const struct initramfs_file *initramfs_lookup(const char *path);
void initramfs_self_test(void);

#endif
