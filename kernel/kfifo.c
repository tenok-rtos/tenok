#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <mm/mm.h>
#include <kernel/list.h>
#include <kernel/kfifo.h>

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
    list_init(&fifo->r_wait_list);
    list_init(&fifo->w_wait_list);
}

struct kfifo *kfifo_alloc(size_t esize, size_t size)
{
    struct kfifo *fifo = kmalloc(sizeof(struct kfifo));
    uint8_t *fifo_data = kmalloc((esize + sizeof(struct kfifo_hdr)) * size);
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
    size_t esize = fifo->esize;
    uint16_t *recsize;
    char *src = (char*)data;
    char *dest;

    if(kfifo_is_full(fifo)) {
        /* ring buffer is full, overwrite the oldest data and
         * shift the pointer to the next position
         */
        recsize = (uint16_t *)
                  (fifo->data + fifo->start * (esize + sizeof(struct kfifo_hdr)));
        dest = (char *)recsize + sizeof(struct kfifo_hdr);
        memcpy(dest, src, n);
        fifo->start = kfifo_increase(fifo, fifo->start);
    } else {
        /* ring buffer has space for appending new data */
        recsize = (uint16_t *)
                  (fifo->data + fifo->end * (esize + sizeof(struct kfifo_hdr)));
        dest = (char *)recsize + sizeof(struct kfifo_hdr);
        memcpy(dest, src, n);
        fifo->count++;
    }

    *recsize = n;
    fifo->end = kfifo_increase(fifo, fifo->end);
}

void kfifo_out(struct kfifo *fifo, void *data, size_t n)
{
    if(fifo->count <= 0)
        return;

    char *dest = (char *)data;
    char *src = (char*)(fifo->data + sizeof(struct kfifo_hdr) +
                        fifo->start * (fifo->esize + sizeof(struct kfifo_hdr)));
    memcpy(dest, src, n);
    fifo->start = kfifo_increase(fifo, fifo->start);
    fifo->count--;
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

size_t kfifo_recsize(struct kfifo *fifo)
{
    uint16_t *rec_size = (uint16_t *)
                         (fifo->data + fifo->start *
                          (fifo->esize + sizeof(struct kfifo_hdr)));
    return *rec_size;
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
