/**
 * @file
 */
#ifndef __KERNEL_IPC_H__
#define __KERNEL_IPC_H__

#include <stdio.h>

#include <fs/fs.h>
#include <kernel/kfifo.h>

typedef struct kfifo pipe_t;

pipe_t *pipe_create_generic(size_t nmem, size_t size);
ssize_t pipe_read_generic(pipe_t *pipe, char *buf, size_t size);
ssize_t pipe_write_generic(pipe_t *pipe, const char *buf, size_t size);

int fifo_init(int fd, struct file **files, struct inode *file_inode);
ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset);

#endif
