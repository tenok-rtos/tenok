#ifndef __UART_H__
#define __UART_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/mutex.h>
#include <kernel/wait.h>

#include "stm32f4xx.h"

typedef struct {
    /* Tx */
    struct list_head tx_wait_list;
    struct mutex tx_mtx;

    /* Rx */
    struct list_head rx_wait_list;
    struct kfifo *rx_fifo;
    size_t rx_wait_size;
} uart_dev_t;

void uart_putc(USART_TypeDef *uart, char c);
char uart_getc(USART_TypeDef *uart);
int uart_puts(USART_TypeDef *uart, const char *data, size_t size);

#endif
