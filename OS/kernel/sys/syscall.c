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
    FD_STDERR = 2,
    FD_TABLE_CAP = 32,
    MEM_FILE_COUNT = 8,
    MEM_FILE_NAME_CAP = 64,
    MEM_FILE_DATA_CAP = 4096,
    PIPE_COUNT = 4,
    PIPE_CAP = 4096
};

#define USER_POINTER_LIMIT UINT64_C(0x0000800000000000)

enum fd_kind {
    FD_KIND_FREE = 0,
    FD_KIND_STDIN,
    FD_KIND_STDOUT,
    FD_KIND_STDERR,
    FD_KIND_MEM_FILE,
    FD_KIND_PIPE_READ,
    FD_KIND_PIPE_WRITE
};

struct mem_file {
    bool used;
    char path[MEM_FILE_NAME_CAP];
    uint8_t data[MEM_FILE_DATA_CAP];
    uint64_t size;
};

struct pipe_buffer {
    bool used;
    uint64_t read_refs;
    uint64_t write_refs;
    uint64_t read_index;
    uint64_t write_index;
    uint64_t size;
    uint8_t data[PIPE_CAP];
};

struct fd_entry {
    bool used;
    enum fd_kind kind;
    uint64_t object;
    uint64_t offset;
};

static bool fd_initialized;
static struct fd_entry fd_table[FD_TABLE_CAP];
static struct mem_file mem_files[MEM_FILE_COUNT];
static struct pipe_buffer pipe_buffers[PIPE_COUNT];
static char kernel_cwd[MEM_FILE_NAME_CAP] = "/";

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

static void copy_kernel_text(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return;
    }

    size_t index = 0;
    while (index + 1 < dest_size && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }

    dest[index] = '\0';
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

static void fd_init_once(void)
{
    if (fd_initialized) {
        return;
    }

    for (size_t i = 0; i < FD_TABLE_CAP; i++) {
        fd_table[i].used = false;
        fd_table[i].kind = FD_KIND_FREE;
        fd_table[i].object = 0;
        fd_table[i].offset = 0;
    }

    fd_table[FD_STDIN] = (struct fd_entry){.used = true, .kind = FD_KIND_STDIN};
    fd_table[FD_STDOUT] = (struct fd_entry){.used = true, .kind = FD_KIND_STDOUT};
    fd_table[FD_STDERR] = (struct fd_entry){.used = true, .kind = FD_KIND_STDERR};
    fd_initialized = true;
}

static bool fd_valid(uint64_t fd)
{
    fd_init_once();
    return fd < FD_TABLE_CAP && fd_table[fd].used;
}

static uint64_t fd_alloc(enum fd_kind kind, uint64_t object, uint64_t offset)
{
    fd_init_once();

    for (uint64_t fd = FD_STDERR + 1; fd < FD_TABLE_CAP; fd++) {
        if (!fd_table[fd].used) {
            fd_table[fd].used = true;
            fd_table[fd].kind = kind;
            fd_table[fd].object = object;
            fd_table[fd].offset = offset;
            return fd;
        }
    }

    return ZENITH_SYSCALL_ERR_INVALID;
}

static struct mem_file *lookup_mem_file(const char *path)
{
    for (size_t i = 0; i < MEM_FILE_COUNT; i++) {
        if (mem_files[i].used && string_equals(mem_files[i].path, path)) {
            return &mem_files[i];
        }
    }

    return NULL;
}

static struct mem_file *open_mem_file(const char *path)
{
    struct mem_file *file = lookup_mem_file(path);
    if (file != NULL) {
        return file;
    }

    for (size_t i = 0; i < MEM_FILE_COUNT; i++) {
        if (!mem_files[i].used) {
            mem_files[i].used = true;
            mem_files[i].size = 0;
            copy_kernel_text(mem_files[i].path, sizeof(mem_files[i].path), path);
            return &mem_files[i];
        }
    }

    return NULL;
}

