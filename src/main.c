#include <stddef.h>
#include "stm32f4xx_gpio.h"
#include "gpio.h"
#include "uart.h"
#include "kernel.h"

tcb_t tcb1;
tcb_t tcb2;

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

		task_yield();
		//task_delay(1000);

		state = (state + 1) % 2;
	}
}

void task2(void *param)
{
	int state = 1;
	while(1) {
		GPIO_WriteBit(GPIOD, GPIO_Pin_14, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_15, state);

		task_yield();
		//task_delay(1000);

		state = (state + 1) % 2;
	}
}

int main(void)
{
	led_init();
	uart3_init();

	task_register(task1, "task1", 0, NULL, 1, &tcb1);
	task_register(task2, "task2", 0, NULL, 2, &tcb2);

	os_start();

	return 0;
}
