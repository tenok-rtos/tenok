#include <errno.h>
#include <fcntl.h>
#include <ioctl.h>

#include <fs/fs.h>
#include <printk.h>

#include <drivers/fb.h>
#include "bsp_drv.h"
#include "stm32f429i_discovery_ioe.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_ltdc.h"
#include "uart.h"

/* Example of using the SDRAM:
 * uint8_t sdram[10000] __attribute__((section(".sdram")));
 */

struct lcd_layer {
    LTDC_Layer_TypeDef *ltdc_layer;
    int lcd_layer;
    void *buf;
};

/* Declare two layers for double buffering */
struct lcd_layer lcd_layers[] = {
    {.ltdc_layer = LTDC_Layer1,
     .lcd_layer = LCD_BACKGROUND_LAYER,
     .buf = (void *) LCD_FRAME_BUFFER},
    {.ltdc_layer = LTDC_Layer2,
     .lcd_layer = LCD_FOREGROUND_LAYER,
     .buf = (void *) LCD_FRAME_BUFFER + BUFFER_OFFSET},
};

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
        gpio_group = GPIOG;
        gpio_pin = GPIO_Pin_13;
        break;
    case LED1:
        gpio_group = GPIOG;
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

static struct file_operations led_file_ops = {
    .ioctl = led_ioctl,
    .open = led_open,
};

static void led_init(void)
{
    /* Register LED to the file system */
    register_chrdev("led", &led_file_ops);

    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType = GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN,
    };

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

    GPIO_Init(GPIOG, &GPIO_InitStruct);

    printk("led: gpio");
}

void led_write(int fd, int state)
{
    ioctl(fd, LED0, state);
    ioctl(fd, LED1, state);
}



static struct fb_info lcd_fb = {
    .name = "stm32f429",
    .base = (void *) LCD_FRAME_BUFFER,
    .size = LCD_PIXEL_WIDTH * LCD_PIXEL_HEIGHT * 2,
    .width = LCD_PIXEL_WIDTH,
    .height = LCD_PIXEL_HEIGHT,
    .bpp = 16,
    .stride = LCD_PIXEL_WIDTH * 2,
    .pages = 1,
};

void lcd_init(void)
{
    LCD_Init();
    LCD_LayerInit();
    LTDC_Cmd(ENABLE);

    /* The controller takes what it is told at the next reload, so the alpha of
     * each layer is set before the reload and not after it
     */
    LTDC_LayerAlpha(lcd_layers[0].ltdc_layer, 0xff);
    LTDC_LayerAlpha(lcd_layers[1].ltdc_layer, 0x00);
    LTDC_ReloadConfig(LTDC_IMReload);

    LCD_SetLayer(lcd_layers[0].lcd_layer);
    LCD_Clear(LCD_COLOR_BLACK);
    LCD_DisplayStringLine(LCD_LINE_1, (uint8_t *) "Tenok RTOS");

    LCD_DisplayOn();

    fb_register(&lcd_fb);
}

void early_write(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

/* The board carries SDRAM that the chip has no equivalent of, and the heap of
 * the user space is laid out in it, so the controller has to be up first
 */
void __board_memory_init(void)
{
    SDRAM_Init();
}

void __board_init(void)
{
    lcd_init();
    serial1_init(115200, "console", "shell (alias: serial0)");
    serial2_init(115200, "mavlink", "mavlink (alias: serial1)");
    serial3_init(115200, "dbglink", "debug-link (alias: serial2)");
    led_init();
}
