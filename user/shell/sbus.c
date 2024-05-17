#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kconfig.h"
#include "sbus.h"
#include "shell.h"

int sbus(int argc, char *argv[])
{
    int sbus_fd = open("/dev/sbus", O_RDWR);

    if (sbus_fd < 0) {
        shell_puts("no sbus receiver found.\n\r");
        return 0;
    }

    sbus_t sbus;
    read(sbus_fd, &sbus, sizeof(sbus_t));

    char str[PRINT_SIZE_MAX];

    /* Mapped signal */
    char *safety = sbus.dual_switch1 ? "[disarmed]" : "[armed]";
    snprintf(str, PRINT_SIZE_MAX,
             "%s roll: %d, pitch: %d, yaw: %d, throttle: %d%%\n\r", safety,
             (int) sbus.roll, (int) sbus.pitch, (int) sbus.yaw,
             (int) sbus.throttle);
    shell_puts(str);

    /* Raw data */
    int i;
    for (i = 0; i < 16; i++) {
        snprintf(str, PRINT_SIZE_MAX, "channel %d: %d\n\r", i, sbus.rc_val[i]);
        shell_puts(str);
    }

    return 0;
}

HOOK_SHELL_CMD("sbus", sbus);
