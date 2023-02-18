#include <stddef.h>
#include "fs.h"
#include "mqueue.h"
#include "mpool.h"
#include "ringbuf.h"
#include "kconfig.h"
#include "kernel.h"
#include "syscall.h"

extern tcb_t *running_task;

int mqueue_init(int fd, struct file **files, struct mq_attr *attr, struct memory_pool *mem_pool)
{
	/* initialize the ring buffer pipe */
	struct ringbuf *pipe = memory_pool_alloc(mem_pool, sizeof(struct ringbuf));
	uint8_t *pipe_mem = memory_pool_alloc(mem_pool, attr->mq_msgsize * attr->mq_maxmsg);
	ringbuf_init(pipe, pipe_mem, attr->mq_msgsize, attr->mq_maxmsg);

	/* register message queue to the file table */
	struct list *wait_list = &((&pipe->file)->task_wait_list);
	list_init(wait_list);
	pipe->file.f_op = NULL;
	files[fd] = &pipe->file;

	return 0;
}

ssize_t mqueue_receive(struct file *filp, char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
	struct ringbuf *pipe = container_of(filp, struct ringbuf, file);

	/* block the task if the request size is larger than the mq can serve */
	if(msg_len > pipe->count) {
		/* put the current task into the file waiting list */
		prepare_to_wait(&filp->task_wait_list, &running_task->list, TASK_WAIT);

		/* turn on the syscall pending flag */
		running_task->syscall_pending = true;

		return 0;
	} else {
		/* request can be fulfilled, turn off the syscall pending flag */
		running_task->syscall_pending = false;
	}

	/* pop data from the pipe */
	int i;
	for(i = 0; i < msg_len; i++) {
		ringbuf_get(pipe, &msg_ptr[i]);
	}

	return msg_len;
}

ssize_t mqueue_send(struct file *filp, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
	struct ringbuf *pipe = container_of(filp, struct ringbuf, file);
	struct list *wait_list = &filp->task_wait_list;

	/* push data into the pipe */
	int i;
	for(i = 0; i < msg_len; i++) {
		ringbuf_put(pipe, &msg_ptr[i]);
	}

	/* resume a blocked task if the pipe has enough data to serve */
	if(!list_is_empty(wait_list)) {
		tcb_t *task = list_entry(wait_list->next, tcb_t, list);

		/* check if read request size can be fulfilled */
		if(task->file_request.size <= pipe->count) {
			/* wake up the oldest task from the file waiting list */
			wake_up(wait_list);
		}
	}

	return msg_len;
}
