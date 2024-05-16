#include <errno.h>
#include <fcntl.h>
#include <ioctl.h>

#include <fs/fs.h>
#include <printk.h>

#include "bsp_drv.h"
#include "mpu6500.h"
#include "pwm.h"
#include "sbus.h"
#include "stm32f4xx_gpio.h"
#include "uart.h"

int led_open(struct inode *inode, struct file *file)
{
    return 0;
}

int led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (arg != LED_ENABLE && arg != LED_DISABLE)
        return -EINVAL;

    switch (cmd) {
    case LED0:
        GPIO_WriteBit(GPIOA, GPIO_Pin_2, arg);
        break;
    case LED1:
        GPIO_WriteBit(GPIOA, GPIO_Pin_0, arg);
        break;
    case LED2:
        GPIO_WriteBit(GPIOA, GPIO_Pin_3, arg);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static struct file_operations led_file_ops = {
    .ioctl = led_ioctl,
    .open = led_open,
};

static void led_init(void)
{
    /* Register LED to the file system */
    register_chrdev("led", &led_file_ops);

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_2 | GPIO_Pin_3,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN};

    GPIO_Init(GPIOA, &GPIO_InitStruct);

    printk("led: gpio");
}

void led_write(int fd, int state)
{
    ioctl(fd, LED0, state);
    ioctl(fd, LED1, state);
    ioctl(fd, LED2, state);
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
