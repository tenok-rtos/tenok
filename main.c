#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_gpio.h"
#include "gpio.h"
#include "uart.h"
#include "kernel.h"
#include "syscall.h"
#include "semaphore.h"
#include "shell.h"
#include "shell_cmd.h"
#include "fs.h"
#include "rom_dev.h"
#include "examples.h"
#include "bsp_drv.h"

extern struct inode *shell_dir_curr;

sem_t sem_led;

/* shell */
struct shell_cmd shell_cmds[] = {
    DEF_SHELL_CMD(help),
    DEF_SHELL_CMD(clear),
    DEF_SHELL_CMD(history),
    DEF_SHELL_CMD(ps),
    DEF_SHELL_CMD(echo),
    DEF_SHELL_CMD(ls),
    DEF_SHELL_CMD(cd),
    DEF_SHELL_CMD(pwd),
    DEF_SHELL_CMD(cat),
    DEF_SHELL_CMD(file),
    DEF_SHELL_CMD(mpool)
};

struct shell shell;
struct shell_history history[SHELL_HISTORY_MAX];
struct shell_autocompl autocompl[SHELL_CMDS_CNT(shell_cmds)];

void led_task1(void)
{
    set_program_name("led1");
    setpriority(0, getpid(), 3);

    int state = 1;
    while(1) {
        sem_wait(&sem_led);

        led_write(state);

        state = (state + 1) % 2;
    }
}

void led_task2(void)
{
    set_program_name("led2");
    setpriority(0, getpid(), 3);

    while(1) {
        sem_post(&sem_led);
        sleep(1000);
    }
}

void shell_task(void)
{
    set_program_name("shell");
    setpriority(0, getpid(), 2);

    char shell_path[PATH_LEN_MAX] = {0};
    char prompt[SHELL_PROMPT_LEN_MAX] = {0};
    int  shell_cmd_cnt = SHELL_CMDS_CNT(shell_cmds);

    /* shell initialization */
    shell_init(&shell, shell_cmds, shell_cmd_cnt, history, SHELL_HISTORY_MAX, autocompl);
    shell_path_init();
    shell_serial_init();

    /* clean screen */
    shell_cls();

    /* greeting */
    char s[100] = {0};
    snprintf(s, 100, "firmware build time: %s %s\n\rtype `help' for help\n\r\n\r", __TIME__, __DATE__);
    shell_puts(s);

    while(1) {
        fs_get_pwd(shell_path, shell_dir_curr);
        snprintf(prompt, SHELL_PROMPT_LEN_MAX, __USER_NAME__ "@%s:%s$ ",  __BOARD_NAME__, shell_path);
        shell_set_prompt(&shell, prompt);

        shell_listen(&shell);
        shell_execute(&shell);
    }
}

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

    /* uart3 should be initialized before starting the os
     * since the configuration of the nvic requires using
     * privilege mode */
    uart3_init(115200);

    sched_start(first);

    return 0;
}
