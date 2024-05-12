#include "mpu6500.h"
#include "pwm.h"
#include "sbus.h"
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
    pwm_init();
    serial1_init(115200, "console", "shell (alias serial0)");
    serial3_init(115200, "dbglink", "debug-link (alias serial1)");
    // uartX_init(115200, "mavlink", "mavlink (alias serialX)"); //TODO
    sbus_init();
    mpu6500_init();
}
