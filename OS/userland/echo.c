#include "zenith/user.h"

int main(const char *arg)
{
    uint64_t length = z_strlen(arg);

    if (length == 0) {
        z_write(1, "", 1);
    } else {
        z_write(1, arg, length);
    }

    return 0;
}
