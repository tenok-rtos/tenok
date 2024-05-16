#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "bsp_drv.h"

void led_task1(void)
{
    setprogname("led");

    int led_fd = open("/dev/led", 0);
    if (led_fd < 0) {
        printf("failed to open the LED.\n\r");
        exit(1);
    }

    int state = 1;
    while (1) {
        led_write(led_fd, state);
        state = (state + 1) % 2;
        sleep(1);
    }
}

HOOK_USER_TASK(led_task1, 3, 512);
