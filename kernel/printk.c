#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tenok.h>
#include <time.h>
#include <unistd.h>

#include <arch/port.h>
#include <common/list.h>
#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/thread.h>
#include <kernel/time.h>
#include <kernel/tty.h>
#include <kernel/wait.h>

#include "kconfig.h"

#define PRINTK_QUEUE_SIZE 10

struct printk_data {
    struct list_head list;
    size_t size;
    char data[PRINT_SIZE_MAX];
};

static struct printk_data printk_buf[PRINTK_QUEUE_SIZE];

static LIST_HEAD(printkd_wait);
static LIST_HEAD(printk_free_list);
static LIST_HEAD(printk_wait_list);

static bool stdout_initialized;
static bool printk_is_writing;

ssize_t console_write(const char *buf, size_t size)
{
    struct printk_data *entry;

    /* Check if the free list still has space */
    if (list_empty(&printk_free_list)) {
        /* No, overwrite the oldest buffer */
        entry = list_first_entry(&printk_wait_list, struct printk_data, list);
        /* Move the buffer to the end of the wait queue */
        list_del(&entry->list);
        list_add(&entry->list, &printk_wait_list);
    } else {
        /* Yes, take one buffer from the free queue */
        entry = list_first_entry(&printk_free_list, struct printk_data, list);
        /* Move the buffer to the wait queue */
        list_move(&entry->list, &printk_wait_list);
    }

    /* Copy write message to the prink buffer */
    memcpy(entry->data, buf, sizeof(char) * size);
    entry->size = size;

    /* Wake up the printk daemon */
    if (!printk_is_writing)
        wake_up_all(&printkd_wait);

    return 0;
}

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

void panic(char *format, ...)
{
    char buf[1024] = {0};

    va_list args;
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);
    early_write(buf, strlen(buf));

    va_end(args);

    while (1)
        ;
}

void printkd_init(void)
{
    /* Place all prink buffers to the free queue */
    for (int i = 0; i < PRINTK_QUEUE_SIZE; i++)
        list_add(&printk_buf[i].list, &printk_free_list);
}

void printkd_start(void)
{
    /* Initiate the printk daemon */
    stdout_initialized = true;
    wake_up_all(&printkd_wait);
}

static void printkd_sleep(void)
{
    prepare_to_wait(&printkd_wait, current_thread_info(), THREAD_WAIT);
    schedule();
}

void printkd(void)
{
    setprogname("printk");

    struct printk_data *entry;

    /* Wait until stdout is ready */
    while (!stdout_initialized)
        printkd_sleep();

    while (1) {
        /* Check if there is any printk message to write */
        if (list_empty(&printk_wait_list)) {
            /* No, suspend the daemon */
            printkd_sleep();
        } else {
            /* Pop one prink data from the wait queue */
            preempt_disable();
            entry =
                list_first_entry(&printk_wait_list, struct printk_data, list);
            list_del(&entry->list);
            preempt_enable();

            /* Write printk message to the serial */
            printk_is_writing = true;
            write(STDOUT_FILENO, entry->data, entry->size);
            printk_is_writing = false;

            /* Place the used buffer back to the free queue */
            preempt_disable();
            list_add(&entry->list, &printk_free_list);
            preempt_enable();
        }
    }
}
