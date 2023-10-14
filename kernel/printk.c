#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

#include <kernel/time.h>
#include <kernel/kfifo.h>
#include <kernel/printk.h>

#define PRINTK_BUF_LEN   100
#define PRINTK_FIFO_SIZE 10

bool console_is_free(void);
ssize_t console_write(const char *buf, size_t size);

struct printk_struct {
    char buf[PRINTK_BUF_LEN];
    size_t len;
};

static struct kfifo *printk_fifo;
static struct printk_struct printk_buf;
static int printk_state;

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
    get_sys_time(&tp);

    char sec[15] = {0};
    ltoa(sec, tp.tv_sec, 10);

    char rem[15] = {0};
    ltoa(rem, tp.tv_nsec, 10);

    char zeros[15] = {0};
    for(int i = 0; i < (9 - strlen(rem)); i++) {
        zeros[i] = '0';
    }

    struct printk_struct printk_obj;

    int pos = sprintf(printk_obj.buf, "[%5s.%s%s] ", sec, zeros, rem);
    pos += vsprintf(&printk_obj.buf[pos], format, args);
    pos += sprintf(&printk_obj.buf[pos], "\n\r");
    va_end(args);

    printk_obj.len = strlen(printk_obj.buf);
    kfifo_put(printk_fifo, &printk_obj);
}

void printk_init(void)
{
    printk_fifo = kfifo_alloc(sizeof(struct printk_struct),
                              PRINTK_FIFO_SIZE);

#ifndef BUILD_QEMU
    /* clean screen */
    struct printk_struct printk_obj;
    strcpy(printk_obj.buf, "\x1b[H\x1b[2J");
    printk_obj.len = strlen(printk_obj.buf);
    kfifo_put(printk_fifo, &printk_obj);
#endif
}

void printk_handler(void)
{
    switch(printk_state) {
        case PRINTK_IDLE:
            break;
        case PRINTK_BUSY:
            if(console_is_free())
                printk_state = PRINTK_IDLE;
            else
                return;
    }

    if(!kfifo_is_empty(printk_fifo) && console_is_free()) {
        kfifo_get(printk_fifo, &printk_buf);
        console_write(printk_buf.buf, printk_buf.len);
        printk_state = PRINTK_BUSY;
    }
}
