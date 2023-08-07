#include <stddef.h>
#include "bsp_drv.h"
#include "uart.h"
#include "kernel.h"
#include "semaphore.h"
#include "bsp_drv.h"

extern sem_t sem_led;

int main(void)
{
    /* user-level driver initialization */
    led_init();
    bsp_driver_init();

    /*
     * depends on your needs, you can initialize
     * some semaphores / mutexes / message queues
     * before starting the os
     */
    sem_init(&sem_led, 0, 0);

    /* start the os */
    sched_start();

    return 0;
}
