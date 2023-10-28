#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

#include "kconfig.h"

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

#if (USE_TENOK_PRINTF != 0)

static void format_integer(char *str, char *format_str,
                           char *padding_str, int *padding_end,
                           bool *pad_with_zeros)
{
    /* read length setting */
    padding_str[*padding_end] = '\0';
    *padding_end = 0;
    int desired_len = atoi(padding_str);

    /* deterimine the padding symbol */
    char symbol = *pad_with_zeros ? '0' : ' ';
    *pad_with_zeros = false;

    /* check if the format string requires paddings */
    size_t format_str_len = strlen(format_str);
    if(format_str_len >= desired_len) {
        /* no, output without padding */
        strcat(str, format_str);
        return;
    }


    /* generate paddings */
    size_t padding_size = desired_len - format_str_len;
    for(int i = 0; i < padding_size; i++) {
        padding_str[i] = symbol;
    }
    padding_str[padding_size] = '\0';

    /* write output */
    strcat(str, padding_str);
    strcat(str, format_str);
}

static int __vsnprintf(char *str, size_t size, bool check_size,
                       const char *format, va_list ap)
{
    str[0] = 0;
    char append_str[2] = {0};

    bool pad_with_zeros = false;

    char buf[100]; //XXX
    int buf_pos = 0;

    int pos = 0;
    while(format[pos]) {
        /* boundary check */
        if(check_size && pos >= size)
            return strlen(str);

        if(format[pos] != '%') {
            /* copy */
            append_str[0] = format[pos];
            pos++;
            strcat(str, append_str);

            /* reset flags */
            pad_with_zeros = false;
            buf_pos = 0;
        } else {
            pos++;

            bool leave = false;
            while(!leave && format[pos] != '\0') {
                /* boundary check */
                if(check_size && pos >= size)
                    return strlen(str);

                /* handle specifiers */
                switch(format[pos]) {
                    case '0': {
                        if(buf_pos == 0) {
                            pad_with_zeros = true;
                        } else {
                            buf[buf_pos] = format[pos];
                            buf_pos++;
                        }
                        break;
                    }
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9': {
                        buf[buf_pos] = format[pos];
                        buf_pos++;
                        break;
                    }
                    case '.': {
                        break;
                    }
                    case 's': {
                        char *s = va_arg(ap, char *);
                        strcat(str, s);
                        leave = true;
                        break;
                    }
                    case 'c': {
                        append_str[0] = (char)va_arg(ap, int);
                        strcat(str, append_str);
                        leave = true;
                        break;
                    }
                    case 'x': {
                        int x = va_arg(ap, int);
                        char x_str[sizeof(int) * 8 + 1];
                        itoa(x, x_str, 16);
                        format_integer(str, x_str, buf, &buf_pos,
                                       &pad_with_zeros);
                        leave = true;
                        break;
                    }
                    case 'd': {
                        int d = va_arg(ap, int);
                        char d_str[sizeof(int) * 8 + 1];
                        itoa(d, d_str, 10);
                        format_integer(str, d_str, buf, &buf_pos,
                                       &pad_with_zeros);
                        leave = true;
                        break;
                    }
                    case 'u': {
                        unsigned int u = va_arg(ap, unsigned int);
                        char u_str[sizeof(unsigned int) * 8 + 1];
                        itoa(u, u_str, 10);
                        format_integer(str, u_str, buf, &buf_pos,
                                       &pad_with_zeros);
                        leave = true;
                        break;
                    }
                    default:
                        break;
                }

                pos++;
            }
        }
    }

    return strlen(str);
}

int vsprintf(char *str, const char *format, va_list ap)
{
    return __vsnprintf(str, 0, false, format, ap);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    return __vsnprintf(str, size, true, format, ap);
}

int sprintf(char *str, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int retval = vsprintf(str, format, args);
    va_end(args);

    return retval;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int retval = vsnprintf(str, size, format, args);
    va_end(args);

    return retval;
}

#endif
