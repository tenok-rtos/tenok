#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/kfifo.h>
#include <kernel/printk.h>
#include <kernel/time.h>
#include <kernel/tty.h>

#include "kconfig.h"

void printk(char *format, ...)
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
    for (int i = 0; i < (9 - strlen(rem)); i++) {
        zeros[i] = '0';
    }

    char buf[PRINT_SIZE_MAX] = {'\r', 0};
    int pos = snprintf(&buf[1], PRINT_SIZE_MAX, "[%5s.%s%s] ", sec, zeros, rem);
    vsnprintf(&buf[pos + 1], PRINT_SIZE_MAX, format, args);
    strcat(buf, "\n\r");

    console_write(buf, strlen(buf));

    va_end(args);
}

void early_printf(char *format, ...)
{
    va_list args;
    va_start(args, format);

    char buf[300] = {0};
    vsnprintf(buf, 300, format, args);
    early_tty_print(buf, strlen(buf));

    va_end(args);
}
