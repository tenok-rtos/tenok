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

void task1(void)
{
	int state = 1;
	while(1) {
		GPIO_WriteBit(GPIOD, GPIO_Pin_12, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_13, state);

		volatile int pid = getpid();

		sleep(1000);

		state = (state + 1) % 2;
	}
}

void task2(void)
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

void first(void *param)
{
	if(!fork()) task1();
	if(!fork()) task2();

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
