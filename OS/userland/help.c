#include "zenith/user.h"

static const char line0[] = "COMMANDS: HELP CLEAR SYSINFO ECHO REBOOT";
static const char line1[] = "HELP SYSINFO AND ECHO RUN AS USER PROGRAMS";
static const char line2[] = "STDIN STDOUT STDERR FILE DESCRIPTORS ONLINE";

int main(const char *arg)
{
    (void)arg;

    z_write(1, line0, z_strlen(line0));
    z_write(1, line1, z_strlen(line1));
    z_write(1, line2, z_strlen(line2));
    return 0;
}
