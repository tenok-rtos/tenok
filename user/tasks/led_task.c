#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "bsp_drv.h"

void led_task1(void)
{
    setprogname("led");

    int state = 1;
    while (1) {
        led_write(state);
        state = (state + 1) % 2;
        sleep(1);
    }
}

HOOK_USER_TASK(led_task1, 3, 512);
