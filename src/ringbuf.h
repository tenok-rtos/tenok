#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "file.h"

struct ringbuf {
	struct file file;
	int         start;
	int         end;
	int         count;
	uint8_t     *data;
	size_t      size;
};

void ringbuf_init(struct ringbuf *rb, uint8_t *data, size_t size);
void ringbuf_put(struct ringbuf *rb, uint8_t new);
uint8_t ringbuf_get(struct ringbuf *rb);
bool ringbuf_is_empty(struct ringbuf *rb);
bool ringbuf_is_full(struct ringbuf *rb);

#endif
