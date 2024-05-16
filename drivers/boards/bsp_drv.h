#ifndef __BSP_DRV_H__
#define __BSP_DRV_H__

#define LED0 0
#define LED1 1
#define LED2 2
#define LED3 3

#define LED_DISABLE 0
#define LED_ENABLE 1

void led_init(void);
void led_write(int state);

#endif
