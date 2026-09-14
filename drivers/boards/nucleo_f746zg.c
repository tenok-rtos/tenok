/**
 * @file
 * The NUCLEO-F746ZG.
 *
 * Three LEDs and one serial port is the whole of what this board offers, and
 * the serial port does not leave it: the debugger on the far end of the board
 * carries it to the host as a virtual COM port, so the terminal is reached
 * over the same cable that flashes the part.
 */

#include <errno.h>
#include <fcntl.h>
#include <ioctl.h>

#include <fs/fs.h>
#include <printk.h>

#include "kconfig.h"

#include "bsp_drv.h"
#include "eth.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_gpio.h"
#include "uart_ll.h"

/* The three LEDs, which are green, blue and red in that order. There is no
 * fourth, so the fourth the file system offers is refused
 */
#define LED0_PORT GPIOB
#define LED0_PIN LL_GPIO_PIN_0
#define LED1_PORT GPIOB
#define LED1_PIN LL_GPIO_PIN_7
#define LED2_PORT GPIOB
#define LED2_PIN LL_GPIO_PIN_14

/* Which port carries the terminal, and on which pins.
 *
 * The debugger's own port is the least to wire and the most to go wrong: it is
 * a USB device in its own right and falls off the bus under sustained output
 * on some hosts. The other two leave the board on a pin header instead, for a
 * converter of the reader's own, and neither shares a pin with the Ethernet
 * interface.
 */
#if defined(CONFIG_CONSOLE_USART6)
#define CONSOLE_UART USART6
#define CONSOLE_PORT GPIOG
#define CONSOLE_TX_PIN LL_GPIO_PIN_14
#define CONSOLE_RX_PIN LL_GPIO_PIN_9
#define CONSOLE_AF LL_GPIO_AF_8
#define CONSOLE_GPIO_CLK LL_AHB1_GRP1_PERIPH_GPIOG
#define CONSOLE_UART_CLK_APB2 1
#elif defined(CONFIG_CONSOLE_USART2)
#define CONSOLE_UART USART2
#define CONSOLE_PORT GPIOD
#define CONSOLE_TX_PIN LL_GPIO_PIN_5
#define CONSOLE_RX_PIN LL_GPIO_PIN_6
#define CONSOLE_AF LL_GPIO_AF_7
#define CONSOLE_GPIO_CLK LL_AHB1_GRP1_PERIPH_GPIOD
#define CONSOLE_UART_CLK_APB2 0
#else /* CONFIG_CONSOLE_VCP */
#define CONSOLE_UART USART3
#define CONSOLE_PORT GPIOD
#define CONSOLE_TX_PIN LL_GPIO_PIN_8
#define CONSOLE_RX_PIN LL_GPIO_PIN_9
#define CONSOLE_AF LL_GPIO_AF_7
#define CONSOLE_GPIO_CLK LL_AHB1_GRP1_PERIPH_GPIOD
#define CONSOLE_UART_CLK_APB2 0
#endif

int led_open(struct inode *inode, struct file *file)
{
    return 0;
}

int led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (arg != LED_ENABLE && arg != LED_DISABLE && arg != LED_TOGGLE)
        return -EINVAL;

    GPIO_TypeDef *gpio_group;
    uint32_t gpio_pin;

    /* Pin selection */
    switch (cmd) {
    case LED0:
        gpio_group = LED0_PORT;
        gpio_pin = LED0_PIN;
        break;
    case LED1:
        gpio_group = LED1_PORT;
        gpio_pin = LED1_PIN;
        break;
    case LED2:
        gpio_group = LED2_PORT;
        gpio_pin = LED2_PIN;
        break;
    default:
        return -EINVAL;
    }

    /* Write new pin state */
    switch (arg) {
    case LED_ENABLE:
        LL_GPIO_SetOutputPin(gpio_group, gpio_pin);
        break;
    case LED_DISABLE:
        LL_GPIO_ResetOutputPin(gpio_group, gpio_pin);
        break;
    case LED_TOGGLE:
        LL_GPIO_TogglePin(gpio_group, gpio_pin);
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

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

    LL_GPIO_InitTypeDef gpio = {
        .Pin = LED0_PIN | LED1_PIN | LED2_PIN,
        .Mode = LL_GPIO_MODE_OUTPUT,
        .Speed = LL_GPIO_SPEED_FREQ_LOW,
        .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
        .Pull = LL_GPIO_PULL_NO,
    };
    LL_GPIO_Init(LED0_PORT, &gpio);

    printk("led: gpio");
}

static void console_pins_init(void)
{
    LL_AHB1_GRP1_EnableClock(CONSOLE_GPIO_CLK);

    /* The sixth port hangs off the faster of the two buses; the rest off the
     * slower one
     */
#if CONSOLE_UART_CLK_APB2
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART6);
#elif defined(CONFIG_CONSOLE_USART2)
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
#else
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);
#endif

    LL_GPIO_InitTypeDef gpio = {
        .Pin = CONSOLE_TX_PIN | CONSOLE_RX_PIN,
        .Mode = LL_GPIO_MODE_ALTERNATE,
        .Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH,
        .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
        .Pull = LL_GPIO_PULL_UP,
        .Alternate = CONSOLE_AF,
    };
    LL_GPIO_Init(CONSOLE_PORT, &gpio);
}

/* The three the board has, with the fourth the caller asks for left out */
void led_write(int fd, int state)
{
    ioctl(fd, LED0, state);
    ioctl(fd, LED1, state);
    ioctl(fd, LED2, state);
}

void early_write(char *buf, size_t size)
{
    uart_puts(CONSOLE_UART, buf, size);
}

/* This board adds no memory of its own, so there is nothing to bring up */
void __board_memory_init(void)
{
}

/* An address of this interface's own choosing, which the second bit says is
 * locally administered and the first says is not a group. Nothing hands these
 * out for a board, so one is picked rather than bought
 */
static const uint8_t board_mac[ETH_MAC_ADDR_LEN] = {0x02, 0x00, 0x00,
                                                    0xf7, 0x46, 0x01};

void __board_init(void)
{
    console_pins_init();
    serial_console_init(CONSOLE_UART, 115200, "console",
                        "shell (alias: serial0)");
    led_init();
    eth_init(board_mac);
}
