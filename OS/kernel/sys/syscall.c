#include "zenith/syscall.h"
#include "zenith/ui.h"

#include "zenith/console.h"
#include "zenith/cpu.h"
#include "zenith/framebuffer.h"
#include "zenith/initramfs.h"
#include "zenith/input.h"
#include "zenith/ipc.h"
#include "zenith/panic.h"
#include "zenith/pmm.h"
#include "zenith/scheduler.h"
#include "zenith/serial.h"
#include "zenith/user_loader.h"
#include "zenith/vmm.h"

#include <stdbool.h>
#include <stddef.h>

static bool initialized;
static volatile bool user_probe_done;
static volatile bool user_probe_sleep_seen;
static volatile uint64_t user_probe_result;

enum {
    USER_COPY_STRING_LIMIT = 256,
    FD_STDIN = 0,
    FD_STDOUT = 1,
    FD_STDERR = 2
};

#define USER_POINTER_LIMIT UINT64_C(0x0000800000000000)

static bool user_pointer_ok(uint64_t address, uint64_t size)
{
    if (address == 0 || address >= USER_POINTER_LIMIT) {
        return false;
    }

    if (size > USER_POINTER_LIMIT || address + size < address || address + size >= USER_POINTER_LIMIT) {
        return false;
    }

    return true;
}

static bool copy_user_string(uint64_t user_address, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0 || !user_pointer_ok(user_address, 1)) {
        return false;
    }

    const char *user = (const char *)(uintptr_t)user_address;

    for (size_t i = 0; i < buffer_size; i++) {
        if (!user_pointer_ok(user_address + i, 1)) {
            return false;
        }
        buffer[i] = user[i];
        if (buffer[i] == '\0') {
            return true;
        }
    }

    buffer[buffer_size - 1] = '\0';
    return false;
}

static bool string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) {
            return false;
        }
    }

    return *left == '\0' && *right == '\0';
}

static uint64_t copy_user_text(uint64_t user_address, uint64_t size, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0 || size == 0 || !user_pointer_ok(user_address, 1)) {
        return 0;
    }

    uint64_t copied = 0;
    const char *user = (const char *)(uintptr_t)user_address;
    uint64_t limit = size;
    if (limit >= buffer_size) {
        limit = buffer_size - 1;
    }

    while (copied < limit && user_pointer_ok(user_address + copied, 1)) {
        char value = user[copied];
        if (value == '\0') {
            break;
        }

        buffer[copied] = value;
        copied++;
    }

    buffer[copied] = '\0';
    return copied;
}

