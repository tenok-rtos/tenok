#include <string.h>

#include <kernel/task.h>
#include <kernel/kernel.h>

#include <tenok/tenok.h>
#include <tenok/stdio.h>
#include <tenok/poll.h>
#include <tenok/fcntl.h>
#include <tenok/unistd.h>
#include <tenok/sys/stat.h>

char buffer[100];

void poll_task1(void)
{
    set_program_name("poll1");

    mkfifo("/poll_test", 0);
    int fifo_fd = open("/poll_test", 0);

    char *s = "poll example.\n\r";

    while(1) {
        write(fifo_fd, s, strlen(s));
        sleep(1);
    }
}

void poll_task2(void)
{
    set_program_name("poll2");

    int serial_fd = open("/dev/serial0", 0);
    int fifo_fd = open("/poll_test", O_NONBLOCK);

    struct pollfd fds[1];
    fds[0].fd = fifo_fd;
    fds[0].events = POLLIN;

    char s[100] = {0};

    sleep(3);

    while(1) {
        poll(fds, 1, -1);

        if (fds[0].revents & POLLIN) {
            ssize_t rbytes = read(fifo_fd, buffer, sizeof(buffer) - 1);
            buffer[rbytes] = '\0';

            sprintf(s, "received %d bytes\n\r", rbytes);
            write(serial_fd, s, strlen(s));

            fds[0].revents = 0;
        }
    }
}

HOOK_USER_TASK(poll_task1);
HOOK_USER_TASK(poll_task2);
