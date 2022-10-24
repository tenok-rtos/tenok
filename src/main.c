#include <stddef.h>
#include "stm32f4xx_gpio.h"
#include "gpio.h"
#include "uart.h"
#include "kernel.h"
#include "syscall.h"
#include "shell.h"

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);
void shell_cmd_clear(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);

struct cmd_list_entry shell_cmd_list[] = {
	DEF_SHELL_CMD(help)
	DEF_SHELL_CMD(clear)
};

void led_task1(void)
{
	int state = 1;
	while(1) {
		GPIO_WriteBit(GPIOD, GPIO_Pin_12, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_13, state);

		volatile int pid = getpid();
		volatile int retval = setpriority(0, 2, 3);

		sleep(1000);

		state = (state + 1) % 2;
	}
}

void led_task2(void)
{
	int state = 1;
	while(1) {
		GPIO_WriteBit(GPIOD, GPIO_Pin_14, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_15, state);

		volatile int pid = getpid();

		sleep(1000);

		state = (state + 1) % 2;
	}
}

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char *s = "supported commands:\n\r"
	          "help\n\r"
	          "clear\n\r";
	shell_puts(s);
}

void shell_cmd_clear(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	shell_cls();
}

void shell_task(void)
{
	/* shell initialization */
	char ret_shell_cmd[CMD_LEN_MAX];
	struct shell_struct shell;
	shell_init_struct(&shell, __USER_NAME__ "@stm32f407 > ", ret_shell_cmd);
	int shell_cmd_cnt = SIZE_OF_SHELL_CMD_LIST(shell_cmd_list);

	shell_cls();

	while(1) {
		shell_cli(&shell);
		shell_cmd_exec(&shell, shell_cmd_list, shell_cmd_cnt);
	}
}

void first(void *param)
{
	if(!fork()) led_task1();
	if(!fork()) led_task2();
	if(!fork()) shell_task();

	//idle loop if no work to do
	while(1) {
		volatile int pid = getpid();
	}
}

int main(void)
{
	led_init();
	uart3_init();

	task_create(first, 1);

	os_start();

	return 0;
}
