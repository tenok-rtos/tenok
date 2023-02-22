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

extern struct inode *shell_dir_curr;

sem_t sem_led;

/* shell */
struct cmd_list_entry shell_cmd_list[] = {
	DEF_SHELL_CMD(help),
	DEF_SHELL_CMD(clear),
	DEF_SHELL_CMD(history),
	DEF_SHELL_CMD(ps),
	DEF_SHELL_CMD(echo),
	DEF_SHELL_CMD(ls),
	DEF_SHELL_CMD(cd),
	DEF_SHELL_CMD(pwd),
	DEF_SHELL_CMD(cat),
	DEF_SHELL_CMD(file)
};

struct shell_struct shell;

void led_task1(void)
{
	set_program_name("led1");
	setpriority(0, getpid(), 3);

	int state = 1;
	while(1) {
		sem_wait(&sem_led);

		GPIO_WriteBit(GPIOD, GPIO_Pin_12, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_13, state);

		state = (state + 1) % 2;
	}
}

void led_task2(void)
{
	set_program_name("led2");
	setpriority(0, getpid(), 3);

	int state = 1;
	while(1) {
		sem_post(&sem_led);

		volatile int pid = getpid();

		GPIO_WriteBit(GPIOD, GPIO_Pin_14, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_15, state);

		sleep(1000);

		state = (state + 1) % 2;
	}
}

void shell_task(void)
{
	set_program_name("shell");
	setpriority(0, getpid(), 2);

	/* shell initialization */
	char ret_shell_cmd[CMD_LEN_MAX];
	char path_curr[200] = {0};
	char prompt_msg[PROMPT_LEN_MAX] = {0};
	int  shell_cmd_cnt = SIZE_OF_SHELL_CMD_LIST(shell_cmd_list);

	shell_init(&shell, prompt_msg, ret_shell_cmd);
	shell_path_init();
	shell_serial_init();

	shell.shell_cmds = shell_cmd_list;
	shell.cmd_cnt = shell_cmd_cnt;

	/* clean screen */
	shell_cls();

	/* greeting */
	char s[100] = {0};
	snprintf(s, 100, "firmware build time: %s %s\n\rtype `help' for help\n\r\n\r", __TIME__, __DATE__);
	shell_puts(s);

	while(1) {
		fs_get_pwd(path_curr, shell_dir_curr);
		snprintf(prompt_msg, PROMPT_LEN_MAX, __USER_NAME__ "@stm32f407:%s$ ",  path_curr);
		shell.prompt_len = strlen(shell.prompt_msg);

		shell_cli(&shell);
		shell_cmd_exec(&shell, shell_cmd_list, shell_cmd_cnt);
	}
}

void mk_cpuinfo(void)
{
	/* create a new regular file */
	mknod("/proc/cpuinfo", 0, S_IFREG);
	int fd = open("/proc/cpuinfo", 0, 0);

	char *str = "processor  : 0\n\r"
	            "model name : STM32F407 Microcontroller\n\r"
	            "cpu MHz    : 168\n\r"
	            "flash size : 1M\n\r"
	            "ram size   : 192K\n\r";

	/* write file content */
	int len = strlen(str);
	write(fd, str, len);

	/* write end-of-file */
	signed char c = EOF;
	write(fd, &c, 1);
}

void first(void *param)
{
	set_program_name("first");

	/* the service should be initialized before
	 * forking any new tasks */
	os_service_init();

	sem_init(&sem_led, 0, 0);
	mk_cpuinfo();
	rom_dev_init();
	mount("/dev/rom", "/");
	serial0_init();

	if(!fork()) led_task1();
	if(!fork()) led_task2();
	if(!fork()) shell_task();

	//run_fifo_example();
	//run_mutex_example();
	//run_mqueue_example();

	while(1); //idle loop when nothing to do
}

int main(void)
{
	led_init();

	/* uart3 should be initialized before starting the os
	 * since the configuration of the nvic requires using
	 * privilege mode */
	uart3_init(115200);

	sched_start(first);

	return 0;
}
