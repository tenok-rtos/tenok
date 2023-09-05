#include <stddef.h>
#include <stdint.h>
#include "fs.h"
#include "fifo.h"
#include "ringbuf.h"
#include "kconfig.h"
#include "kernel.h"
#include "pipe.h"

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations fifo_ops = {
    .read = fifo_read,
    .write = fifo_write
};

int fifo_init(int fd, struct file **files, struct inode *file_inode)
{
    /* initialize the pipe */
    pipe_t *pipe = generic_pipe_create(sizeof(uint8_t), PIPE_DEPTH);

    /* register fifo to the file table */
    pipe->file.f_op = &fifo_ops;
    pipe->file.file_inode = file_inode;
    files[fd] = &pipe->file;

    return 0;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
    pipe_t *pipe = container_of(filp, struct ringbuf, file);
    return generic_pipe_read(pipe, buf, size, offset);
}

ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
    pipe_t *pipe = container_of(filp, struct ringbuf, file);
    return generic_pipe_write(pipe, buf, size, offset);
}
