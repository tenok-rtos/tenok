/**
 * @file
 */
#ifndef __KERNEL_PIPE_H__
#define __KERNEL_PIPE_H__

#include <stdbool.h>
#include <stdio.h>

#include <fs/fs.h>
#include <kernel/kfifo.h>

struct pipe {
    struct kfifo *fifo;
    struct file file;
    struct list_head r_wait_list;
    struct list_head w_wait_list;
    /* A pipe of POSIX carries a stream between two descriptors: a read of one
     * takes what is there rather than waiting for the whole request, and finds
     * the end of the stream once the last writer has gone. The pipes the
     * kernel exchanges whole messages through do neither.
     */
    bool stream;
    /* The descriptors that name a stream are what keep it alive */
    unsigned readers;
    unsigned writers;
};

struct file *fifo_init(struct inode *file_inode, struct pipe *pipe);
ssize_t fifo_read(struct file *filp, char *buf, size_t size, off_t offset);
ssize_t fifo_write(struct file *filp,
                   const char *buf,
                   size_t size,
                   off_t offset);

struct file *pipe_alloc(void);
bool file_is_pipe(struct file *filp);
void pipe_release(struct file *filp);
void pipe_take(struct file *filp, bool writer);
void pipe_give_up(struct file *filp, bool writer);

#endif
