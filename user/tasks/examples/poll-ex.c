#include <tenok.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <kernel/task.h>
#include <kernel/kernel.h>

static char buffer[100];

void poll_task1(void)
{
    setprogname("poll1");

    char *s = "poll example.\n\r";

    if(mkfifo("/poll_test", 0) < 0) {
        exit(1);
    }

    int fifo_fd = open("/poll_test", O_RDWR);
    if(fifo_fd < 0) {
        exit(1);
    }

    while(1) {
        write(fifo_fd, s, strlen(s));
        sleep(1);
    }
}

void poll_task2(void)
{
    setprogname("poll2");

    int serial_fd = open("/dev/serial0", O_RDWR);
    if(serial_fd < 0) {
        exit(1);
    }

    int fifo_fd = open("/poll_test", O_NONBLOCK);
    if(fifo_fd < 0) {
        exit(1);
    }

    struct pollfd fds[1];
    fds[0].fd = fifo_fd;
    fds[0].events = POLLIN;

    sleep(3);

    while(1) {
        poll(fds, 1, -1);

        if (fds[0].revents & POLLIN) {
            ssize_t rbytes = read(fifo_fd, buffer, sizeof(buffer) - 1);
            buffer[rbytes] = '\0';

            dprintf(serial_fd, "received %d bytes\n\r", rbytes);

            fds[0].revents = 0;
        }
    }
}

HOOK_USER_TASK(poll_task1, 0, 1024);
HOOK_USER_TASK(poll_task2, 0, 1024);
