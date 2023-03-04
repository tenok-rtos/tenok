#include "stm32f4xx_gpio.h"

void led_init(void)
{
#if defined STM32F40_41xxx /* STM32F4DISCOVERY board */
    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType =GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN
    };

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_Init(GPIOD, &GPIO_InitStruct);
#elif defined STM32F429_439xx /* 32F429IDISCOVERY board */
    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType =GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN
    };

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

    GPIO_Init(GPIOG, &GPIO_InitStruct);
#endif
}

void led_write(int state)
{
#if defined STM32F40_41xxx /* STM32F4DISCOVERY board */
    GPIO_WriteBit(GPIOD, GPIO_Pin_12, state);
    GPIO_WriteBit(GPIOD, GPIO_Pin_13, state);
    GPIO_WriteBit(GPIOD, GPIO_Pin_14, state);
    GPIO_WriteBit(GPIOD, GPIO_Pin_15, state);
#elif defined STM32F429_439xx /* 32F429IDISCOVERY board */
    GPIO_WriteBit(GPIOG, GPIO_Pin_13, state);
    GPIO_WriteBit(GPIOG, GPIO_Pin_14, state);
#endif
}
