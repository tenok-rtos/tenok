#include <unistd.h>
#include <stddef.h>
#include "file.h"
#include "fifo.h"

ssize_t fifo_read(struct file *filp, char *buf, size_t size, long offset);
ssize_t fifo_write(struct file *filp, const char *buf, size_t size, long offset);

static struct file_operations fifo_ops = {
read:
	fifo_read,
write:
	fifo_write
};

ssize_t fifo_read(struct file *filp, char *buf, size_t size, long offset)
{
}

ssize_t fifo_write(struct file *filp, const char *buf, size_t size, long offset)
{
}
