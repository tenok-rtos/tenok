#include "stm32f4xx_ltdc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ioe.h"

//example of using the sdram:
//uint8_t sdram[10000] __attribute__((section(".sdram")));

struct lcd_layer {
    LTDC_Layer_TypeDef *ltdc_layer;
    int  lcd_layer;
    void *buf;
};

/* declare two layers for double buffering */
struct lcd_layer lcd_layers[] = {
    {.ltdc_layer = LTDC_Layer1, .lcd_layer = LCD_BACKGROUND_LAYER, .buf = (void*)LCD_FRAME_BUFFER},
    {.ltdc_layer = LTDC_Layer2, .lcd_layer = LCD_FOREGROUND_LAYER, .buf = (void*)LCD_FRAME_BUFFER + BUFFER_OFFSET}
};

void led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {
        .GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14,
        .GPIO_Mode = GPIO_Mode_OUT,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_OType =GPIO_OType_PP,
        .GPIO_PuPd = GPIO_PuPd_DOWN
    };

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

    GPIO_Init(GPIOG, &GPIO_InitStruct);
}

void led_write(int state)
{
    GPIO_WriteBit(GPIOG, GPIO_Pin_13, state);
    GPIO_WriteBit(GPIOG, GPIO_Pin_14, state);
}

void bsp_driver_init(void)
{
    /* sdram initialization */
    SDRAM_Init();

    /* lcd initialization */
    LCD_Init();
    LCD_LayerInit();
    LTDC_Cmd(ENABLE);

    LTDC_ReloadConfig(LTDC_IMReload);
    LTDC_LayerAlpha(lcd_layers[0].ltdc_layer, 0xff);
    LTDC_LayerAlpha(lcd_layers[1].ltdc_layer, 0x00);

    LCD_SetLayer(lcd_layers[1].lcd_layer);
    LCD_Clear(LCD_COLOR_WHITE);
    LCD_DisplayStringLine(LCD_LINE_1, "Tenok RTOS");

    LCD_DisplayOn();
}
