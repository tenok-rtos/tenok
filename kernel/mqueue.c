#include <stddef.h>
#include "fs.h"
#include "mqueue.h"
#include "util.h"
#include "mpool.h"
#include "ringbuf.h"
#include "kconfig.h"
#include "kernel.h"
#include "syscall.h"

extern tcb_t *running_task;
extern struct memory_pool mem_pool;
extern struct file *files[FILE_CNT_LIMIT];
extern int file_cnt;

static struct file_operations mq_ops;

mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr)
{
	preempt_disable();

	/* if file count reached the limit */
	if(file_cnt >= FILE_CNT_LIMIT) {
		return -1;
	}

	/* initialize the ring buffer pipe */
	struct ringbuf *pipe = memory_pool_alloc(&mem_pool, sizeof(struct ringbuf));
	uint8_t *pipe_mem = memory_pool_alloc(&mem_pool, attr->mq_msgsize * attr->mq_maxmsg);
	ringbuf_init(pipe, pipe_mem, attr->mq_msgsize, attr->mq_maxmsg);

	/* register message queue to the file table */
	struct list *wait_list = &((&pipe->file)->task_wait_list);
	list_init(wait_list);
	pipe->file.f_op = &mq_ops;
	files[file_cnt] = &pipe->file;

	mqd_t mqdes = file_cnt;
	file_cnt++;

	preempt_enable();

	return mqdes;
}

ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
	preempt_disable();

	struct file *filp = files[mqdes];
	struct ringbuf *pipe = container_of(filp, struct ringbuf, file);

	/* block the task if the request size is larger than the mq can serve */
	while(msg_len > pipe->count) {
		/* put the current task into the file waiting list */
		prepare_to_wait(&filp->task_wait_list, &running_task->list, TASK_WAIT);

		/* start sleeping until the resource is available */
		sched_yield();
	}

	/* pop data from the pipe */
	int i;
	for(i = 0; i < msg_len; i++) {
		ringbuf_get(pipe, &msg_ptr[i]);
	}

	preempt_enable();

	return msg_len;
}

int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
	preempt_disable();

	struct file *filp = files[mqdes];
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

	preempt_enable();

	return msg_len;
}
