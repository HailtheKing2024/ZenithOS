#ifndef ZENITH_COMPILER_H
#define ZENITH_COMPILER_H

#define ZENITH_USED __attribute__((used))
#define ZENITH_SECTION(name) __attribute__((section(name)))
#define ZENITH_ALIGN(bytes) __attribute__((aligned(bytes)))
#define ZENITH_NORETURN __attribute__((noreturn))
#define ZENITH_PACKED __attribute__((packed))

#endif
