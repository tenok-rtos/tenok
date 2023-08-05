#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <string.h>

int serial_init(char *port_name)
{
    //open the port
    int serial_fd = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if(serial_fd == -1) {
        return -1;
    }

    //config the port
    struct termios options;

    tcgetattr(serial_fd, &options);

    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;

    tcflush(serial_fd, TCIFLUSH);
    tcsetattr(serial_fd, TCSANOW, &options);

    return serial_fd;
}

void serial_puts(int serial_fd, char *s, size_t size)
{
    write(serial_fd, s, size);
}

char serial_getc(int serial_fd)
{
    char c;
    int received_len = 0;

    do {
        received_len = read(serial_fd, (void *)&c, 1);
    } while(received_len <= 0);

    return c;
}
