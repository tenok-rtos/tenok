/* The knobs of the line discipline, see drivers/periph/uart.c */
#ifndef _TENOK_SYS_TERMIOS_H
#define _TENOK_SYS_TERMIOS_H

#define NCCS 32

/* Input flags */
#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define INPCK 0000020
#define ISTRIP 0000040
#define INLCR 0000100
#define IGNCR 0000200
#define ICRNL 0000400
#define IXON 0002000
#define IXOFF 0010000

/* Output flags */
#define OPOST 0000001 /* Output is processed at all */
#define ONLCR 0000004 /* A newline goes out as a return and a newline */

/* Control flags */
#define CS8 0000060
#define CSIZE 0000060
#define PARENB 0000400

/* Local flags */
#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define ECHOE 0000020
#define ECHOK 0000040
#define ECHONL 0000100
#define IEXTEN 0100000

/* Indices into c_cc */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6

/* Actions of tcsetattr(). Tenok does not buffer, all three are the same */
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define B0 0
#define B9600 13
#define B115200 17

/* Requests of ioctl(), with the numbers Linux gives them */
#define TCGETS 0x5401
#define TCSETS 0x5402
#define TIOCGWINSZ 0x5413

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/**
 * @brief  Read the settings of the terminal a descriptor refers to
 * @param  fd: The file descriptor to provide.
 * @param  termios_p: The buffer for returning the settings.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int tcgetattr(int fd, struct termios *termios_p);

/**
 * @brief  Replace the settings of the terminal a descriptor refers to
 * @param  fd: The file descriptor to provide.
 * @param  actions: When the change takes effect. Tenok does not buffer, so
 *         every one of them takes effect at once.
 * @param  termios_p: The settings to apply.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int tcsetattr(int fd, int actions, const struct termios *termios_p);

#endif
