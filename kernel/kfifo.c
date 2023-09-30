/* kfifo -
 *  kernel fifo based on ring buffer.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <mm/mm.h>
#include <kernel/kernel.h>
#include <kernel/kfifo.h>

void kfifo_init(struct kfifo *fifo, void *data, size_t type_size, size_t ring_size)
{
    fifo->start = 0;
    fifo->end = 0;
    fifo->count = 0;
    fifo->data = data;
    fifo->type_size = type_size;
    fifo->ring_size = ring_size;
    fifo->lock = 0;
    list_init(&fifo->r_wait_list);
    list_init(&fifo->w_wait_list);
}

struct kfifo *kfifo_alloc(size_t nmem, size_t size)
{
    struct kfifo *fifo = kmalloc(sizeof(struct kfifo));
    uint8_t *fifo_data = kmalloc(nmem * size);
    kfifo_init(fifo, fifo_data, nmem, size);

    return fifo;
}

static int kfifo_increase(struct kfifo *fifo, int ptr)
{
    ptr++;
    if(ptr >= fifo->ring_size) {
        ptr = 0;
    }
    return ptr;
}

void kfifo_put(struct kfifo *fifo, const void *data)
{
    size_t type_size = fifo->type_size;

    if(kfifo_is_full(fifo) == true) {
        /* ring buffer is full, overwrite the oldest data and shift the pointer to the next position */
        memcpy((char *)(fifo->data + (fifo->start * type_size)), (char *)data, type_size);
        fifo->start = kfifo_increase(fifo, fifo->start);
    } else {
        /* ring buffer is not full, append new data */
        memcpy((char *)(fifo->data + (fifo->end * type_size)), (char *)data, type_size);
        fifo->count++;
    }

    fifo->end = kfifo_increase(fifo, fifo->end);
}

void kfifo_get(struct kfifo *fifo, void *data)
{
    size_t type_size = fifo->type_size;

    if(fifo->count > 0) {
        memcpy((char *)data, (char *)(fifo->data + (fifo->start * type_size)), type_size);
        fifo->start = kfifo_increase(fifo, fifo->start);
        fifo->count--;
    }
}

void kfifo_in(struct kfifo *fifo, const void *data, size_t n)
{
    for(int i = 0; i < n; i++) {
        kfifo_put(fifo, (char *)data + fifo->type_size * i);
    }
}

void kfifo_out(struct kfifo *fifo, void *data, size_t n)
{
    for(int i = 0; i < n; i++) {
        kfifo_get(fifo, (char *)data + fifo->type_size * i);
    }
}

size_t kfifo_avail(struct kfifo *fifo)
{
    return fifo->ring_size - fifo->count;
}

size_t kfifo_len(struct kfifo *fifo)
{
    return fifo->count;
}

size_t kfifo_esize(struct kfifo *fifo)
{
    return fifo->type_size;
}

bool kfifo_is_empty(struct kfifo *fifo)
{
    return fifo->count == 0;
}

bool kfifo_is_full(struct kfifo *fifo)
{
    return fifo->count == fifo->ring_size;
}
