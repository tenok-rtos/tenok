#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/time.h>
#include <kernel/kfifo.h>
#include <kernel/printk.h>

#define PRINTK_BUF_LEN   100
#define PRINTK_FIFO_SIZE 10

void printk(char *format,  ...)
{
    va_list args;
    va_start(args, format);

    struct timespec tp;
    get_sys_time(&tp);

    char sec[sizeof(long) * 8 + 1] = {0};
    ltoa(tp.tv_sec, sec, 10);

    char rem[sizeof(long) * 8 + 1] = {0};
    ltoa(tp.tv_nsec, rem, 10);

    char zeros[15] = {0};
    for(int i = 0; i < (9 - strlen(rem)); i++) {
        zeros[i] = '0';
    }

    char buf[100] = {0};
    int pos = sprintf(buf, "[%5s.%s%s] ", sec, zeros, rem);
    vsprintf(&buf[pos], format, args);
    strcat(buf, "\n\r");
    va_end(args);

    console_write(buf, strlen(buf));
}
