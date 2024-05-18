#ifndef __BSP_DRV_H__
#define __BSP_DRV_H__

#define LED0 0
#define LED1 1
#define LED2 2
#define LED3 3

#define GPIO_PIN0 0

#define GPIO_RESET 0
#define GPIO_SET 1
#define GPIO_TOGGLE 2

#define LED_DISABLE GPIO_RESET
#define LED_ENABLE GPIO_SET
#define LED_TOGGLE GPIO_TOGGLE

void led_write(int fd, int state);

#endif
