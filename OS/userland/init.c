#include "zenith/user.h"

static const char session_path[] = "/bin/session";
static const char shell_path[] = "/bin/sh";

int main(const char *arg)
{
    (void)arg;

    uint64_t ping = z_ping();
    z_console_init();
    z_sleep_ticks(2);
    z_syscall6(ZENITH_SYSCALL_INIT_READY, ping, 0, 0, 0, 0, 0);

    for (;;) {
        uint64_t pid = z_spawn(session_path, 0);
        if (pid == UINT64_MAX) {
            z_console_init();
            pid = z_spawn(shell_path, 0);
        }

        if (pid != UINT64_MAX) {
            z_wait(pid);
        }

        z_sleep_ticks(1);
    }
}
