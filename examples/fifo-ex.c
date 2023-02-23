#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "syscall.h"
#include "uart.h"

void fifo_task1(void)
{
    set_program_name("fifo1");

    int fifo_fd = open("/fifo_test", 0, 0);
    char data[] = "hello";
    int len = strlen(data);

    while(1) {
        int i;
        for(i = 0; i < len; i++) {
            write(fifo_fd, &data[i], 1);
        }
        sleep(200);
    }
}

void fifo_task2(void)
{
    set_program_name("fifo2");

    int fifo_fd = open("/fifo_test", 0, 0);
    int serial_fd = open("/dev/serial0", 0, 0);

    char data[10] = {0};
    char str[50];

    while(1) {
        read(fifo_fd, &data, 5);
        snprintf(str, 50, "received: %s\n\r", data);
        write(serial_fd, str, strlen(str));
    }
}

void run_fifo_example(void)
{
    mknod("/fifo_test", 0, S_IFIFO);

    if(!fork()) fifo_task1();
    if(!fork()) fifo_task2();
}
