#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "kernel.h"
#include "ringbuf.h"

void ringbuf_init(struct ringbuf *rb, void *data, size_t type_size, size_t ring_size)
{
    rb->start = 0;
    rb->end = 0;
    rb->count = 0;
    rb->data = data;
    rb->type_size = type_size;
    rb->ring_size = ring_size;
    list_init(&rb->task_wait_list);
}

struct ringbuf *ringbuf_create(size_t nmem, size_t size)
{
    struct ringbuf *rb = kmalloc(sizeof(struct ringbuf));
    uint8_t *rb_data = kmalloc(nmem * size);
    ringbuf_init(rb, rb_data, nmem, size);

    return rb;
}

static int ringbuf_increase(struct ringbuf *rb, int ptr)
{
    ptr++;
    if(ptr >= rb->ring_size) {
        ptr = 0;
    }
    return ptr;
}

void ringbuf_put(struct ringbuf *rb, const void *data)
{
    size_t type_size = rb->type_size;

    if(ringbuf_is_full(rb) == true) {
        /* ring buffer is full, overwrite the oldest data and shift the pointer to the next position */
        memcpy((char *)(rb->data + (rb->start * type_size)), (char *)data, type_size);
        rb->start = ringbuf_increase(rb, rb->start);
    } else {
        /* ring buffer is not full, append new data */
        memcpy((char *)(rb->data + (rb->end * type_size)), (char *)data, type_size);
        rb->count++;
    }

    rb->end = ringbuf_increase(rb, rb->end);
}

void ringbuf_get(struct ringbuf *rb, void *data)
{
    size_t type_size = rb->type_size;

    if(rb->count > 0) {
        memcpy((char *)data, (char *)(rb->data + (rb->start * type_size)), type_size);
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
