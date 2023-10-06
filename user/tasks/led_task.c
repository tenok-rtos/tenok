#include <tenok.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/resource.h>

#include <kernel/task.h>

#include "bsp_drv.h"

sem_t sem_led;

void led_task1(void)
{
    setprogname("led1");

    sem_init(&sem_led, 0, 0);

    int state = 1;
    while(1) {
        sem_wait(&sem_led);

        led_write(state);

        state = (state + 1) % 2;
    }
}

void led_task2(void)
{
    setprogname("led2");

    while(1) {
        sem_post(&sem_led);
        sleep(1);
    }
}

HOOK_USER_TASK(led_task1, 3, 256);
HOOK_USER_TASK(led_task2, 3, 256);
