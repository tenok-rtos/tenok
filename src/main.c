#include <stddef.h>
#include "stm32f4xx_gpio.h"
#include "uart.h"
#include "task.h"

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
		delay(1000000L);
		state = (state + 1) % 2;

		//task_yield();
	}
}

void task2(void *param)
{
	int state = 1;
	while(1) {
		GPIO_WriteBit(GPIOD, GPIO_Pin_14, state);
		GPIO_WriteBit(GPIOD, GPIO_Pin_15, state);
		delay(1000000L);
		state = (state + 1) % 2;

		//task_yield();
	}
}

void led_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {
		.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15,
		.GPIO_Mode = GPIO_Mode_OUT,
		.GPIO_Speed = GPIO_Speed_50MHz,
		.GPIO_OType =GPIO_OType_PP,
		.GPIO_PuPd = GPIO_PuPd_DOWN
	};

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	GPIO_Init(GPIOD, &GPIO_InitStruct);
}

int main(void)
{
	led_init();
	uart3_init();

	task_register(task1, "task1", 0, NULL, &tcb1);
	task_register(task2, "task2", 0, NULL, &tcb2);

	os_start();

	return 0;
}
