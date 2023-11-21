#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <task.h>
#include <tenok.h>
#include <unistd.h>

#include "uart.h"

#define TEST_STR "fifo: hello world\n\r"
#define LEN strlen(TEST_STR)

void fifo_task1(void)
{
    setprogname("fifo-ex-1");

    if (mkfifo("/fifo_test", 0) < 0) {
        exit(1);
    }

    int fifo_fd = open("/fifo_test", O_RDWR);
    if (fifo_fd < 0) {
        exit(1);
    }

    while (1) {
        write(fifo_fd, TEST_STR, LEN);
        sleep(1);
    }
}

void fifo_task2(void)
{
    setprogname("fifo-ex-2");

    int fifo_fd = open("/fifo_test", 0);
    if (fifo_fd < 0) {
        exit(1);
    }

    int serial_fd = open("/dev/serial0", O_RDWR);
    if (serial_fd < 0) {
        exit(1);
    }

    while (1) {
        char data[50] = {0};

        /* Read fifo */
        read(fifo_fd, &data, LEN);

        /* Write serial */
        write(serial_fd, data, LEN);
    }
}

HOOK_USER_TASK(fifo_task1, 0, STACK_SIZE_MIN);
HOOK_USER_TASK(fifo_task2, 0, STACK_SIZE_MIN);
