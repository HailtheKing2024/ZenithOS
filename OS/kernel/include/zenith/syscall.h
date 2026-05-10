#ifndef ZENITH_SYSCALL_H
#define ZENITH_SYSCALL_H

#include "zenith/interrupt_frame.h"

#include <stdint.h>

enum zenith_syscall_number {
    ZENITH_SYSCALL_PING = 0,
    ZENITH_SYSCALL_IPC_SEND2 = 1,
    ZENITH_SYSCALL_TEST_COMPLETE = 2,
    ZENITH_SYSCALL_SLEEP_TICKS = 3,
    ZENITH_SYSCALL_MAP_PAGE = 4,
    ZENITH_SYSCALL_FILE_READ = 5,
    ZENITH_SYSCALL_EVENT_POLL = 6,
    ZENITH_SYSCALL_DRAW_RECT = 7,
    ZENITH_SYSCALL_DRAW_TEXT = 8,
    ZENITH_SYSCALL_CONSOLE_INIT = 9,
    ZENITH_SYSCALL_CONSOLE_EVENT = 10,
    ZENITH_SYSCALL_INIT_READY = 11,
    ZENITH_SYSCALL_PROC_SPAWN = 12,
    ZENITH_SYSCALL_PROC_WAIT = 13,
    ZENITH_SYSCALL_PROC_EXIT = 14,
    ZENITH_SYSCALL_PROC_EXEC = 15,
    ZENITH_SYSCALL_CONSOLE_WRITE = 16,
    ZENITH_SYSCALL_CONSOLE_PROMPT = 17,
    ZENITH_SYSCALL_CONSOLE_ARG_READ = 18,
    ZENITH_SYSCALL_READ = 19,
    ZENITH_SYSCALL_WRITE = 20,
    ZENITH_SYSCALL_OPEN = 21,
    ZENITH_SYSCALL_CLOSE = 22,
    ZENITH_SYSCALL_CONSOLE_READ_LINE = 23,
    ZENITH_SYSCALL_CONSOLE_CLEAR = 24,
    ZENITH_SYSCALL_UI_BIND_SESSION = 25,
    ZENITH_SYSCALL_UI_SEND = 26,
    ZENITH_SYSCALL_UI_RECV = 27,
    ZENITH_SYSCALL_COUNT = 28
};

#define ZENITH_SYSCALL_PING_RESULT UINT64_C(0x5a454e4954480001)
#define ZENITH_SYSCALL_ERR_INVALID UINT64_MAX
#define ZENITH_SYSCALL_ERR_NOT_READY (UINT64_MAX - UINT64_C(1))
#define ZENITH_SYSCALL_THREAD_EXIT (UINT64_MAX - UINT64_C(2))

void syscall_init(void);
uint64_t syscall_dispatch(uint64_t number,
                          uint64_t arg0,
                          uint64_t arg1,
                          uint64_t arg2,
                          uint64_t arg3,
                          uint64_t arg4,
                          uint64_t arg5);
struct interrupt_frame *syscall_handle_frame(struct interrupt_frame *frame);
void syscall_self_test(void);
void syscall_start_user_self_test(void);
void syscall_user_self_test(void);

#endif
