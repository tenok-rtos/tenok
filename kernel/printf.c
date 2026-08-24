#include <stdarg.h>
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

#define LLTOA_BUF_LEN (sizeof(long long) * 8)

/* Not exported: the printf family needs them for the length modifiers, and
 * one more non standard name in the public headers is one more thing for a
 * ported program to collide with.
 */
static char *ulltoa(unsigned long long value, char *buffer, int radix)
{
    char reversed[LLTOA_BUF_LEN + 1];
    int digits = 0;

    if (value == 0)
        reversed[digits++] = '0';

    while (value) {
        int digit = value % radix;
        reversed[digits++] =
            (digit < 10) ? (digit + '0') : "abcdef"[digit - 10];
        value /= radix;
    }

    int pos = 0;
    while (digits > 0)
        buffer[pos++] = reversed[--digits];

    buffer[pos] = '\0';

    return buffer;
}

static char *lltoa(long long value, char *buffer, int radix)
{
    if (value >= 0)
        return ulltoa((unsigned long long) value, buffer, radix);

    buffer[0] = '-';

    /* Negating the most negative value overflows, so the magnitude is taken
     * in the unsigned type instead
     */
    ulltoa(-(unsigned long long) value, &buffer[1], radix);

    return buffer;
}

/* Bounded output of the printf family. A null buffer or a size of zero
 * measures the output without writing it, which is what a caller sizing a
 * buffer relies on, and the return value is the length the whole output would
 * have had rather than the part that fitted.
 */
struct fmt_out {
    char *buf;
    size_t size;
    size_t len;
};

static void fmt_char(struct fmt_out *out, char c)
{
    if (out->buf && (out->len + 1) < out->size)
        out->buf[out->len] = c;

    out->len++;
}

static void fmt_str(struct fmt_out *out, const char *str, int limit)
{
    for (int i = 0; str[i] && (limit < 0 || i < limit); i++)
        fmt_char(out, str[i]);
}

static void fmt_end(struct fmt_out *out)
{
    if (!out->buf || out->size == 0)
        return;

    out->buf[(out->len < out->size) ? out->len : (out->size - 1)] = '\0';
}

/* Writes one converted item, padded out to the requested width */
static void format_write(struct fmt_out *out,
                         const char *text,
                         int width,
                         int limit,
                         bool pad_with_zeros,
                         bool left_align)
{
    int len = 0;
    while (text[len] && (limit < 0 || len < limit))
        len++;

    if (left_align) {
        fmt_str(out, text, limit);

        /* Zero padding on the right would change the value */
        for (int i = len; i < width; i++)
            fmt_char(out, ' ');

        return;
    }

    for (int i = len; i < width; i++)
        fmt_char(out, pad_with_zeros ? '0' : ' ');

    fmt_str(out, text, limit);
}

