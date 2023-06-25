#ifndef __UART_H__
#define __UART_H__

#include "stm32f4xx.h"

void uart1_init(uint32_t baudrate);
void uart2_init(uint32_t baudrate);
void uart3_init(uint32_t baudrate);

void uart_putc(USART_TypeDef *uart, char c);
char uart_getc(USART_TypeDef *uart);
int uart_puts(USART_TypeDef *uart, const char *data, size_t size);

#endif