static uint64_t mem_file_index(const struct mem_file *file)
{
    return (uint64_t)(file - mem_files);
}

static void release_pipe_reference(struct fd_entry *entry)
{
    if (entry->object >= PIPE_COUNT) {
        return;
    }

    struct pipe_buffer *pipe = &pipe_buffers[entry->object];
    if (!pipe->used) {
        return;
    }

    if (entry->kind == FD_KIND_PIPE_READ && pipe->read_refs > 0) {
        pipe->read_refs--;
    } else if (entry->kind == FD_KIND_PIPE_WRITE && pipe->write_refs > 0) {
        pipe->write_refs--;
    }

    if (pipe->read_refs == 0 && pipe->write_refs == 0) {
        pipe->used = false;
        pipe->read_index = 0;
        pipe->write_index = 0;
        pipe->size = 0;
    }
}

static void fd_release(uint64_t fd)
{
    if (fd >= FD_TABLE_CAP || !fd_table[fd].used || fd <= FD_STDERR) {
        return;
    }

    release_pipe_reference(&fd_table[fd]);
    fd_table[fd].used = false;
    fd_table[fd].kind = FD_KIND_FREE;
    fd_table[fd].object = 0;
    fd_table[fd].offset = 0;
}

static uint64_t fd_open(uint64_t path_address)
{
    char path[USER_COPY_STRING_LIMIT];
    if (!copy_user_string(path_address, path, sizeof(path))) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    fd_init_once();

    if (string_equals(path, "/dev/stdin")) {
        return FD_STDIN;
    }

    if (string_equals(path, "/dev/stdout") || string_equals(path, "/dev/console")) {
        return FD_STDOUT;
    }

    if (string_equals(path, "/dev/stderr")) {
        return FD_STDERR;
    }

    struct mem_file *file = open_mem_file(path);
    if (file == NULL) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    return fd_alloc(FD_KIND_MEM_FILE, mem_file_index(file), 0);
}

