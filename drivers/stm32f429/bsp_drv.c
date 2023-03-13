#include "stm32f4xx_ltdc.h"
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
