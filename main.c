#include <stddef.h>
#include "gpio.h"
#include "uart.h"
#include "kernel.h"
#include "semaphore.h"
#include "bsp_drv.h"

extern sem_t sem_led;

int main(void)
{
    led_init();
    bsp_driver_init();

    uart1_init(115200);

    /* uart3 should be initialized before starting the os
     * since the configuration of the nvic requires using
     * privilege mode */
    uart3_init(115200);

    sem_init(&sem_led, 0, 0);

    sched_start();

    return 0;
}
