#ifndef ZENITH_USER_H
#define ZENITH_USER_H

#include <stdint.h>

#define USER_HEAP_BASE UINT64_C(0x0000000000900000)

#define CONSOLE_ACTION_NONE UINT64_C(0)
#define CONSOLE_ACTION_LINE_READY UINT64_C(1)

#define Z_INPUT_EVENT_KEY UINT64_C(1)
#define Z_INPUT_EVENT_MOUSE UINT64_C(2)
#define Z_MOUSE_LEFT UINT64_C(1)
#define Z_MOUSE_RIGHT UINT64_C(2)
#define Z_MOUSE_MIDDLE UINT64_C(4)

#define Z_UI_TEXT_CAP 64

enum z_ui_role {
    Z_UI_ROLE_TERMINAL = 1,
    Z_UI_ROLE_SETTINGS = 2,
    Z_UI_ROLE_LAUNCHER = 3
};

enum z_ui_message_type {
    Z_UI_MSG_NONE = 0,
    Z_UI_MSG_REGISTER = 1,
    Z_UI_MSG_SET_LINE = 2,
    Z_UI_MSG_CLEAR = 3,
    Z_UI_MSG_INPUT_KEY = 4,
    Z_UI_MSG_INPUT_MOUSE = 5,
    Z_UI_MSG_COMMAND = 6,
    Z_UI_MSG_NOTIFY = 7,
    Z_UI_MSG_CLOSE = 8
};

enum z_ui_command {
    Z_UI_CMD_NONE = 0,
    Z_UI_CMD_SHOW_TERMINAL = 1,
    Z_UI_CMD_SHOW_SETTINGS = 2,
    Z_UI_CMD_SHOW_OVERVIEW = 3,
    Z_UI_CMD_NOTIFY = 4,
    Z_UI_CMD_BRIDGE_DATA = 5
};

struct z_ui_message {
    uint64_t type;
    uint64_t source;
    uint64_t target;
    uint64_t a;
    uint64_t b;
    uint64_t c;
    char text[Z_UI_TEXT_CAP];
};

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
    ZENITH_SYSCALL_PIPE = 28,
    ZENITH_SYSCALL_DUP2 = 29,
    ZENITH_SYSCALL_GETPID = 30,
    ZENITH_SYSCALL_GETPPID = 31,
    ZENITH_SYSCALL_REBOOT = 32,
    ZENITH_SYSCALL_CHDIR = 33,
    ZENITH_SYSCALL_GETCWD = 34
};

uint64_t z_syscall6(uint64_t number,
                    uint64_t arg0,
                    uint64_t arg1,
                    uint64_t arg2,
                    uint64_t arg3,
                    uint64_t arg4,
                    uint64_t arg5);
uint64_t z_ping(void);
uint64_t z_sleep_ticks(uint64_t ticks);
uint64_t z_map_page(uint64_t address, uint64_t writable);
uint64_t z_event_poll(void);
uint64_t z_draw_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color);
uint64_t z_draw_text(uint64_t x, uint64_t y, const char *text, uint32_t color, uint64_t scale);
uint64_t z_console_init(void);
uint64_t z_console_event(uint64_t code);
uint64_t z_console_prompt(void);
uint64_t z_console_arg_read(char *dest, uint64_t max_size);
uint64_t z_console_read_line(char *dest, uint64_t max_size);
uint64_t z_console_clear(void);
uint64_t z_spawn(const char *path, const char *arg);
uint64_t z_wait(uint64_t pid);
uint64_t z_exit(uint64_t status);
uint64_t z_read(uint64_t fd, void *buffer, uint64_t size);
uint64_t z_write(uint64_t fd, const void *buffer, uint64_t size);
uint64_t z_open(const char *path);
uint64_t z_close(uint64_t fd);
uint64_t z_pipe(uint64_t *fds);
uint64_t z_dup2(uint64_t old_fd, uint64_t new_fd);
uint64_t z_getpid(void);
uint64_t z_getppid(void);
uint64_t z_reboot(void);
uint64_t z_chdir(const char *path);
uint64_t z_getcwd(char *dest, uint64_t max_size);
uint64_t z_strlen(const char *value);
uint64_t z_ui_bind_session(void);
uint64_t z_ui_send(uint64_t type,
                   uint64_t target,
                   uint64_t a,
                   uint64_t b,
                   uint64_t c,
                   const char *text);
uint64_t z_ui_recv(struct z_ui_message *message);

#endif
