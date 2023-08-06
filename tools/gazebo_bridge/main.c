#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "serial.h"

#define IP_ADDR     "127.0.0.1"
#define PORT        4560
#define SERIAL_PORT "/dev/pts/4"

pthread_t thread1, thread2;
int serial_fd, socket_fd;
bool terminate = false;

void *thread_gazebo_to_hil(void *arg)
{
    /* sil / hil to gazebo forwarding */
    char c;
    while(1) {
        c = serial_getc(serial_fd);
        write(socket_fd, &c, 1);
    }
}

void *thread_hil_to_gazebo(void *arg)
{
    /* gazebo to sil / hil forwarding */
    char c;
    while(1) {
        read(socket_fd, &c, 1);
        serial_puts(serial_fd, &c, 1);
    }
}

void sig_handler(int signum)
{
    pthread_cancel(thread1);
    pthread_cancel(thread2);

    close(serial_fd);
    close(socket_fd);

    terminate = true;
}

int main(void)
{
    serial_fd = serial_init(SERIAL_PORT);
    if(serial_fd == -1) {
        printf("Failed to connect to the serial.\n");
        exit(1);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        printf("Failed to create socket.\n");
        exit(1);
    }

    struct sockaddr_in servaddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(IP_ADDR),
        .sin_port = htons(PORT)
    };

    if(connect(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr))) {
        printf("Failed to connect to the Gazeobo.\n");
        exit(1);
    }

    printf("Established connection between SIL/HIL and Gazebo.\n");

    pthread_create(&thread1, NULL, thread_gazebo_to_hil, NULL);
    pthread_create(&thread2, NULL, thread_hil_to_gazebo, NULL);

    signal(SIGINT, sig_handler);
    signal(SIGABRT, sig_handler);
    signal(SIGTERM, sig_handler);

    while(!terminate);

    return 0;
}
