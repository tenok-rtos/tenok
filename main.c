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

void first(void)
{
    set_program_name("first");

    /* the service should be initialized before
     * forking any new tasks */
    os_service_init();

    sem_init(&sem_led, 0, 0);
    rom_dev_init();
    mount("/dev/rom", "/");
    serial0_init();

    /* launched all hooked user tasks */
    extern char _tasks_start;
    extern char _tasks_end;

    int func_list_size = ((uint8_t *)&_tasks_end - (uint8_t *)&_tasks_start);
    int task_cnt = func_list_size / sizeof(task_func_t);

    /* point to the first task function in the list */
    task_func_t *task_func = (task_func_t *)&_tasks_start;

    int i = 0;
    for(i = 0; i < task_cnt; i++) {
        if(!fork()) {
            (*(task_func + i))();
        } else {
            /* yield the cpu so the child task can be created */
            sched_yield(); //FIXME
        }
    }

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
