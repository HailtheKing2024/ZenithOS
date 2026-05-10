#include "zenith/user.h"

uint64_t z_syscall6(uint64_t number,
                    uint64_t arg0,
                    uint64_t arg1,
                    uint64_t arg2,
                    uint64_t arg3,
                    uint64_t arg4,
                    uint64_t arg5)
{
    register uint64_t rax __asm__("rax") = number;
    register uint64_t rdi __asm__("rdi") = arg0;
    register uint64_t rsi __asm__("rsi") = arg1;
    register uint64_t rdx __asm__("rdx") = arg2;
    register uint64_t r10 __asm__("r10") = arg3;
    register uint64_t r8 __asm__("r8") = arg4;
    register uint64_t r9 __asm__("r9") = arg5;

    __asm__ volatile ("syscall"
                      : "+a"(rax)
                      : "D"(rdi), "S"(rsi), "d"(rdx),
                        "r"(r10), "r"(r8), "r"(r9)
                      : "rcx", "r11", "memory");

    return rax;
}

uint64_t z_ping(void)
{
    return z_syscall6(ZENITH_SYSCALL_PING, 0, 0, 0, 0, 0, 0);
}

uint64_t z_sleep_ticks(uint64_t ticks)
{
    return z_syscall6(ZENITH_SYSCALL_SLEEP_TICKS, ticks, 0, 0, 0, 0, 0);
}

uint64_t z_map_page(uint64_t address, uint64_t writable)
{
    return z_syscall6(ZENITH_SYSCALL_MAP_PAGE, address, writable, 0, 0, 0, 0);
}

uint64_t z_event_poll(void)
{
    return z_syscall6(ZENITH_SYSCALL_EVENT_POLL, 0, 0, 0, 0, 0, 0);
}

uint64_t z_draw_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color)
{
    return z_syscall6(ZENITH_SYSCALL_DRAW_RECT, x, y, width, height, color, 0);
}

uint64_t z_draw_text(uint64_t x, uint64_t y, const char *text, uint32_t color, uint64_t scale)
{
    return z_syscall6(ZENITH_SYSCALL_DRAW_TEXT,
                      x,
                      y,
                      (uint64_t)(uintptr_t)text,
                      color,
                      scale,
                      0);
}

uint64_t z_console_init(void)
{
    return z_syscall6(ZENITH_SYSCALL_CONSOLE_INIT, 0, 0, 0, 0, 0, 0);
}

uint64_t z_console_event(uint64_t code)
{
    return z_syscall6(ZENITH_SYSCALL_CONSOLE_EVENT, code, 0, 0, 0, 0, 0);
}

uint64_t z_console_prompt(void)
{
    return z_syscall6(ZENITH_SYSCALL_CONSOLE_PROMPT, 0, 0, 0, 0, 0, 0);
}

uint64_t z_console_arg_read(char *dest, uint64_t max_size)
{
    return z_syscall6(ZENITH_SYSCALL_CONSOLE_ARG_READ,
                      (uint64_t)(uintptr_t)dest, max_size, 0, 0, 0, 0);
}

uint64_t z_console_read_line(char *dest, uint64_t max_size)
{
    return z_syscall6(ZENITH_SYSCALL_CONSOLE_READ_LINE,
                      (uint64_t)(uintptr_t)dest, max_size, 0, 0, 0, 0);
}

uint64_t z_console_clear(void)
{
    return z_syscall6(ZENITH_SYSCALL_CONSOLE_CLEAR, 0, 0, 0, 0, 0, 0);
}

uint64_t z_spawn(const char *path, const char *arg)
{
    return z_syscall6(ZENITH_SYSCALL_PROC_SPAWN,
                      (uint64_t)(uintptr_t)path,
                      (uint64_t)(uintptr_t)arg,
                      0, 0, 0, 0);
}

uint64_t z_wait(uint64_t pid)
{
    return z_syscall6(ZENITH_SYSCALL_PROC_WAIT, pid, 0, 0, 0, 0, 0);
}

uint64_t z_exit(uint64_t status)
{
    return z_syscall6(ZENITH_SYSCALL_PROC_EXIT, status, 0, 0, 0, 0, 0);
}

uint64_t z_read(uint64_t fd, void *buffer, uint64_t size)
{
    return z_syscall6(ZENITH_SYSCALL_READ, fd, (uint64_t)(uintptr_t)buffer, size, 0, 0, 0);
}

uint64_t z_write(uint64_t fd, const void *buffer, uint64_t size)
{
    return z_syscall6(ZENITH_SYSCALL_WRITE, fd, (uint64_t)(uintptr_t)buffer, size, 0, 0, 0);
}

uint64_t z_open(const char *path)
{
    return z_syscall6(ZENITH_SYSCALL_OPEN, (uint64_t)(uintptr_t)path, 0, 0, 0, 0, 0);
}

uint64_t z_close(uint64_t fd)
{
    return z_syscall6(ZENITH_SYSCALL_CLOSE, fd, 0, 0, 0, 0, 0);
}

uint64_t z_strlen(const char *value)
{
    uint64_t length = 0;

    if (value == 0) {
        return 0;
    }

    while (value[length] != '\0') {
        length++;
    }

    return length;
}

uint64_t z_ui_bind_session(void)
{
    return z_syscall6(ZENITH_SYSCALL_UI_BIND_SESSION, 0, 0, 0, 0, 0, 0);
}

uint64_t z_ui_send(uint64_t type,
                   uint64_t target,
                   uint64_t a,
                   uint64_t b,
                   uint64_t c,
                   const char *text)
{
    return z_syscall6(ZENITH_SYSCALL_UI_SEND,
                      type,
                      target,
                      a,
                      b,
                      c,
                      (uint64_t)(uintptr_t)text);
}

uint64_t z_ui_recv(struct z_ui_message *message)
{
    return z_syscall6(ZENITH_SYSCALL_UI_RECV,
                      (uint64_t)(uintptr_t)message,
                      sizeof(*message),
                      0,
                      0,
                      0,
                      0);
}
