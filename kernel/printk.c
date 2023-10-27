#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/time.h>
#include <kernel/kfifo.h>
#include <kernel/printk.h>

#define PRINTK_BUF_LEN   100
#define PRINTK_FIFO_SIZE 10

#define ITOA_BUF_LEN (sizeof(int) * 8)
#define LTOA_BUF_LEN (sizeof(long) * 8)

char *itoa(int value, char *buffer, int radix)
{
    int remainder;
    char reverse_buffer[LTOA_BUF_LEN + 1] = {0};
    char *last_digit = &reverse_buffer[LTOA_BUF_LEN - 1];

    if (value < 0) {
        *last_digit = '-';
        last_digit--;
        value *= -1;
    } else if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    for(int i = 0; i < LTOA_BUF_LEN && value; i++) {
        remainder = value % radix;

        if (remainder < 10) {
            *last_digit = remainder + '0';
        } else if (radix == 16) {
            *last_digit = "abcdef"[remainder - 10];
        }

        last_digit--;
        value /= radix;
    }

    char *str = last_digit + 1;
    size_t size = &reverse_buffer[LTOA_BUF_LEN] - last_digit;
    strncpy(buffer, str, size);

    return buffer;
}

char *ltoa(long value, char *buffer, int radix)
{
    long remainder;
    char reverse_buffer[LTOA_BUF_LEN + 1] = {0};
    char *last_digit = &reverse_buffer[LTOA_BUF_LEN - 1];

    if (value < 0) {
        *last_digit = '-';
        last_digit--;
        value *= -1;
    } else if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    for(int i = 0; i < LTOA_BUF_LEN && value; i++) {
        remainder = value % radix;

        if (remainder < 10) {
            *last_digit = remainder + '0';
        } else if (radix == 16) {
            *last_digit = "abcdef"[remainder - 10];
        }

        last_digit--;
        value /= radix;
    }

    char *str = last_digit + 1;
    size_t size = &reverse_buffer[LTOA_BUF_LEN] - last_digit;
    strncpy(buffer, str, size);

    return buffer;
}

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
    //int pos = sprintf(buf, "[%5s.%s%s] ", sec, zeros, rem);
    int pos = sprintf(buf, "[%s.%s%s] ", sec, zeros, rem);
    pos += vsprintf(&buf[pos], format, args);
    pos += sprintf(&buf[pos], "\n\r");
    va_end(args);

    console_write(buf, strlen(buf));
}

int vsprintf(char *str, const char *format, va_list ap)
{
    str[0] = 0;
    char append_str[2] = {0};

    int pos = 0;
    while(format[pos]) {
        if(format[pos] != '%') {
            append_str[0] = format[pos];
            pos++;
            strcat(str, append_str);
            continue;
        }

        switch(format[++pos]) {
            case 's': {
                char *s = va_arg(ap, char *);
                strcat(str, s);
                break;
            }
            case 'c': {
                append_str[0] = (char)va_arg(ap, int);
                strcat(str, append_str);
                break;
            }
            case 'd': {
                int d = va_arg(ap, int);
                char cast[sizeof(int) * 8 + 1];
                strcat(str, itoa(d, cast, 10));
                break;
            }
            case 'u': {
                unsigned int u = va_arg(ap, unsigned int);
                char cast[sizeof(unsigned int) * 8 + 1];
                strcat(str, itoa(u, cast, 10));
                break;
            }
            default:
                break;
        }

        pos++;
    }

    return strlen(str);
}

int sprintf(char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int retval = vsprintf(str, format, args);
    va_end(args);

    return retval;
}
