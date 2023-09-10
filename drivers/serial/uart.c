#include <errno.h>
#include <string.h>

#include "uart.h"
#include "stm32f4xx.h"

void uart_putc(USART_TypeDef *uart, char c)
{
    while(USART_GetFlagStatus(uart, USART_FLAG_TXE) == RESET);
    USART_SendData(uart, c);
    while(USART_GetFlagStatus(uart, USART_FLAG_TC) == RESET);
}

char uart_getc(USART_TypeDef *uart)
{
    while(USART_GetFlagStatus(uart, USART_FLAG_RXNE) == RESET);
    return USART_ReceiveData(uart);
}

int uart_puts(USART_TypeDef *uart, const char *data, size_t size)
{
    int i;
    for(i = 0; i < size; i++)
        uart_putc(uart, data[i]);

    return size;
}
