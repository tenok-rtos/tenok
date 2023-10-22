#ifndef __KERNEL_TTY_H__
#define __KERNEL_TTY_H__

void tty_init(void);
ssize_t console_write(const char *buf, size_t size);

#endif
