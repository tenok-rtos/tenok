#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <mm/mm.h>
#include <kernel/list.h>
#include <kernel/kfifo.h>

#define HEADER_SIZE sizeof(struct kfifo_hdr)
#define PAYLOAD_SIZE(fifo) ((fifo)->esize + sizeof(struct kfifo_hdr))

struct kfifo_hdr {
    uint16_t recsize;
};

void kfifo_init(struct kfifo *fifo, void *data, size_t esize, size_t size)
{
    fifo->start = 0;
    fifo->end = 0;
    fifo->count = 0;
    fifo->data = data;
    fifo->esize = esize;
    fifo->size = size;
}

struct kfifo *kfifo_alloc(size_t esize, size_t size)
{
    struct kfifo *fifo = kmalloc(sizeof(struct kfifo));
    uint8_t *fifo_data = kmalloc((esize + HEADER_SIZE) * size);
    kfifo_init(fifo, fifo_data, esize, size);

    return fifo;
}

static int kfifo_increase(struct kfifo *fifo, int ptr)
{
    ptr++;
    if(ptr >= fifo->size)
        ptr = 0;
    return ptr;
}

void kfifo_in(struct kfifo *fifo, const void *data, size_t n)
{
    uint16_t *recsize;
    char *dest;

    if(kfifo_is_full(fifo)) {
        /* ring buffer is full, overwrite the oldest data and
         * shift the pointer to the next position
         */
        recsize = (uint16_t *)((uintptr_t)fifo->data +
                               fifo->start * PAYLOAD_SIZE(fifo));
        dest = (char *)((uintptr_t)recsize + HEADER_SIZE);
        memcpy(dest, data, n);
        fifo->start = kfifo_increase(fifo, fifo->start);
    } else {
        /* ring buffer has space for appending new data */
        recsize = (uint16_t *)((uintptr_t)fifo->data +
                               fifo->end * PAYLOAD_SIZE(fifo));
        dest = (char *)((uintptr_t)recsize + HEADER_SIZE);
        memcpy(dest, data, n);
        fifo->count++;
    }

    *recsize = n;
    fifo->end = kfifo_increase(fifo, fifo->end);
}

void kfifo_out(struct kfifo *fifo, void *data, size_t n)
{
    if(fifo->count <= 0)
        return;

    char *src = (char *)((uintptr_t)fifo->data + HEADER_SIZE +
                         fifo->start * PAYLOAD_SIZE(fifo));
    memcpy(data, src, n);
    fifo->start = kfifo_increase(fifo, fifo->start);
    fifo->count--;
}

void kfifo_peek(struct kfifo *fifo, void *data)
{
    if(fifo->count <= 0)
        return;

    uint16_t *recsize = (uint16_t *)((uintptr_t)fifo->data +
                                     fifo->start * PAYLOAD_SIZE(fifo));
    char *src = (char*)((uintptr_t)recsize + sizeof(struct kfifo_hdr));
    memcpy(data, src, fifo->esize);
}

size_t kfifo_peek_len(struct kfifo *fifo)
{
    uint16_t *rec_size = (uint16_t *)((uintptr_t)fifo->data +
                                      fifo->start * PAYLOAD_SIZE(fifo));
    return *rec_size;
}

void kfifo_put(struct kfifo *fifo, void *data)
{
    kfifo_in(fifo, data, fifo->esize);
}

void kfifo_get(struct kfifo *fifo, void *data)
{
    kfifo_out(fifo, data, fifo->esize);
}

void kfifo_skip(struct kfifo *fifo)
{
    if(fifo->count <= 0)
        return;

    fifo->start = kfifo_increase(fifo, fifo->start);
    fifo->count--;
}

size_t kfifo_avail(struct kfifo *fifo)
{
    return fifo->size - fifo->count;
}

size_t kfifo_len(struct kfifo *fifo)
{
    return fifo->count;
}

size_t kfifo_esize(struct kfifo *fifo)
{
    return fifo->esize;
}

size_t kfifo_size(struct kfifo *fifo)
{
    return fifo->size;
}

bool kfifo_is_empty(struct kfifo *fifo)
{
    return fifo->count == 0;
}

bool kfifo_is_full(struct kfifo *fifo)
{
    return fifo->count == fifo->size;
}
