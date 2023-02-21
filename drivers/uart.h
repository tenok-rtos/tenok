#ifndef __UART_H__
#define __UART_H__

#include "stm32f4xx.h"

void serial0_init(void);

void uart3_init(uint32_t baudrate);

char uart3_getc(void);
void uart3_puts(char *str);

void uart_putc(USART_TypeDef *uart, char c);
char uart_getc(USART_TypeDef *uart);
void uart_puts(USART_TypeDef *uart, char *str);

#endif
