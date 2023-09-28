#include <tenok.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <kernel/task.h>

#include "uart.h"

#define TEST_STR "fifo: hello world\n\r"
#define LEN      strlen(TEST_STR)

void fifo_task1(void)
{
    setprogname("fifo1");

    mkfifo("/fifo_test", 0);

    int fifo_fd = open("/fifo_test", 0);
    char data[] = TEST_STR;

    while(1) {
        write(fifo_fd, TEST_STR, LEN);
        sleep(1);
    }
}

void fifo_task2(void)
{
    setprogname("fifo2");

    int fifo_fd = open("/fifo_test", 0);
    int serial_fd = open("/dev/serial0", 0);

    while(1) {
        char data[50] = {0};

        /* read fifo */
        read(fifo_fd, &data, LEN);

        /* write serial */
        write(serial_fd, data, LEN);
    }
}

HOOK_USER_TASK(fifo_task1, 0, 512);
HOOK_USER_TASK(fifo_task2, 0, 512);
