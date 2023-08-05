#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "serial.h"

#define IP_ADDR     "127.0.0.1"
#define PORT        4560
#define SERIAL_PORT "/dev/pts/4"

int main(void)
{
    int serial_fd = serial_init(SERIAL_PORT);
    if(serial_fd == -1) {
        printf("failed to connect to the serial.\n");
        exit(0);
    } else {
        printf("established connection to the SIL / HIL.\n");
    }

    int sockfd, connfd;
    struct sockaddr_in cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("failed to create socket.\n");
        exit(0);
    }

    struct sockaddr_in servaddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(IP_ADDR),
        .sin_port = htons(PORT)
    };

    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        printf("failed to connect to the gazeobo.\n");
        exit(0);
    } else {
        printf("established connection to the gazebo.\n");
    }

    /* sil/hil to gazebo forwarding */
    char c;
    while(1) {
        c = serial_getc(serial_fd);
        write(sockfd, &c, 1);
    }

    /* gazebo to sil/hil forwarding */
    while(1) {
        read(sockfd, &c, 1);
        serial_puts(serial_fd, &c, 1);
    }

    close(sockfd);
}
