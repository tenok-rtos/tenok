#include <errno.h>
#include <fcntl.h>
#include <ioctl.h>

#include <fs/fs.h>
#include <printk.h>

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
    if (arg != LED_ENABLE && arg != LED_DISABLE)
        return -EINVAL;

    switch (cmd) {
    case LED0:
        GPIO_WriteBit(GPIOG, GPIO_Pin_13, arg);
        break;
    case LED1:
        GPIO_WriteBit(GPIOG, GPIO_Pin_14, arg);
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

void lcd_init(void)
{
    LCD_Init();
    LCD_LayerInit();
    LTDC_Cmd(ENABLE);

    LTDC_ReloadConfig(LTDC_IMReload);
    LTDC_LayerAlpha(lcd_layers[0].ltdc_layer, 0xff);
    LTDC_LayerAlpha(lcd_layers[1].ltdc_layer, 0x00);

    LCD_SetLayer(lcd_layers[1].lcd_layer);
    LCD_Clear(LCD_COLOR_WHITE);
    LCD_DisplayStringLine(LCD_LINE_1, (uint8_t *) "Tenok RTOS");

    LCD_DisplayOn();
}

void early_write(char *buf, size_t size)
{
    uart_puts(USART1, buf, size);
}

void __board_init(void)
{
    SDRAM_Init();
    lcd_init();
    led_init();
    serial1_init(115200, "console", "shell (alias: serial0)");
    serial2_init(115200, "mavlink", "mavlink (alias: serial1)");
    serial3_init(115200, "dbglink", "debug-link (alias: serial2)");
}
