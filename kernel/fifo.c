#include <stddef.h>
#include <stdint.h>
#include "fs.h"
#include "fifo.h"
#include "mpool.h"
#include "ringbuf.h"
#include "kconfig.h"
#include "kernel.h"

extern struct task_ctrl_blk *running_task;

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations fifo_ops = {
    .read = fifo_read,
    .write = fifo_write
};

int fifo_init(int fd, struct file **files, struct inode *file_inode, struct memory_pool *mem_pool)
{
    /* initialize the pipe */
    struct ringbuf *pipe = memory_pool_alloc(mem_pool, sizeof(struct ringbuf));
    uint8_t *pipe_mem = memory_pool_alloc(mem_pool, sizeof(uint8_t) * PIPE_DEPTH);
    ringbuf_init(pipe, pipe_mem, sizeof(uint8_t), PIPE_DEPTH);

    /* register fifo to the file table */
    pipe->file.f_op = &fifo_ops;
    pipe->file.file_inode = file_inode;
    files[fd] = &pipe->file;

    return 0;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
    struct ringbuf *pipe = container_of(filp, struct ringbuf, file);

    /* block the current task if the request size is larger than the fifo can serve */
    if(size > pipe->count) {
        /* put the current task into the waiting list */
        prepare_to_wait(&pipe->task_wait_list, &running_task->list, TASK_WAIT);

        /* turn on the syscall pending flag */
        running_task->syscall_pending = true;

        return 0;
    } else {
        /* request can be fulfilled, turn off the syscall pending flag */
        running_task->syscall_pending = false;
    }

    /* pop data from the pipe */
    int i;
    for(i = 0; i < size; i++) {
        ringbuf_get(pipe, &buf[i]);
    }

    return size;
}

ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
    struct ringbuf *pipe = container_of(filp, struct ringbuf, file);
    struct list *wait_list = &pipe->task_wait_list;

    /* push data into the pipe */
    int i;
    for(i = 0; i < size; i++) {
        ringbuf_put(pipe, &buf[i]);
    }

    /* resume a blocked task if the pipe has enough data to serve */
    if(!list_is_empty(wait_list)) {
        struct task_ctrl_blk *task = list_entry(wait_list->next, struct task_ctrl_blk, list);

        /* check if request size of the blocked task can be fulfilled */
        if(task->file_request.size <= pipe->count) {
            /* wake up the task from the waiting list */
            wake_up(wait_list);
        }
    }

    return size;
}
