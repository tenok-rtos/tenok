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

    /* initialize kfifo as byte stream mode if esize is 1 */
    if(fifo->esize > 1) {
        /* structured content with header (esize > 1) */
        fifo->header_size = sizeof(struct kfifo_hdr);
        fifo->payload_size = sizeof(struct kfifo_hdr) + esize;
    } else {
        /* byte stream without header (esize == 1) */
        fifo->header_size = 0;
        fifo->payload_size = 1;
    }
}

struct kfifo *kfifo_alloc(size_t esize, size_t size)
{
    /* allocate new kfifo object */
    struct kfifo *fifo = kmalloc(sizeof(struct kfifo));
    if(!fifo)
        return NULL; /* allocation failed */
    kfifo_init(fifo, NULL, esize, size);

    /* allocate buffer space for the kfifo */
    uint8_t *fifo_data = kmalloc(fifo->payload_size * size);
    if(!fifo_data)
        return NULL; /* allocation failed  */
    fifo->data = fifo_data;

    /* return the allocated kfifo object */
    return fifo;
}

static int kfifo_increase(struct kfifo *fifo, int ptr)
{
    ptr++;
    if(ptr >= fifo->size)
        ptr = 0;
    return ptr;
}

void kfifo_in(struct kfifo *fifo, const void *buf, size_t n)
{
    char *data_start;
    char *dest;

    if(kfifo_is_full(fifo)) {
        /* ring buffer is full, overwrite the oldest data and
         * shift the pointer to the next position */
        data_start = (char *)((uintptr_t)fifo->data +
                              fifo->start * fifo->payload_size);
        dest = (char *)((uintptr_t)data_start + fifo->header_size);
        memcpy(dest, buf, n);

        /* update the start position */
        fifo->start = kfifo_increase(fifo, fifo->start);
    } else {
        /* append data at the end as the ring buffer has free space */
        data_start = (char *)((uintptr_t)fifo->data +
                              fifo->end * fifo->payload_size);
        dest = (char *)((uintptr_t)data_start + fifo->header_size);
        memcpy(dest, buf, n);

        /* update the data count */
        fifo->count++;
    }

    /* write the record size field if the fifo is configured
     * as the structured content mode */
    if(fifo->esize > 1)
        *(uint16_t *)data_start = n; /* recsize field */

    /* update the end position */
    fifo->end = kfifo_increase(fifo, fifo->end);
}

void kfifo_out(struct kfifo *fifo, void *buf, size_t n)
{
    /* return if no data to read */
    if(fifo->count <= 0)
        return;

    /* copy the data from the fifo */
    char *src = (char *)((uintptr_t)fifo->data +
                         fifo->header_size +
                         fifo->start * fifo->payload_size);
    memcpy(buf, src, n);

    /* update fifo information */
    fifo->start = kfifo_increase(fifo, fifo->start);
    fifo->count--;
}

void kfifo_peek(struct kfifo *fifo, void *data)
{
    /* return if no data to read */
    if(fifo->count <= 0)
        return;

    /* calculate the start address of the next data */
    char *data_start = (char *)((uintptr_t)fifo->data +
                                fifo->start * fifo->payload_size);

    /* copy the data from the fifo */
    if(fifo->esize > 1) {
        /* structured content mode */
        uint16_t recsize = *(uint16_t *)data_start; /* read size */
        char *src = (char*)((uintptr_t)data_start + sizeof(struct kfifo_hdr));
        memcpy(data, src, recsize);
    } else {
        /* byte stream mode */
        *(char *)data = *data_start;
    }
}

size_t kfifo_peek_len(struct kfifo *fifo)
{
    /* kfifo_peek_len() is not supported under the
     * byte stream mode */
    if(fifo->esize > 1)
        return 0;

    /* read and return the recsize */
    uint16_t *recsize = (uint16_t *)((uintptr_t)fifo->data +
                                     fifo->start * fifo->payload_size);
    return *recsize;
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
