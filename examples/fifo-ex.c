#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "kernel.h"
#include "syscall.h"
#include "uart.h"
#include "shell.h"

#define TEST_STR "fifo: hello world\n\r"
#define LEN      strlen(TEST_STR)

void fifo_task1(void)
{
    set_program_name("fifo1");

    int fifo_fd = open("/fifo_test", 0, 0);
    char data[] = TEST_STR;

    while(1) {
        write(fifo_fd, TEST_STR, LEN);
        sleep(1000);
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
        shell_puts(data);
    }
}

void run_fifo_example(void)
{
    mknod("/fifo_test", 0, S_IFIFO);

    if(!fork()) fifo_task1();
    if(!fork()) fifo_task2();
}
