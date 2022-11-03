#ifndef __UART_H__
#define __UART_H__

void uart3_init(void);

char uart3_getc(void);
void uart3_putc(char c);
void uart3_puts(char *string);

#endif
