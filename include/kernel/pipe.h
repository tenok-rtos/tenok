/**
 * @file
 */
#ifndef __KERNEL_PIPE_H__
#define __KERNEL_PIPE_H__

#include <stdio.h>

#include <fs/fs.h>
#include <kernel/kfifo.h>

struct pipe {
    struct kfifo *fifo;
    struct file file;
    struct list_head r_wait_list;
    struct list_head w_wait_list;
};

int fifo_init(int fd, struct file **files,
              struct inode *file_inode, struct pipe *pipe);
ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, off_t offset);

#endif
