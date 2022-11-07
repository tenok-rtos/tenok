#include "stddef.h"
#include "stdint.h"
#include "string.h"
#include "ringbuf.h"
#include "stdbool.h"

void ringbuf_init(struct ringbuf *rb, uint8_t *data, size_t type_size, size_t ring_size)
{
	rb->start = 0;
	rb->end = 0;
	rb->count = 0;
	rb->data = data;
	rb->type_size = type_size;
	rb->ring_size = ring_size;
}

static int ringbuf_increase(struct ringbuf *rb, int ptr)
{
	ptr++;
	if(ptr >= rb->ring_size) {
		ptr = 0;
	}
	return ptr;
}

void ringbuf_put(struct ringbuf *rb, const uint8_t *data)
{
	size_t type_size = rb->type_size;

	if(ringbuf_is_full(rb) == true) {
		/* full, overwrite and shift the start pointer
		 * to the next position */
		memcpy(&rb->data[rb->start * type_size], data, type_size);
		rb->start = ringbuf_increase(rb, rb->start);
	} else {
		/* not full, append new data */
		memcpy(&rb->data[rb->end * type_size], data, type_size);
		rb->count++;
	}

	rb->end = ringbuf_increase(rb, rb->end);
}

void ringbuf_get(struct ringbuf *rb, uint8_t *data)
{
	size_t type_size = rb->type_size;

	if(rb->count > 0) {
		memcpy(data, &rb->data[rb->start * rb->type_size], rb->type_size);
		rb->start = ringbuf_increase(rb, rb->start);
		rb->count--;
	}
}

bool ringbuf_is_empty(struct ringbuf *rb)
{
	return rb->count == 0;
}

bool ringbuf_is_full(struct ringbuf *rb)
{
	return rb->count == rb->ring_size;
}
