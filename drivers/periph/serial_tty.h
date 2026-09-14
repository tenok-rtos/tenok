#ifndef __SERIAL_TTY_H__
#define __SERIAL_TTY_H__

#include <stddef.h>
#include <sys/types.h>

#include <fs/fs.h>
#include <sys/termios.h>

#include "uart.h"

/* The line discipline that sits between a reader or a writer and the bytes a
 * serial port carries: what it does to them on the way through, and the
 * waiting it does when there are none. Nothing in it speaks to a register, so
 * every family shares it and each brings only the driver that does the
 * speaking. That driver hands its raw output in as `put`.
 */
void tty_init(uart_dev_t *dev, speed_t speed);
int tty_write(uart_dev_t *dev,
              int (*put)(const char *buf, size_t size),
              const char *buf,
              size_t size);
ssize_t tty_read(uart_dev_t *dev,
                 struct file *filp,
                 int (*put)(const char *buf, size_t size),
                 char *buf,
                 size_t size);
int tty_ioctl(uart_dev_t *dev, unsigned int cmd, unsigned long arg);

#endif
