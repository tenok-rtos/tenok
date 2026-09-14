#ifndef __UART_LL_H__
#define __UART_LL_H__

#include <stdint.h>

#include "uart.h"

/* Bring up one serial port as a terminal and register it under `dev_name`.
 *
 * The Low Layer names its ports the way the reference manual does rather than
 * the way the older driver numbered them, so the port is named here instead of
 * being spelt into the name of the function.
 */
void serial_console_init(USART_TypeDef *uart,
                         uint32_t baudrate,
                         char *dev_name,
                         char *desc);

#endif
