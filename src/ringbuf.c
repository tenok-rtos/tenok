#include "stddef.h"
#include "stdint.h"
#include "ringbuf.h"
#include "stdbool.h"

void ringbuf_init(struct ringbuf *rb, uint8_t *data, size_t size)
{
	rb->start = 0;
	rb->end = 0;
	rb->count = 0;
	rb->data = data;
	rb->size = size;
}

static int ringbuf_increase(struct ringbuf *rb, int ptr)
{
	ptr++;
	if(ptr >= rb->size) {
		ptr = 0;
	}
	return ptr;
}

void ringbuf_put(struct ringbuf *rb, uint8_t new)
{
	if(ringbuf_is_full(rb) == true) {
		/* full, overwrite and shift the start pointer
                 * to the next position */
		rb->data[rb->start] = new;
		rb->start = ringbuf_increase(rb, rb->start);		
	} else {
		/* not full, append new data */
		rb->data[rb->end] = new;
		rb->count++;
	}

	rb->end = ringbuf_increase(rb, rb->end);
}

uint8_t ringbug_get(struct ringbuf *rb)
{
	if(rb->count > 0) {
		uint8_t element = rb->data[rb->start];

		rb->start = ringbuf_increase(rb, rb->start);
		rb->count--;

		return element;
	}
}

bool ringbuf_is_empty(struct ringbuf *rb)
{
	return rb->count == 0;
}

bool ringbuf_is_full(struct ringbuf *rb)
{
	return rb->count == rb->size;
}