static uint64_t copy_file_to_user(uint64_t path_address, uint64_t dest_address, uint64_t max_size)
{
    char path[USER_COPY_STRING_LIMIT];

    if (!copy_user_string(path_address, path, sizeof(path)) ||
        max_size == 0 ||
        !user_pointer_ok(dest_address, max_size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    const struct initramfs_file *file = initramfs_lookup(path);
    if (file == NULL) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    uint64_t copy_size = file->size;
    if (copy_size >= max_size) {
        copy_size = max_size - 1;
    }

    uint8_t *dest = (uint8_t *)(uintptr_t)dest_address;
    for (uint64_t i = 0; i < copy_size; i++) {
        dest[i] = file->data[i];
    }
    dest[copy_size] = 0;

    return copy_size;
}

static uint64_t spawn_user_program(uint64_t path_address, uint64_t arg_address)
{
    char path[USER_COPY_STRING_LIMIT];
    char arg[USER_COPY_STRING_LIMIT];
    const char *arg_ptr = NULL;
    uint64_t pid = 0;

    if (!copy_user_string(path_address, path, sizeof(path))) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (arg_address != 0) {
        if (!copy_user_string(arg_address, arg, sizeof(arg))) {
            return ZENITH_SYSCALL_ERR_INVALID;
        }
        arg_ptr = arg;
    }

    if (!user_loader_spawn(path, arg_ptr, scheduler_current_task_id(), &pid)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    return pid;
}

static uint64_t copy_console_argument_to_user(uint64_t dest_address, uint64_t max_size)
{
    if (max_size == 0 || !user_pointer_ok(dest_address, max_size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    return console_copy_argument((char *)(uintptr_t)dest_address, max_size);
}

static uint64_t copy_console_line_to_user(uint64_t dest_address, uint64_t max_size)
{
    if (max_size == 0 || !user_pointer_ok(dest_address, max_size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    return console_read_line((char *)(uintptr_t)dest_address, max_size);
}

static uint64_t fd_open(uint64_t path_address)
{
    char path[USER_COPY_STRING_LIMIT];
    if (!copy_user_string(path_address, path, sizeof(path))) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (string_equals(path, "/dev/stdin")) {
        return FD_STDIN;
    }

    if (string_equals(path, "/dev/stdout") || string_equals(path, "/dev/console")) {
        return FD_STDOUT;
    }

    if (string_equals(path, "/dev/stderr")) {
        return FD_STDERR;
    }

    return ZENITH_SYSCALL_ERR_INVALID;
}

static uint64_t fd_close(uint64_t fd)
{
    if (fd <= FD_STDERR) {
        return 0;
    }

    return ZENITH_SYSCALL_ERR_INVALID;
}

static uint64_t fd_write(uint64_t fd, uint64_t user_address, uint64_t size)
{
    char text[USER_COPY_STRING_LIMIT];

    if (fd != FD_STDOUT && fd != FD_STDERR) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (size == 0) {
        return 0;
    }

    if (!user_pointer_ok(user_address, 1)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    uint64_t copied = copy_user_text(user_address, size, text, sizeof(text));
    console_write_line(text);
    return copied;
}

static uint64_t fd_read(uint64_t fd, uint64_t user_address, uint64_t size)
{
    if (fd != FD_STDIN || size == 0 || !user_pointer_ok(user_address, size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    uint64_t event = input_poll_packed();
    if (event == 0) {
        return 0;
    }

    uint64_t code = (event >> 32) & 0xffffff;
    uint8_t *dest = (uint8_t *)(uintptr_t)user_address;
    dest[0] = (uint8_t)code;
    return 1;
}

static uint64_t send_ui_message_from_user(uint64_t type,
                                          uint64_t target,
                                          uint64_t a,
                                          uint64_t b,
                                          uint64_t c,
                                          uint64_t text_address)
{
    char text[UI_TEXT_CAP];
    const char *text_ptr = NULL;

    if (text_address != 0) {
        if (!copy_user_string(text_address, text, sizeof(text))) {
            return ZENITH_SYSCALL_ERR_INVALID;
        }
        text_ptr = text;
    }

    return ui_send(scheduler_current_task_id(), target, type, a, b, c, text_ptr);
}

static uint64_t receive_ui_message_to_user(uint64_t message_address, uint64_t size)
{
    if (size < sizeof(struct ui_message) ||
        !user_pointer_ok(message_address, sizeof(struct ui_message))) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    struct ui_message message = {0};
    enum ui_result result = ui_receive(scheduler_current_task_id(), &message);
    if (result != UI_OK) {
        return result;
    }

    struct ui_message *dest = (struct ui_message *)(uintptr_t)message_address;
    *dest = message;
    return UI_OK;
}

void syscall_init(void)
{
    initialized = true;
    x86_64_syscall_enable();
    serial_write("[syscall] dispatch table initialized count=");
    serial_write_u64_dec(ZENITH_SYSCALL_COUNT);
    serial_write("\n");
}

uint64_t syscall_dispatch(uint64_t number,
                          uint64_t arg0,
                          uint64_t arg1,
                          uint64_t arg2,
                          uint64_t arg3,
                          uint64_t arg4,
                          uint64_t arg5)
{
    (void)arg4;
    (void)arg5;

    if (!initialized) {
        return ZENITH_SYSCALL_ERR_NOT_READY;
    }

    switch (number) {
    case ZENITH_SYSCALL_PING:
        return ZENITH_SYSCALL_PING_RESULT;
    case ZENITH_SYSCALL_IPC_SEND2: {
        const struct ipc_message message = {
            .sender = 0,
            .tag = arg1,
            .word_count = 2,
            .words = {arg2, arg3, 0, 0}
        };
        return ipc_send(arg0, &message);
    }
    case ZENITH_SYSCALL_TEST_COMPLETE:
        user_probe_result = arg0;
        user_probe_done = true;
        return ZENITH_SYSCALL_THREAD_EXIT;
    case ZENITH_SYSCALL_INIT_READY:
        user_probe_result = arg0;
        user_probe_done = true;
        return 0;
    case ZENITH_SYSCALL_PROC_SPAWN:
    case ZENITH_SYSCALL_PROC_EXEC:
    case ZENITH_SYSCALL_PROC_WAIT:
    case ZENITH_SYSCALL_PROC_EXIT:
    case ZENITH_SYSCALL_CONSOLE_WRITE:
    case ZENITH_SYSCALL_CONSOLE_PROMPT:
    case ZENITH_SYSCALL_CONSOLE_ARG_READ:
    case ZENITH_SYSCALL_READ:
    case ZENITH_SYSCALL_WRITE:
    case ZENITH_SYSCALL_OPEN:
    case ZENITH_SYSCALL_CLOSE:
    case ZENITH_SYSCALL_CONSOLE_READ_LINE:
    case ZENITH_SYSCALL_CONSOLE_CLEAR:
    case ZENITH_SYSCALL_UI_BIND_SESSION:
    case ZENITH_SYSCALL_UI_SEND:
    case ZENITH_SYSCALL_UI_RECV:
        return ZENITH_SYSCALL_ERR_INVALID;
    case ZENITH_SYSCALL_SLEEP_TICKS:
        return ZENITH_SYSCALL_ERR_INVALID;
    case ZENITH_SYSCALL_MAP_PAGE:
    case ZENITH_SYSCALL_FILE_READ:
    case ZENITH_SYSCALL_EVENT_POLL:
    case ZENITH_SYSCALL_DRAW_RECT:
    case ZENITH_SYSCALL_DRAW_TEXT:
    case ZENITH_SYSCALL_CONSOLE_INIT:
    case ZENITH_SYSCALL_CONSOLE_EVENT:
        return ZENITH_SYSCALL_ERR_INVALID;
    default:
        return ZENITH_SYSCALL_ERR_INVALID;
    }
}

struct interrupt_frame *syscall_handle_frame(struct interrupt_frame *frame)
{
    if (!initialized) {
        frame->rax = ZENITH_SYSCALL_ERR_NOT_READY;
        return frame;
    }

    switch (frame->rax) {
    case ZENITH_SYSCALL_PING:
        frame->rax = ZENITH_SYSCALL_PING_RESULT;
        return frame;
    case ZENITH_SYSCALL_IPC_SEND2: {
        const struct ipc_message message = {
            .sender = 0,
            .tag = frame->rsi,
            .word_count = 2,
            .words = {frame->rdx, frame->r10, 0, 0}
        };
        frame->rax = ipc_send(frame->rdi, &message);
        return frame;
    }
    case ZENITH_SYSCALL_SLEEP_TICKS:
        user_probe_sleep_seen = true;
        frame->rax = 0;
        return scheduler_sleep_current_from_frame(frame, frame->rdi);
    case ZENITH_SYSCALL_MAP_PAGE: {
        if ((frame->rdi & (PMM_PAGE_SIZE - 1)) != 0) {
            frame->rax = ZENITH_SYSCALL_ERR_INVALID;
            return frame;
        }

        uint64_t page = pmm_alloc_frame();
        if (page == 0) {
            frame->rax = ZENITH_SYSCALL_ERR_INVALID;
            return frame;
        }

        uint64_t flags = VMM_PAGE_USER;
        if ((frame->rsi & 1) != 0) {
            flags |= VMM_PAGE_WRITABLE;
        }

        if (!vmm_map_page_in_space(scheduler_current_address_space(), frame->rdi, page, flags)) {
            pmm_free_frame(page);
            frame->rax = ZENITH_SYSCALL_ERR_INVALID;
            return frame;
        }

        frame->rax = frame->rdi;
        return frame;
    }
    case ZENITH_SYSCALL_FILE_READ:
        frame->rax = copy_file_to_user(frame->rdi, frame->rsi, frame->rdx);
        return frame;
    case ZENITH_SYSCALL_EVENT_POLL:
        frame->rax = input_poll_packed();
        return frame;
    case ZENITH_SYSCALL_DRAW_RECT:
        framebuffer_fill_rect(frame->rdi, frame->rsi, frame->rdx, frame->r10, (uint32_t)frame->r8);
        frame->rax = 0;
        return frame;
    case ZENITH_SYSCALL_DRAW_TEXT: {
        char text[USER_COPY_STRING_LIMIT];
        if (!copy_user_string(frame->rdx, text, sizeof(text))) {
            frame->rax = ZENITH_SYSCALL_ERR_INVALID;
            return frame;
        }

        framebuffer_draw_text(frame->rdi, frame->rsi, text, (uint32_t)frame->r10, frame->r8);
        frame->rax = 0;
        return frame;
    }
    case ZENITH_SYSCALL_CONSOLE_INIT:
        console_init();
        frame->rax = 0;
        return frame;
    case ZENITH_SYSCALL_CONSOLE_EVENT:
        frame->rax = console_handle_key(frame->rdi);
        return frame;
    case ZENITH_SYSCALL_CONSOLE_WRITE: {
        char text[USER_COPY_STRING_LIMIT];
        if (!copy_user_string(frame->rdi, text, sizeof(text))) {
            frame->rax = ZENITH_SYSCALL_ERR_INVALID;
            return frame;
        }

        console_write_line(text);
        frame->rax = 0;
        return frame;
    }
    case ZENITH_SYSCALL_CONSOLE_PROMPT:
        console_render_prompt();
        frame->rax = 0;
        return frame;
    case ZENITH_SYSCALL_CONSOLE_ARG_READ:
        frame->rax = copy_console_argument_to_user(frame->rdi, frame->rsi);
        return frame;
    case ZENITH_SYSCALL_CONSOLE_READ_LINE:
        frame->rax = copy_console_line_to_user(frame->rdi, frame->rsi);
        return frame;
    case ZENITH_SYSCALL_CONSOLE_CLEAR:
        console_clear();
        frame->rax = 0;
        return frame;
    case ZENITH_SYSCALL_UI_BIND_SESSION:
        frame->rax = ui_bind_session(scheduler_current_task_id());
        return frame;
    case ZENITH_SYSCALL_UI_SEND:
        frame->rax = send_ui_message_from_user(frame->rdi,
                                               frame->rsi,
                                               frame->rdx,
                                               frame->r10,
                                               frame->r8,
                                               frame->r9);
        return frame;
    case ZENITH_SYSCALL_UI_RECV:
        frame->rax = receive_ui_message_to_user(frame->rdi, frame->rsi);
        return frame;
    case ZENITH_SYSCALL_READ:
        frame->rax = fd_read(frame->rdi, frame->rsi, frame->rdx);
        return frame;
    case ZENITH_SYSCALL_WRITE:
        frame->rax = fd_write(frame->rdi, frame->rsi, frame->rdx);
        return frame;
    case ZENITH_SYSCALL_OPEN:
        frame->rax = fd_open(frame->rdi);
        return frame;
    case ZENITH_SYSCALL_CLOSE:
        frame->rax = fd_close(frame->rdi);
        return frame;
    case ZENITH_SYSCALL_PROC_SPAWN:
        frame->rax = spawn_user_program(frame->rdi, frame->rsi);
        return frame;
    case ZENITH_SYSCALL_PROC_WAIT:
        return scheduler_wait_child_from_frame(frame, frame->rdi);
    case ZENITH_SYSCALL_PROC_EXIT:
        frame->rax = 0;
        return scheduler_exit_current_with_status_from_frame(frame, frame->rdi);
    case ZENITH_SYSCALL_PROC_EXEC: {
        char path[USER_COPY_STRING_LIMIT];
        if (!copy_user_string(frame->rdi, path, sizeof(path))) {
            frame->rax = ZENITH_SYSCALL_ERR_INVALID;
            return frame;
        }

        struct interrupt_frame *next = user_loader_exec_current_from_frame(path, NULL, frame);
        if (next == NULL) {
            frame->rax = ZENITH_SYSCALL_ERR_INVALID;
            return frame;
        }

        return next;
    }
    case ZENITH_SYSCALL_INIT_READY:
        user_probe_result = frame->rdi;
        user_probe_done = true;
        frame->rax = 0;
        return frame;
    case ZENITH_SYSCALL_TEST_COMPLETE:
        user_probe_result = frame->rdi;
        user_probe_done = true;
        frame->rax = 0;
        return scheduler_exit_current_from_frame(frame);
    default:
        frame->rax = ZENITH_SYSCALL_ERR_INVALID;
        return frame;
    }
}

void syscall_self_test(void)
{
    if (syscall_dispatch(ZENITH_SYSCALL_PING, 0, 0, 0, 0, 0, 0) !=
        ZENITH_SYSCALL_PING_RESULT) {
        panic("syscall ping self-test failed");
    }

    uint64_t port_id = 0;
    if (ipc_create_port(2, &port_id) != IPC_OK) {
        panic("syscall self-test could not create ipc port");
    }

    uint64_t result = syscall_dispatch(ZENITH_SYSCALL_IPC_SEND2,
                                       port_id,
                                       0x77,
                                       0xabc,
                                       0xdef,
                                       0,
                                       0);
    if (result != IPC_OK) {
        panic("syscall ipc send self-test failed");
    }

    struct ipc_message message = {0};
    if (ipc_receive(port_id, &message) != IPC_OK ||
        message.tag != 0x77 ||
        message.word_count != 2 ||
        message.words[0] != 0xabc ||
        message.words[1] != 0xdef) {
        panic("syscall ipc receive self-test failed");
    }

    serial_write("[syscall] self-test ok ping=");
    serial_write_u64_hex(ZENITH_SYSCALL_PING_RESULT);
    serial_write("\n");
}

void syscall_start_user_self_test(void)
{
    if (!initialized) {
        panic("syscall user self-test before init");
    }

    user_probe_done = false;
    user_probe_sleep_seen = false;
    user_probe_result = 0;
}

void syscall_user_self_test(void)
{
    if (!user_probe_done ||
        !user_probe_sleep_seen ||
        user_probe_result != ZENITH_SYSCALL_PING_RESULT) {
        panic("syscall user-mode self-test failed");
    }

    serial_write("[syscall] init process self-test ok result=");
    serial_write_u64_hex(user_probe_result);
    serial_write("\n");
}
