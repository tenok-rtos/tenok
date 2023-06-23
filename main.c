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
#include "fs.h"
#include "rom_dev.h"
#include "examples.h"
#include "bsp_drv.h"

#include "tenok_imu_ahrs_msg.h"

extern char _shell_cmds_start;
extern char _shell_cmds_end;
extern struct inode *shell_dir_curr;

sem_t sem_led;

struct shell shell;
struct shell_history history[SHELL_HISTORY_MAX];
struct shell_autocompl autocompl[100]; //XXX: handle with mpool

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

#define INC 1
#define DEC 0
void led_task2(void)
{
    set_program_name("led2");
    setpriority(0, getpid(), 3);

    int dir = INC;

    tenok_msg_imu_ahrs_t test_msg = {
        .time = 0,
        .accel = {1.0f, 2.0f, 3.0f},
        .q = {1.0f, 0.0f, 0.0f, 0.0f}
    };

    uint8_t buf[100];

    while(1) {
        sem_post(&sem_led);

        size_t size = pack_tenok_imu_ahrs_msg(&test_msg, buf);
        uart_puts(USART1, buf, size);

        if(dir == INC) {
            test_msg.time += 10;
            if(test_msg.time == 1000) {
                dir = DEC;
            }
        } else {
            test_msg.time -= 10;
            if(test_msg.time == 0) {
                dir = INC;
            }
        }

        sleep(10);
    }
}

void shell_task(void)
{
    set_program_name("shell");
    setpriority(0, getpid(), 2);

    char shell_path[PATH_LEN_MAX] = {0};
    char prompt[SHELL_PROMPT_LEN_MAX] = {0};

    struct shell_cmd *shell_cmds = (struct shell_cmd *)&_shell_cmds_start;
    int  shell_cmd_cnt = SHELL_CMDS_CNT(_shell_cmds_start, _shell_cmds_end);

    /* shell initialization */
    shell_init(&shell, shell_cmds, shell_cmd_cnt, history, SHELL_HISTORY_MAX, autocompl);
    shell_path_init();
    shell_serial_init();

    /* clean screen */
    //shell_cls();

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

    uart1_init(115200);

    /* uart3 should be initialized before starting the os
     * since the configuration of the nvic requires using
     * privilege mode */
    uart3_init(115200);

    sched_start(first);

    return 0;
}
