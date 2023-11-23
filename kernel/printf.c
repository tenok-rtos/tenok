#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reent.h>
#include <unistd.h>

#include "kconfig.h"

#define ITOA_BUF_LEN (sizeof(int) * 8)
#define UTOA_BUF_LEN (sizeof(unsigned int) * 8)
#define LTOA_BUF_LEN (sizeof(long) * 8)
#define ULTOA_BUF_LEN (sizeof(unsigned long) * 8)

#define XTOA(TYPE, VALUE, BUF, BUF_SIZE, RADIX)        \
    TYPE remainder;                                    \
    char reverse_BUF[BUF_SIZE + 1] = {0};              \
    char *last_digit = &reverse_BUF[BUF_SIZE - 1];     \
                                                       \
    if (VALUE < 0) {                                   \
        *last_digit = '-';                             \
        last_digit--;                                  \
        VALUE *= -1;                                   \
    } else if (VALUE == 0) {                           \
        BUF[0] = '0';                                  \
        BUF[1] = '\0';                                 \
        return BUF;                                    \
    }                                                  \
                                                       \
    for (int i = 0; i < BUF_SIZE && VALUE; i++) {      \
        remainder = VALUE % RADIX;                     \
                                                       \
        if (remainder < 10) {                          \
            *last_digit = remainder + '0';             \
        } else if (RADIX == 16) {                      \
            *last_digit = "abcdef"[remainder - 10];    \
        }                                              \
                                                       \
        last_digit--;                                  \
        VALUE /= RADIX;                                \
    }                                                  \
                                                       \
    char *str = last_digit + 1;                        \
    size_t size = &reverse_BUF[BUF_SIZE] - last_digit; \
    strncpy(BUF, str, size);                           \
                                                       \
    return BUF;

char *itoa(int value, char *buffer, int radix)
{
    XTOA(int, value, buffer, ITOA_BUF_LEN, radix);
}

char *utoa(unsigned int value, char *buffer, int radix)
{
    XTOA(unsigned int, value, buffer, UTOA_BUF_LEN, radix);
}

char *ltoa(long value, char *buffer, int radix)
{
    XTOA(long, value, buffer, LTOA_BUF_LEN, radix);
}

char *ultoa(unsigned long value, char *buffer, int radix)
{
    XTOA(unsigned long, value, buffer, ULTOA_BUF_LEN, radix);
}

#if (USE_TENOK_PRINTF != 0)

static void format_write(char *str,
                         char *format_str,
                         char *padding_str,
                         int *padding_end,
                         bool *pad_with_zeros)
{
    /* Read length setting */
    padding_str[*padding_end] = '\0';
    *padding_end = 0;
    int desired_len = atoi(padding_str);

    /* Deterimine the padding symbol */
    char symbol = *pad_with_zeros ? '0' : ' ';
    *pad_with_zeros = false;

    /* Check if the format string requires paddings */
    size_t format_str_len = strlen(format_str);
    if (format_str_len >= desired_len) {
        /* no, output without padding */
        strcat(str, format_str);
        return;
    }

    /* Generate paddings */
    size_t padding_size = desired_len - format_str_len;
    for (int i = 0; i < padding_size; i++) {
        padding_str[i] = symbol;
    }
    padding_str[padding_size] = '\0';

    /* Write output */
    strcat(str, padding_str);
    strcat(str, format_str);
}

static int __vsnprintf(char *str,
                       size_t size,
                       bool check_size,
                       const char *format,
                       va_list ap)
{
    str[0] = 0;
    char c_str[2] = {0};

    bool pad_with_zeros = false;

    char buf[100];  // XXX
    int buf_pos = 0;

    int pos = 0;
    while (format[pos]) {
        /* Boundary check */
        if (check_size && pos >= size)
            return strlen(str);

        if (format[pos] != '%') {
            /* Copy */
            c_str[0] = format[pos];
            pos++;
            strcat(str, c_str);

            /* Reset flags */
            pad_with_zeros = false;
            buf_pos = 0;
        } else {
            pos++;

            bool leave = false;
            while (!leave && format[pos] != '\0') {
                /* Boundary check */
                if (check_size && pos >= size)
                    return strlen(str);

                /* Handle specifiers */
                switch (format[pos]) {
                case '0': {
                    if (buf_pos == 0) {
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
                    format_write(str, s, buf, &buf_pos, &pad_with_zeros);
                    leave = true;
                    break;
                }
                case 'c': {
                    c_str[0] = (char) va_arg(ap, int);
                    format_write(str, c_str, buf, &buf_pos, &pad_with_zeros);
                    leave = true;
                    break;
                }
                case 'o': {
                    int o = va_arg(ap, int);
                    char o_str[sizeof(int) * 8 + 1];
                    itoa(o, o_str, 8);
                    format_write(str, o_str, buf, &buf_pos, &pad_with_zeros);
                    leave = true;
                    break;
                }
                case 'x': {
                    int x = va_arg(ap, int);
                    char x_str[sizeof(int) * 8 + 1];
                    itoa(x, x_str, 16);
                    format_write(str, x_str, buf, &buf_pos, &pad_with_zeros);
                    leave = true;
                    break;
                }
                case 'p': {
                    unsigned long p = (long) va_arg(ap, void *);
                    char p_str[sizeof(unsigned long) * 8 + 1];
                    ultoa(p, p_str, 16);
                    strcat(str, "0x");
                    format_write(str, p_str, buf, &buf_pos, &pad_with_zeros);
                    leave = true;
                    break;
                }
                case 'd': {
                    int d = va_arg(ap, int);
                    char d_str[sizeof(int) * 8 + 1];
                    itoa(d, d_str, 10);
                    format_write(str, d_str, buf, &buf_pos, &pad_with_zeros);
                    leave = true;
                    break;
                }
                case 'u': {
                    unsigned int u = va_arg(ap, unsigned int);
                    char u_str[sizeof(unsigned int) * 8 + 1];
                    utoa(u, u_str, 10);
                    format_write(str, u_str, buf, &buf_pos, &pad_with_zeros);
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

int vdprintf(int fd, const char *format, va_list ap)
{
    char buf[PRINT_SIZE_MAX];
    vsnprintf(buf, PRINT_SIZE_MAX, format, ap);

    size_t len = strlen(buf);
    int retval = write(fd, buf, len);

    return retval;
}

int dprintf(int fd, const char *format, ...)
{
    va_list args;

    va_start(args, format);

    char buf[PRINT_SIZE_MAX];
    vsnprintf(buf, PRINT_SIZE_MAX, format, args);

    size_t len = strlen(buf);
    int retval = write(fd, buf, len);

    va_end(args);

    return retval;
}

int vprintf(const char *format, va_list ap)
{
    return vdprintf(STDOUT_FILENO, format, ap);
}

int printf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int retval = vprintf(format, args);
    va_end(args);

    return retval;
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list args;

    va_start(args, format);

    char buf[PRINT_SIZE_MAX];
    vsnprintf(buf, PRINT_SIZE_MAX, format, args);

    __FILE *_stream = (__FILE *) stream;
    size_t len = strlen(buf);
    int retval = write(_stream->fd, buf, len);

    va_end(args);

    return retval;
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
    char buf[PRINT_SIZE_MAX];
    vsnprintf(buf, PRINT_SIZE_MAX, format, ap);

    __FILE *_stream = (__FILE *) stream;
    size_t len = strlen(buf);
    int retval = write(_stream->fd, buf, len);

    return retval;
}
