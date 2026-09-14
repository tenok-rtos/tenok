#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <poll.h>

#include <fs/fs.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>
#include <kernel/mutex.h>
#include <kernel/preempt.h>
#include <kernel/wait.h>
#include <sys/termios.h>

#include "serial_tty.h"
#include "uart.h"

/*====================*
 * UART common driver *
 *====================*/

/* The settings a terminal starts with. The input side is not processed yet,
 * so nothing is claimed for it
 */
void tty_init(uart_dev_t *dev, speed_t speed)
{
    dev->termios.c_iflag = ICRNL;
    dev->termios.c_oflag = OPOST | ONLCR;
    dev->termios.c_cflag = CS8;
    /* ISIG is stored and nothing here acts on it: Tenok has no foreground task
     * to deliver a signal to. It is what a reader consults to learn that the
     * interrupt character is meant to be special
     */
    dev->termios.c_lflag = ISIG | ICANON | ECHO | ECHOE;
    dev->termios.c_cc[VERASE] = 0x7f;
    dev->termios.c_cc[VEOF] = 0x04;
    dev->termios.c_cc[VINTR] = 0x03;
    dev->termios.c_ispeed = speed;
    dev->termios.c_ospeed = speed;
}

/* Output processing: with ONLCR a newline goes out as a return and a newline.
 * The pair is a local because DMA reads the memory it is handed, and DMA1 has
 * no port to the flash a literal would live in
 */
int tty_write(uart_dev_t *dev,
              int (*put)(const char *buf, size_t size),
              const char *buf,
              size_t size)
{
    const tcflag_t on = OPOST | ONLCR;

    if ((dev->termios.c_oflag & on) != on)
        return put(buf, size);

    char crlf[2] = {'\r', '\n'};
    size_t done = 0;

    while (done < size) {
        size_t run = done;

        while (run < size && buf[run] != '\n')
            run++;

        if (run > done)
            put(&buf[done], run - done);

        if (run < size)
            put(crlf, sizeof(crlf));

        done = (run < size) ? run + 1 : run;
    }

    return size;
}

/* Take one byte, waiting for the sender if there is none yet */
static char tty_getc(uart_dev_t *dev)
{
    char c;

    preempt_disable();
    dev->rx_wait_size = 1;
    wait_event(dev->rx_wait_list, kfifo_len(dev->rx_fifo) > 0);
    kfifo_out(dev->rx_fifo, &c, sizeof(char));
    preempt_enable();

    /* The Enter key sends a return, and a line ends with a newline */
    if ((dev->termios.c_iflag & ICRNL) && c == '\r')
        c = '\n';

    return c;
}

/* Assemble a line, which is what ICANON asks for. A line longer than the
 * buffer is handed over in pieces, the rest of it stays in the device
 */
static ssize_t tty_read_line(uart_dev_t *dev,
                             int (*put)(const char *buf, size_t size),
                             char *buf,
                             size_t size)
{
    size_t len = 0;

    while (len < size) {
        char c = tty_getc(dev);

        /* The end of file character, which ends the line it stands after and
         * ends the file when it stands alone
         */
        if (c == (char) dev->termios.c_cc[VEOF])
            return len;

        if (c == (char) dev->termios.c_cc[VERASE]) {
            if (len > 0) {
                len--;
                if (dev->termios.c_lflag & ECHOE)
                    put("\b \b", 3);
            }
            continue;
        }

        buf[len++] = c;

        if (dev->termios.c_lflag & ECHO)
            put(&c, 1);

        if (c == '\n')
            break;
    }

    return len;
}

/* Hand over whatever arrived, which is what a terminal without ICANON does */
static ssize_t tty_read_raw(uart_dev_t *dev, char *buf, size_t size)
{
    preempt_disable();

    /* Waiting for the full request would hang any reader that asks for more
     * than the sender is about to send
     */
    dev->rx_wait_size = 1;
    wait_event(dev->rx_wait_list, kfifo_len(dev->rx_fifo) > 0);

    size_t avail = kfifo_len(dev->rx_fifo);
    if (size > avail)
        size = avail;

    preempt_enable();

    /* kfifo_out() pops a single element however large its size argument is */
    for (size_t i = 0; i < size; i++) {
        kfifo_out(dev->rx_fifo, &buf[i], sizeof(char));

        /* ICRNL is an input flag, it applies whether a line is assembled or
         * not
         */
        if ((dev->termios.c_iflag & ICRNL) && buf[i] == '\r')
            buf[i] = '\n';
    }

    return size;
}

ssize_t tty_read(uart_dev_t *dev,
                 struct file *filp,
                 int (*put)(const char *buf, size_t size),
                 char *buf,
                 size_t size)
{
    mutex_lock(&dev->rx_mtx);

    /* Remember the file so that the interrupt handler can report readability
     * to poll(). fs_open_file() never calls the open operation of a driver
     */
    dev->filp = filp;

    ssize_t len = (dev->termios.c_lflag & ICANON)
                      ? tty_read_line(dev, put, buf, size)
                      : tty_read_raw(dev, buf, size);

    /* The interrupt handler is masked by the preemption lock, so the buffer
     * cannot refill in between
     */
    preempt_disable();
    if (kfifo_len(dev->rx_fifo) == 0)
        filp->f_events &= ~POLLIN;
    preempt_enable();

    mutex_unlock(&dev->rx_mtx);

    return len;
}

/* Answer the requests of the line discipline. A device that is not a terminal
 * has no settings to give, which is what ENOTTY says
 */
int tty_ioctl(uart_dev_t *dev, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case TCGETS:
        *(struct termios *) arg = dev->termios;
        return 0;
    case TCSETS:
        dev->termios = *(struct termios *) arg;
        return 0;
    default:
        /* A serial line has no window size to report, and nothing else to
         * say either. A caller that asks falls back on its own default
         */
        return -ENOTTY;
    }
}
