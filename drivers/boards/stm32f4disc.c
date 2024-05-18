#include <errno.h>
#include <fcntl.h>
#include <ioctl.h>

#include <fs/fs.h>
#include <printk.h>

#include "bsp_drv.h"
#include "stm32f4xx_conf.h"
#include "uart.h"

int led_open(struct inode *inode, struct file *file)
{
    return 0;
}

int led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (arg != LED_ENABLE && arg != LED_DISABLE && arg != LED_TOGGLE)
        return -EINVAL;

    GPIO_TypeDef *gpio_group;
    uint16_t gpio_pin;

    /* Pin selection */
    switch (cmd) {
    case LED0:
        gpio_group = GPIOD;
        gpio_pin = GPIO_Pin_12;
        break;
    case LED1:
        gpio_group = GPIOD;
        gpio_pin = GPIO_Pin_13;
        break;
    case LED2:
        gpio_group = GPIOD;
        gpio_pin = GPIO_Pin_14;
        break;
    case LED3:
        gpio_group = GPIOD;
        gpio_pin = GPIO_Pin_15;
        break;
    default:
        return -EINVAL;
    }

    /* Write new pin state */
    switch (arg) {
    case LED_ENABLE:
    case LED_DISABLE:
        GPIO_WriteBit(gpio_group, gpio_pin, arg);
        break;
    case LED_TOGGLE:
        GPIO_ToggleBits(gpio_group, gpio_pin);
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

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_Init(GPIOD, &GPIO_InitStruct);

    printk("led: gpio");
}

void led_write(int fd, int state)
{
    ioctl(fd, LED0, state);
    ioctl(fd, LED1, state);
    ioctl(fd, LED2, state);
    ioctl(fd, LED3, state);
}

void early_write(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

void __board_init(void)
{
    serial1_init(115200, "console", "shell (alias: serial0)");
    serial2_init(115200, "mavlink", "mavlink (alias: serial1)");
    serial3_init(115200, "dbglink", "debug-link (alias: serial2)");
    led_init();
}
