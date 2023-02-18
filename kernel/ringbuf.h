#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "fs.h"

struct ringbuf {
	int     start;
	int     end;
	int     count;
	void    *data;
	size_t  ring_size;
	size_t  type_size;

	struct file file;
};

void ringbuf_init(struct ringbuf *rb, void *data, size_t type_size, size_t ring_size);
void ringbuf_put(struct ringbuf *rb, const void *data);
void ringbuf_get(struct ringbuf *rb, void *data);
bool ringbuf_is_empty(struct ringbuf *rb);
bool ringbuf_is_full(struct ringbuf *rb);

#endif
