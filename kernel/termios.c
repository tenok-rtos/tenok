#include <sys/ioctl.h>
#include <sys/termios.h>

/* The settings live in the driver, these two carry them across the call */
int tcgetattr(int fd, struct termios *termios_p)
{
    return ioctl(fd, TCGETS, (unsigned long) termios_p);
}

int tcsetattr(int fd, int actions, const struct termios *termios_p)
{
    return ioctl(fd, TCSETS, (unsigned long) termios_p);
}
