#include <stddef.h>
#include "stm32f4xx_gpio.h"
#include "gpio.h"
#include "uart.h"
#include "kernel.h"
#include "syscall.h"

void delay(uint32_t count)
{
	while(count--);
}

void task1(void *param)
{
	int state = 1;
	while(1) {
		GPIO_WriteBit(GPIOD, GPIO_Pin_12, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_13, state);

		sleep(1000);

		state = (state + 1) % 2;
	}
}

void task2(void *param)
{
	int state = 1;
	while(1) {
		GPIO_WriteBit(GPIOD, GPIO_Pin_14, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_15, state);

		volatile uint32_t pri = getpriority();
		volatile uint32_t pid = getpid();

		sleep(1000);

		state = (state + 1) % 2;
	}
}

int main(void)
{
	led_init();
	uart3_init();

	task_create(task1, 1);
	task_create(task2, 2);

	os_start();

	return 0;
}