static int __vsnprintf(char *str,
                       size_t size,
                       bool check_size,
                       const char *format,
                       va_list ap)
{
    struct fmt_out out = {
        .buf = str,
        .size = check_size ? size : (size_t) -1,
        .len = 0,
    };

    /* Room for the digits of the widest conversion, a sign and a terminator */
    char number[LLTOA_BUF_LEN + 2];
    char c_str[2] = {0};

    int pos = 0;
    while (format[pos]) {
        if (format[pos] != '%') {
            fmt_char(&out, format[pos]);
            pos++;
            continue;
        }

        pos++;

        bool pad_with_zeros = false;
        bool left_align = false;
        int width = 0;
        int limit = -1; /* Precision, -1 when none was given */
        int length = 0; /* 0 for an int, 1 for a long, 2 for a long long */
        bool leave = false;

        /* Flags. A '0' is one of them only before the width digits, and the
         * sign and alternate form flags are accepted and discarded.
         */
        for (;;) {
            if (format[pos] == '-') {
                left_align = true;
            } else if (format[pos] == '0') {
                pad_with_zeros = true;
            } else if (format[pos] != '+' && format[pos] != ' ' &&
                       format[pos] != '#') {
                break;
            }
            pos++;
        }

        if (format[pos] == '*') {
            /* The width comes from the arguments. A negative one asks for
             * the item to be aligned to the left.
             */
            width = va_arg(ap, int);
            pos++;

            if (width < 0) {
                left_align = true;
                width = -width;
            }
        } else {
            while (format[pos] >= '0' && format[pos] <= '9') {
                width = (width * 10) + (format[pos] - '0');
                pos++;
            }
        }

        if (format[pos] == '.') {
            pos++;
            limit = 0;

            if (format[pos] == '*') {
                limit = va_arg(ap, int);
                pos++;
            } else {
                while (format[pos] >= '0' && format[pos] <= '9') {
                    limit = (limit * 10) + (format[pos] - '0');
                    pos++;
                }
            }
        }

        /* Length modifiers. "h" and "hh" need none of their own, what they
         * describe is promoted to an int by the ellipsis.
         */
        while (!leave && format[pos]) {
            switch (format[pos]) {
            case 'h':
                break;
            case 'l':
                if (length < 2)
                    length++;
                break;
            case 'z':
            case 'j':
            case 't':
                /* size_t, intmax_t and ptrdiff_t are as wide as a long here */
                length = 1;
                break;
            default:
                leave = true;
                continue;
            }
            pos++;
        }

        switch (format[pos]) {
        case 's': {
            const char *s = va_arg(ap, char *);
            format_write(&out, s ? s : "(null)", width, limit, pad_with_zeros,
                         left_align);
            break;
        }
        case 'c': {
            c_str[0] = (char) va_arg(ap, int);
            format_write(&out, c_str, width, -1, pad_with_zeros, left_align);
            break;
        }
        case 'd':
        case 'i': {
            long long value;

            if (length == 2)
                value = va_arg(ap, long long);
            else if (length == 1)
                value = va_arg(ap, long);
            else
                value = va_arg(ap, int);
            lltoa(value, number, 10);
            format_write(&out, number, width, -1, pad_with_zeros, left_align);
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            unsigned long long value;

            if (length == 2)
                value = va_arg(ap, unsigned long long);
            else if (length == 1)
                value = va_arg(ap, unsigned long);
            else
                value = va_arg(ap, unsigned int);
            int radix = (format[pos] == 'u')   ? 10
                        : (format[pos] == 'o') ? 8
                                               : 16;
            ulltoa(value, number, radix);

            if (format[pos] == 'X') {
                for (int i = 0; number[i]; i++) {
                    if (number[i] >= 'a' && number[i] <= 'f')
                        number[i] -= 'a' - 'A';
                }
            }

            format_write(&out, number, width, -1, pad_with_zeros, left_align);
            break;
        }
        case 'p': {
            unsigned long value = (unsigned long) va_arg(ap, void *);
            fmt_str(&out, "0x", -1);
            ulltoa(value, number, 16);
            format_write(&out, number, width, -1, pad_with_zeros, left_align);
            break;
        }
        case '%':
            fmt_char(&out, '%');
            break;
        case '\0':
            /* A trailing '%', nothing follows it */
            continue;
        default:
            /* Not a conversion this library knows, copy it as it stands */
            fmt_char(&out, '%');
            fmt_char(&out, format[pos]);
            break;
        }

        pos++;
    }

    fmt_end(&out);

    return (int) out.len;
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

/* Newlib names the integer only forms of these differently and something in
 * the libraries of the toolchain refers to them. Answering here keeps their
 * copies, which define sprintf() and snprintf() in the same object, out of
 * the link. Tenok's printf converts no floating point, so the integer only
 * form is the same function.
 */
int siprintf(char *str, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int retval = vsprintf(str, format, args);
    va_end(args);

    return retval;
}

int sniprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int retval = vsnprintf(str, size, format, args);
    va_end(args);

    return retval;
}

int vsiprintf(char *str, const char *format, va_list ap)
{
    return vsprintf(str, format, ap);
}

int vsniprintf(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
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
    int retval = vdprintf(fd, format, args);
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

int vfprintf(FILE *stream, const char *format, va_list ap)
{
    char buf[PRINT_SIZE_MAX];
    vsnprintf(buf, PRINT_SIZE_MAX, format, ap);

    __FILE *_stream = (__FILE *) stream;
    size_t len = strlen(buf);
    int retval = write(_stream->fd, buf, len);

    return retval;
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int retval = vfprintf(stream, format, args);
    va_end(args);

    return retval;
}
