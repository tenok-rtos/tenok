#ifndef __UART_H__
#define __UART_H__

#include <stdbool.h>

#include "stm32f4xx.h"

enum {
    UART_TX_IDLE,
    UART_TX_DMA_BUSY
} UART_TX_STATE;

typedef struct {
    struct kfifo *rx_fifo;
    int rx_wait_size;
    bool tx_dma_ready;
    int state;
} uart_dev_t;

void uart_putc(USART_TypeDef *uart, char c);
char uart_getc(USART_TypeDef *uart);
int uart_puts(USART_TypeDef *uart, const char *data, size_t size);

#endif
