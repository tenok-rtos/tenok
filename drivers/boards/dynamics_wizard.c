#include "mpu6500.h"
#include "stm32f4xx_gpio.h"
#include "uart.h"

void led_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_2 | GPIO_Pin_3,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN};

    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void led_write(int state)
{
    GPIO_WriteBit(GPIOA, GPIO_Pin_0, state);
    GPIO_WriteBit(GPIOA, GPIO_Pin_2, state);
    GPIO_WriteBit(GPIOA, GPIO_Pin_3, state);
}

void early_write(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

void __board_init(void)
{
    led_init();
    uart1_init("serial0", "console");
    uart2_init("serial1", "mavlink");
    uart3_init("serial2", "debug-link");
    mpu6500_init();
}
