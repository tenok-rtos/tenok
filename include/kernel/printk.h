/**
 * @file
 */
#ifndef __KERNEL_PRINTK_H__
#define __KERNEL_PRINTK_H__

/**
 * @brief  Print kernel message. The function should only be
           called inside the kernel space
 * @param  format: The formatting string.
 * @param  variable arguments: The variables used by the
 *         formatting specifiers.
 * @retval None
 */
void printk(char *format, ...);

/**
 * @brief  Display a message, then halt the system
 * @param  format: The formatting string.
 * @param  variable arguments: The variables used by the
 *         formatting specifiers.
 * @retval None
 */
void panic(char *format, ...);

void printkd_init(void);
void printkd_start(void);
void printkd(void);

#endif
