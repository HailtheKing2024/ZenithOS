#include "zenith/user.h"

#include <stdbool.h>

enum {
    SHELL_LINE_CAP = 72
};

static const char help_path[] = "/bin/help";
static const char sysinfo_path[] = "/bin/sysinfo";
static const char echo_path[] = "/bin/echo";

static bool string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) {
            return false;
        }
    }

    return *left == '\0' && *right == '\0';
}

static bool string_starts_with(const char *value, const char *prefix)
{
    while (*prefix != '\0') {
        if (*value++ != *prefix++) {
            return false;
        }
    }

    return true;
}

static void write_line(const char *text)
{
    z_write(1, text, z_strlen(text));
}

static void spawn_and_wait(const char *path, const char *arg)
{
    uint64_t pid = z_spawn(path, arg);
    if (pid == UINT64_MAX) {
        write_line("EXEC FAILED");
        return;
    }

    z_wait(pid);
}

static void read_command(char *line)
{
    for (;;) {
        uint64_t event = z_event_poll();
        uint64_t code = (event >> 32) & 0xffffff;
        if (code != 0) {
            z_console_event(code);
        }

        uint64_t count = z_console_read_line(line, sizeof(line));
        if (count != UINT64_MAX) {
            return;
        }

        z_sleep_ticks(1);
    }
}

int main(const char *arg)
{
    (void)arg;
    char *line = (char *)(uintptr_t)USER_HEAP_BASE;

    z_map_page(USER_HEAP_BASE, 1);

    for (;;) {
        z_console_prompt();
        read_command(line);

        if (line[0] == '\0') {
            continue;
        }

        if (string_equals(line, "HELP")) {
            spawn_and_wait(help_path, 0);
        } else if (string_equals(line, "SYSINFO")) {
            spawn_and_wait(sysinfo_path, 0);
        } else if (string_equals(line, "CLEAR")) {
            z_console_clear();
        } else if (string_equals(line, "REBOOT")) {
            write_line("REBOOT IS NOT WIRED YET");
        } else if (string_starts_with(line, "ECHO ")) {
            spawn_and_wait(echo_path, line + 5);
        } else if (string_equals(line, "ECHO")) {
            spawn_and_wait(echo_path, "");
        } else if (string_equals(line, "EXIT")) {
            z_exit(0);
        } else {
            write_line("UNKNOWN COMMAND");
        }
    }
}
