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

    /*================================*
     * initialized all hooked drivers *
     *================================*/
    extern char _drvs_start;
    extern char _drvs_end;

    int func_list_size = ((uint8_t *)&_drvs_end - (uint8_t *)&_drvs_start);
    int drv_cnt = func_list_size / sizeof(drv_init_func_t);

    /* point to the first driver initialization function of the list */
    drv_init_func_t *drv_init_func = (drv_init_func_t *)&_drvs_start;

    int i;
    for(i = 0; i < drv_cnt; i++) {
        (*(drv_init_func + i))();
    }

    /*=======================*
     * mount the file system *
     *=======================*/
    mount("/dev/rom", "/");

    /*================================*
     * launched all hooked user tasks *
     *================================*/
    extern char _tasks_start;
    extern char _tasks_end;

    func_list_size = ((uint8_t *)&_tasks_end - (uint8_t *)&_tasks_start);
    int task_cnt = func_list_size / sizeof(task_func_t);

    /* point to the first task function of the list */
    task_func_t *task_func = (task_func_t *)&_tasks_start;

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

    sem_init(&sem_led, 0, 0);

    sched_start(first);

    return 0;
}
