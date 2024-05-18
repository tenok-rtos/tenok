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
    if (arg != LED_ENABLE && arg != LED_DISABLE && arg != LED_TOGGLE)
        return -EINVAL;

    GPIO_TypeDef *gpio_group;
    uint16_t gpio_pin;

    /* Pin selection */
    switch (cmd) {
    case LED0:
        gpio_group = GPIOA;
        gpio_pin = GPIO_Pin_2;
        break;
    case LED1:
        gpio_group = GPIOA;
        gpio_pin = GPIO_Pin_0;
        break;
    case LED2:
        gpio_group = GPIOA;
        gpio_pin = GPIO_Pin_3;
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

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_2 | GPIO_Pin_3,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };

    GPIO_Init(GPIOA, &GPIO_InitStruct);

    printk("led: gpio");
}

void led_write(int fd, int state)
{
    ioctl(fd, LED0, state);
    ioctl(fd, LED1, state);
    ioctl(fd, LED2, state);
}

int gpio_open(struct inode *inode, struct file *file)
{
    return 0;
}

int gpio_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (arg != GPIO_SET && arg != GPIO_RESET && arg != GPIO_TOGGLE)
        return -EINVAL;

    GPIO_TypeDef *gpio_group;
    uint16_t gpio_pin;

    /* Pin selection */
    switch (cmd) {
    case GPIO_PIN0:
        gpio_group = GPIOB;
        gpio_pin = GPIO_Pin_14;
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

static struct file_operations gpio_file_ops = {
    .ioctl = gpio_ioctl,
    .open = gpio_open,
};

static void gpio_init(void)
{
    /* Register GPIO to the file system */
    register_chrdev("freq_tester", &gpio_file_ops);

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_14,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };

    GPIO_Init(GPIOB, &GPIO_InitStruct);

    printk("freq_tester: gpio");
}

void early_write(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

void __board_init(void)
{
    serial1_init(115200, "console", "shell (alias serial0)");
    serial3_init(115200, "dbglink", "debug-link (alias serial1)");
    // uartX_init(115200, "mavlink", "mavlink (alias serialX)"); //TODO
    led_init();
    gpio_init();
    pwm_init();
    sbus_init();
    mpu6500_init();
}
