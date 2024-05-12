#include "stm32f4xx_conf.h"
#include "uart.h"

void led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_Init(GPIOD, &GPIO_InitStruct);
}

void led_write(int state)
{
    GPIO_WriteBit(GPIOD, GPIO_Pin_12, state);
    GPIO_WriteBit(GPIOD, GPIO_Pin_13, state);
    GPIO_WriteBit(GPIOD, GPIO_Pin_14, state);
    GPIO_WriteBit(GPIOD, GPIO_Pin_15, state);
}

void early_write(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

void __board_init(void)
{
    led_init();
    serial1_init(115200, "console", "shell (alias: serial0)");
    serial2_init(115200, "mavlink", "mavlink (alias: serial1)");
    serial3_init(115200, "dbglink", "debug-link (alias: serial2)");
}
