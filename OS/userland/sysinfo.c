#include "zenith/user.h"

static const char line0[] = "ZENITHOS X86_64 USERMODE READY";
static const char line1[] = "PROCESS SPAWN WAIT EXIT ONLINE";
static const char line2[] = "INITRAMFS PROGRAM: /BIN/SYSINFO";

int main(const char *arg)
{
    (void)arg;

    z_write(1, line0, z_strlen(line0));
    z_write(1, line1, z_strlen(line1));
    z_write(1, line2, z_strlen(line2));
    return 0;
}
