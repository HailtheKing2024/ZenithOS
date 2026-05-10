#include "zenith/initramfs.h"

#include "zenith/panic.h"
#include "zenith/serial.h"

#include <stdbool.h>
#include <stddef.h>

#include "initramfs_userland.inc"

static const uint8_t welcome_text[] = "ZENITHOS INITRAMFS READY";
static const uint8_t theme_text[] = "ADWAITA STYLE TOKENS";

static bool string_equals(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a++ != *b++) {
            return false;
        }
    }

    return *a == '\0' && *b == '\0';
}

static const struct initramfs_file files[] = {
    {.path = "/bin/init", .data = userland_init_image, .size = sizeof(userland_init_image)},
    {.path = "/bin/session", .data = userland_session_image, .size = sizeof(userland_session_image)},
    {.path = "/bin/terminal", .data = userland_terminal_image, .size = sizeof(userland_terminal_image)},
    {.path = "/bin/settings", .data = userland_settings_image, .size = sizeof(userland_settings_image)},
    {.path = "/bin/launcher", .data = userland_launcher_image, .size = sizeof(userland_launcher_image)},
    {.path = "/bin/sh", .data = userland_sh_image, .size = sizeof(userland_sh_image)},
    {.path = "/bin/help", .data = userland_help_image, .size = sizeof(userland_help_image)},
    {.path = "/bin/sysinfo", .data = userland_sysinfo_image, .size = sizeof(userland_sysinfo_image)},
    {.path = "/bin/echo", .data = userland_echo_image, .size = sizeof(userland_echo_image)},
    {.path = "/etc/zenith/welcome.txt", .data = welcome_text, .size = sizeof(welcome_text) - 1},
    {.path = "/usr/share/zenith/theme.conf", .data = theme_text, .size = sizeof(theme_text) - 1}
};

void initramfs_init(void)
{
    serial_write("[initramfs] mounted files=");
    serial_write_u64_dec(sizeof(files) / sizeof(files[0]));
    serial_write("\n");
}

const struct initramfs_file *initramfs_lookup(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        if (string_equals(path, files[i].path)) {
            return &files[i];
        }
    }

    return NULL;
}

void initramfs_self_test(void)
{
    if (initramfs_lookup("/bin/init") == NULL ||
        initramfs_lookup("/bin/session") == NULL ||
        initramfs_lookup("/bin/terminal") == NULL ||
        initramfs_lookup("/bin/settings") == NULL ||
        initramfs_lookup("/bin/launcher") == NULL ||
        initramfs_lookup("/bin/sh") == NULL ||
        initramfs_lookup("/bin/help") == NULL ||
        initramfs_lookup("/bin/sysinfo") == NULL ||
        initramfs_lookup("/bin/echo") == NULL ||
        initramfs_lookup("/etc/zenith/welcome.txt") == NULL) {
        panic("initramfs self-test failed");
    }

    serial_write("[initramfs] self-test ok\n");
}
