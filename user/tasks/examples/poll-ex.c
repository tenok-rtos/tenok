#include <string.h>

#include <kernel/task.h>
#include <kernel/kernel.h>

#include <tenok/poll.h>
#include <tenok/fcntl.h>
#include <tenok/unistd.h>

char buffer[100];

void poll_task(void)
{
    set_program_name("poll");

    int serial_fd = open("/dev/serial0", 0, 0);
    int mavlink_fd = open("/dev/serial1", 0, O_NONBLOCK);

    struct pollfd fds[1];
    fds[0].fd = serial_fd;
    fds[0].events = POLLIN;

    char s[100] = {0};

    while(1) {
        poll(fds, 1, -1);

        if (fds[0].revents & POLLIN) {
            ssize_t rbytes = read(mavlink_fd, buffer, sizeof(buffer) - 1);
            buffer[rbytes] = '\0';

            sprintf(s, "received %d bytes\n\r", rbytes);
            write(serial_fd, s, strlen(s));

            fds[0].revents = 0;
        }
    }
}

HOOK_USER_TASK(poll_task);
