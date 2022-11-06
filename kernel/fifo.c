#include <stddef.h>
#include "file.h"
#include "fifo.h"
#include "util.h"
#include "mpool.h"
#include "ringbuf.h"
#include "kconfig.h"

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset);

static struct file_operations fifo_ops = {
	.read = fifo_read,
	.write = fifo_write
};

int mkfifo(const char *pathname, _mode_t mode)
{
}

int fifo_init(int fd, struct file **files, struct memory_pool *mem_pool)
{
	/* initialize the ring buffer pipe */
	struct ringbuf *pipe = memory_pool_alloc(mem_pool, sizeof(struct ringbuf));
	uint8_t *pipe_mem = memory_pool_alloc(mem_pool, sizeof(uint8_t) * PIPE_DEPTH);
	ringbuf_init(pipe, pipe_mem, PIPE_DEPTH);

	/* register fifo to the file table */
	pipe->file.f_op = &fifo_ops;
	files[fd] = &pipe->file;

	return 0;
}

ssize_t fifo_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
	struct ringbuf *pipe = container_of(filp, struct ringbuf, file);

	int i;
	for(i = 0; i < size; i++) {
		buf[i] = ringbuf_get(pipe);
	}

	return size;
}

ssize_t fifo_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
	struct ringbuf *pipe = container_of(filp, struct ringbuf, file);

	int i;
	for(i = 0; i < size; i++) {
		ringbuf_put(pipe, buf[i]);
	}

	return size;
}
