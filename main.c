#include <stddef.h>
#include "gpio.h"
#include "uart.h"
#include "kernel.h"
#include "syscall.h"
#include "semaphore.h"
#include "fs.h"
#include "rom_dev.h"
#include "examples.h"
#include "bsp_drv.h"
#include "tasks.h"

extern sem_t sem_led;

void first(void *param)
{
    set_program_name("first");

    /* the service should be initialized before
     * forking any new tasks */
    os_service_init();

    sem_init(&sem_led, 0, 0);
    rom_dev_init();
    mount("/dev/rom", "/");
    serial0_init();

    if(!fork()) led_task1();
    if(!fork()) led_task2();
    if(!fork()) shell_task();

    //run_fifo_example();
    //run_mutex_example();
    //run_mqueue_example();

    while(1); //idle loop when no task is ready
}

int main(void)
{
    led_init();
    bsp_driver_init();

    uart1_init(115200);

    /* uart3 should be initialized before starting the os
     * since the configuration of the nvic requires using
     * privilege mode */
    uart3_init(115200);

    sched_start(first);

    return 0;
}
