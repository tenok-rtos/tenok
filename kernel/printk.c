#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "shell.h"

void ltoa(char *buf, unsigned long i, int base)
{
#define LEN 25

    char *s;
    int rem;
    char rev[LEN + 1];

    if (i == 0)
        s = "0";
    else {
        rev[LEN] = 0;
        s = &rev[LEN];

        while (i) {
            rem = i % base;

            if (rem < 10) {
                *--s = rem + '0';
            } else if (base == 16) {
                *--s = "abcdef"[rem - 10];
            }

            i /= base;
        }
    }

    strcpy(buf, s);
}

void printk(char *format,  ...)
{
    va_list args;
    va_start(args, format);

    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);

    char sec[15] = {0};
    ltoa(sec, tp.tv_sec, 10);

    char rem[15] = {0};
    ltoa(rem, tp.tv_nsec, 10);

    char zeros[15] = {0};
    for(int i = 0; i < (9 - strlen(rem)); i++) {
        zeros[i] = '0';
    }

    char buf[200] = {0};
    int pos = sprintf(buf, "[%5s.%s%s] ", sec, zeros, rem);
    pos += vsprintf(&buf[pos], format, args);
    pos += sprintf(&buf[pos], "\n\r");
    va_end(args);

    shell_puts(buf); //FIXME: should not rely on shell function
}
