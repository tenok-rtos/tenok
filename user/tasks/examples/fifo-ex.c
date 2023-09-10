#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "tenok/fcntl.h"
#include "tenok/unistd.h"
#include "tenok/sys/stat.h"
#include "kernel.h"
#include "uart.h"
#include "task.h"

#define TEST_STR "fifo: hello world\n\r"
#define LEN      strlen(TEST_STR)

void my_fifo_init(void)
{
    mkfifo("/fifo_test", 0);
}

void fifo_task1(void)
{
    my_fifo_init();

    set_program_name("fifo1");

    int fifo_fd = open("/fifo_test", 0, 0);
    char data[] = TEST_STR;

    while(1) {
        write(fifo_fd, TEST_STR, LEN);
        sleep(1);
    }
}

void fifo_task2(void)
{
    set_program_name("fifo2");

    int fifo_fd = open("/fifo_test", 0, 0);
    int serial_fd = open("/dev/serial0", 0, 0);

    while(1) {
        char data[50] = {0};

        /* read fifo */
        read(fifo_fd, &data, LEN);

        /* write serial */
        write(serial_fd, data, LEN);
    }
}

HOOK_USER_TASK(fifo_task1);
HOOK_USER_TASK(fifo_task2);
