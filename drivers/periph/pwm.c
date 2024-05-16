#include <stdint.h>

#include <fs/fs.h>
#include <printk.h>

#include "pwm.h"
#include "stm32f4xx.h"

#define PWM_FREQ 400  // Hz
#define PWM_RELOAD 25000
#define MICROSECOND 1000000

/* Map from +width time to timer reload value */
#define MICROSEC_TO_RELOAD(us) (us * (PWM_RELOAD * PWM_FREQ / MICROSECOND))

int pwm_open(struct inode *inode, struct file *file)
{
    return 0;
}

int pwm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned long reload = MICROSEC_TO_RELOAD(arg);

    switch (cmd) {
    case SET_PWM_CHANNEL1:
        TIM4->CCR1 = reload;
        break;
    case SET_PWM_CHANNEL2:
        TIM4->CCR2 = reload;
        break;
    case SET_PWM_CHANNEL3:
        TIM1->CCR4 = reload;
        break;
    case SET_PWM_CHANNEL4:
        TIM1->CCR3 = reload;
        break;
    case SET_PWM_CHANNEL5:
        TIM4->CCR3 = reload;
        break;
    case SET_PWM_CHANNEL6:
        TIM4->CCR4 = reload;
        break;
    case SET_PWM_CHANNEL7:
        TIM1->CCR2 = reload;
        break;
    case SET_PWM_CHANNEL8:
        TIM1->CCR1 = reload;
        break;
    }

    return 0;
}

static struct file_operations pwm_file_ops = {
    .ioctl = pwm_ioctl,
    .open = pwm_open,
};

static void pwm_timer1_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_TIM1);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_100MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };

    GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* 180MHz / (25000 * 18) = 400Hz = 0.0025s */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct = {
        .TIM_Period = PWM_RELOAD - 1,
        .TIM_Prescaler = 18 - 1,
        .TIM_ClockDivision = TIM_CKD_DIV1,
        .TIM_CounterMode = TIM_CounterMode_Up,
    };

    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStruct);

    TIM_OCInitTypeDef TIM_OCInitStruct = {
        .TIM_OCMode = TIM_OCMode_PWM1,
        .TIM_OutputState = TIM_OutputState_Enable,
        .TIM_Pulse = 0,
    };

    TIM_OC1Init(TIM1, &TIM_OCInitStruct);
    TIM_OC2Init(TIM1, &TIM_OCInitStruct);
    TIM_OC3Init(TIM1, &TIM_OCInitStruct);
    TIM_OC4Init(TIM1, &TIM_OCInitStruct);

    TIM_Cmd(TIM1, ENABLE);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

static void pwm_timer4_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource12, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource13, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_TIM4);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15,
        .GPIO_Mode = GPIO_Mode_AF,
        .GPIO_Speed = GPIO_Speed_100MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };

    GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* 90MHz / (25000 * 9) = 400Hz = 0.0025s */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct = {
        .TIM_Period = PWM_RELOAD - 1,
        .TIM_Prescaler = 9 - 1,
        .TIM_ClockDivision = TIM_CKD_DIV1,
        .TIM_CounterMode = TIM_CounterMode_Up,
    };

    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStruct);

    TIM_OCInitTypeDef TIM_OCInitStruct = {
        .TIM_OCMode = TIM_OCMode_PWM1,
        .TIM_OutputState = TIM_OutputState_Enable,
        .TIM_Pulse = 0,
    };

    TIM_OC1Init(TIM4, &TIM_OCInitStruct);
    TIM_OC2Init(TIM4, &TIM_OCInitStruct);
    TIM_OC3Init(TIM4, &TIM_OCInitStruct);
    TIM_OC4Init(TIM4, &TIM_OCInitStruct);

    TIM_Cmd(TIM4, ENABLE);
}

void pwm_init(void)
{
    /* Register PWM to the file system */
    register_chrdev("pwm", &pwm_file_ops);

    pwm_timer1_init();
    pwm_timer4_init();

    printk("pwm: pulse-width modulation");
}