static uint64_t fd_close(uint64_t fd)
{
    if (!fd_valid(fd)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    fd_release(fd);
    return 0;
}

static uint64_t fd_write_mem_file(struct fd_entry *entry, uint64_t user_address, uint64_t size)
{
    if (entry->object >= MEM_FILE_COUNT || !mem_files[entry->object].used) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    struct mem_file *file = &mem_files[entry->object];
    if (entry->offset >= MEM_FILE_DATA_CAP) {
        return 0;
    }

    uint64_t writable = MEM_FILE_DATA_CAP - entry->offset;
    if (size < writable) {
        writable = size;
    }

    if (!user_pointer_ok(user_address, writable)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (entry->offset == 0) {
        file->size = 0;
    }

    const uint8_t *src = (const uint8_t *)(uintptr_t)user_address;
    for (uint64_t i = 0; i < writable; i++) {
        file->data[entry->offset + i] = src[i];
    }

    entry->offset += writable;
    if (file->size < entry->offset) {
        file->size = entry->offset;
    }

    return writable;
}

static uint64_t fd_write_pipe(struct fd_entry *entry, uint64_t user_address, uint64_t size)
{
    if (entry->object >= PIPE_COUNT || !pipe_buffers[entry->object].used) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (!user_pointer_ok(user_address, size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    struct pipe_buffer *pipe = &pipe_buffers[entry->object];
    const uint8_t *src = (const uint8_t *)(uintptr_t)user_address;
    uint64_t written = 0;

    while (written < size && pipe->size < PIPE_CAP) {
        pipe->data[pipe->write_index] = src[written++];
        pipe->write_index = (pipe->write_index + 1) % PIPE_CAP;
        pipe->size++;
    }

    return written;
}

static uint64_t fd_write(uint64_t fd, uint64_t user_address, uint64_t size)
{
    char text[USER_COPY_STRING_LIMIT];

    if (!fd_valid(fd)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (size == 0) {
        return 0;
    }

    struct fd_entry *entry = &fd_table[fd];
    if (entry->kind == FD_KIND_STDOUT || entry->kind == FD_KIND_STDERR) {
        if (!user_pointer_ok(user_address, 1)) {
            return ZENITH_SYSCALL_ERR_INVALID;
        }

        uint64_t copied = copy_user_text(user_address, size, text, sizeof(text));
        console_write_line(text);
        return copied;
    }

    if (entry->kind == FD_KIND_MEM_FILE) {
        return fd_write_mem_file(entry, user_address, size);
    }

    if (entry->kind == FD_KIND_PIPE_WRITE) {
        return fd_write_pipe(entry, user_address, size);
    }

    return ZENITH_SYSCALL_ERR_INVALID;
}

static uint64_t fd_read_mem_file(struct fd_entry *entry, uint64_t user_address, uint64_t size)
{
    if (entry->object >= MEM_FILE_COUNT || !mem_files[entry->object].used) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    struct mem_file *file = &mem_files[entry->object];
    if (entry->offset >= file->size) {
        return 0;
    }

    uint64_t readable = file->size - entry->offset;
    if (size < readable) {
        readable = size;
    }

    if (!user_pointer_ok(user_address, readable)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    uint8_t *dest = (uint8_t *)(uintptr_t)user_address;
    for (uint64_t i = 0; i < readable; i++) {
        dest[i] = file->data[entry->offset + i];
    }

    entry->offset += readable;
    return readable;
}

static uint64_t fd_read_pipe(struct fd_entry *entry, uint64_t user_address, uint64_t size)
{
    if (entry->object >= PIPE_COUNT || !pipe_buffers[entry->object].used) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (!user_pointer_ok(user_address, size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    struct pipe_buffer *pipe = &pipe_buffers[entry->object];
    uint8_t *dest = (uint8_t *)(uintptr_t)user_address;
    uint64_t copied = 0;

    while (copied < size && pipe->size > 0) {
        dest[copied++] = pipe->data[pipe->read_index];
        pipe->read_index = (pipe->read_index + 1) % PIPE_CAP;
        pipe->size--;
    }

    return copied;
}

static uint64_t fd_read(uint64_t fd, uint64_t user_address, uint64_t size)
{
    if (!fd_valid(fd) || size == 0 || !user_pointer_ok(user_address, size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    struct fd_entry *entry = &fd_table[fd];
    if (entry->kind == FD_KIND_STDIN) {
        uint64_t event = input_poll_packed();
        if (event == 0) {
            return 0;
        }

        uint64_t code = (event >> 32) & 0xffffff;
        uint8_t *dest = (uint8_t *)(uintptr_t)user_address;
        dest[0] = (uint8_t)code;
        return 1;
    }

    if (entry->kind == FD_KIND_MEM_FILE) {
        return fd_read_mem_file(entry, user_address, size);
    }

    if (entry->kind == FD_KIND_PIPE_READ) {
        return fd_read_pipe(entry, user_address, size);
    }

    return ZENITH_SYSCALL_ERR_INVALID;
}

static uint64_t fd_dup2(uint64_t old_fd, uint64_t new_fd)
{
    if (!fd_valid(old_fd) || new_fd >= FD_TABLE_CAP) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (old_fd == new_fd) {
        return new_fd;
    }

    fd_release(new_fd);
    fd_table[new_fd] = fd_table[old_fd];

    if (fd_table[new_fd].kind == FD_KIND_PIPE_READ && fd_table[new_fd].object < PIPE_COUNT) {
        pipe_buffers[fd_table[new_fd].object].read_refs++;
    } else if (fd_table[new_fd].kind == FD_KIND_PIPE_WRITE && fd_table[new_fd].object < PIPE_COUNT) {
        pipe_buffers[fd_table[new_fd].object].write_refs++;
    }

    return new_fd;
}

static uint64_t pipe_create_user(uint64_t fds_address)
{
    if (!user_pointer_ok(fds_address, sizeof(uint64_t) * 2)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    for (uint64_t i = 0; i < PIPE_COUNT; i++) {
        if (!pipe_buffers[i].used) {
            pipe_buffers[i].used = true;
            pipe_buffers[i].read_refs = 1;
            pipe_buffers[i].write_refs = 1;
            pipe_buffers[i].read_index = 0;
            pipe_buffers[i].write_index = 0;
            pipe_buffers[i].size = 0;

            uint64_t read_fd = fd_alloc(FD_KIND_PIPE_READ, i, 0);
            uint64_t write_fd = fd_alloc(FD_KIND_PIPE_WRITE, i, 0);
            if (read_fd == ZENITH_SYSCALL_ERR_INVALID ||
                write_fd == ZENITH_SYSCALL_ERR_INVALID) {
                if (read_fd != ZENITH_SYSCALL_ERR_INVALID) {
                    fd_release(read_fd);
                }
                pipe_buffers[i].used = false;
                return ZENITH_SYSCALL_ERR_INVALID;
            }

            uint64_t *fds = (uint64_t *)(uintptr_t)fds_address;
            fds[0] = read_fd;
            fds[1] = write_fd;
            return 0;
        }
    }

    return ZENITH_SYSCALL_ERR_INVALID;
}

static uint64_t change_directory(uint64_t path_address)
{
    char path[MEM_FILE_NAME_CAP];

    if (!copy_user_string(path_address, path, sizeof(path)) || path[0] == '\0') {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    if (path[0] == '/') {
        copy_kernel_text(kernel_cwd, sizeof(kernel_cwd), path);
    } else {
        kernel_cwd[0] = '/';
        kernel_cwd[1] = '\0';
        copy_kernel_text(kernel_cwd + 1, sizeof(kernel_cwd) - 1, path);
    }

    return 0;
}

static uint64_t copy_cwd_to_user(uint64_t dest_address, uint64_t max_size)
{
    if (max_size == 0 || !user_pointer_ok(dest_address, max_size)) {
        return ZENITH_SYSCALL_ERR_INVALID;
    }

    char *dest = (char *)(uintptr_t)dest_address;
    uint64_t copied = 0;
    while (copied + 1 < max_size && kernel_cwd[copied] != '\0') {
        dest[copied] = kernel_cwd[copied];
        copied++;
    }
    dest[copied] = '\0';
    return copied;
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
    case ZENITH_SYSCALL_PIPE:
        return pipe_create_user(arg0);
    case ZENITH_SYSCALL_DUP2:
        return fd_dup2(arg0, arg1);
    case ZENITH_SYSCALL_GETPID:
        return scheduler_current_task_id();
    case ZENITH_SYSCALL_GETPPID:
        return scheduler_current_parent_id();
    case ZENITH_SYSCALL_REBOOT:
        x86_64_reboot();
    case ZENITH_SYSCALL_CHDIR:
        return change_directory(arg0);
    case ZENITH_SYSCALL_GETCWD:
        return copy_cwd_to_user(arg0, arg1);
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
    case ZENITH_SYSCALL_PIPE:
        frame->rax = pipe_create_user(frame->rdi);
        return frame;
    case ZENITH_SYSCALL_DUP2:
        frame->rax = fd_dup2(frame->rdi, frame->rsi);
        return frame;
    case ZENITH_SYSCALL_GETPID:
        frame->rax = scheduler_current_task_id();
        return frame;
    case ZENITH_SYSCALL_GETPPID:
        frame->rax = scheduler_current_parent_id();
        return frame;
    case ZENITH_SYSCALL_REBOOT:
        x86_64_reboot();
    case ZENITH_SYSCALL_CHDIR:
        frame->rax = change_directory(frame->rdi);
        return frame;
    case ZENITH_SYSCALL_GETCWD:
        frame->rax = copy_cwd_to_user(frame->rdi, frame->rsi);
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
