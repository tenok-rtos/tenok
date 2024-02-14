/**
 * @file
 */
#ifndef __KERNEL_TTY_H__
#define __KERNEL_TTY_H__

ssize_t console_write(const char *buf, size_t size);
void early_write(char *buf, size_t size);

#endif
