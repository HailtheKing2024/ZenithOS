#ifndef ZENITH_KHEAP_H
#define ZENITH_KHEAP_H

#include <stddef.h>

void kheap_init(void);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t alignment);
void kheap_self_test(void);

#endif

