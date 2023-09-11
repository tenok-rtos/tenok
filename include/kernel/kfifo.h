#ifndef __KFIFO_H__
#define __KFIFO_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <fs/fs.h>
#include <kernel/spinlock.h>

struct kfifo {
    int     start;
    int     end;
    int     count;
    void    *data;
    size_t  ring_size;
    size_t  type_size;

    int     flags;

    spinlock_t lock;

    struct file file;
    struct list r_wait_list;
    struct list w_wait_list;
};

void kfifo_init(struct kfifo *fifo, void *data, size_t type_size, size_t ring_size);
struct kfifo *kfifo_alloc(size_t nmem, size_t size);
void kfifo_put(struct kfifo *fifo, const void *data);
void kfifo_get(struct kfifo *fifo, void *data);
void kfifo_in(struct kfifo *fifo, const void *data, size_t n);
void kfifo_out(struct kfifo *fifo, void *data, size_t n);
size_t kfifo_avail(struct kfifo *fifo);
size_t kfifo_len(struct kfifo *fifo);
size_t kfifo_esize(struct kfifo *fifo);
bool kfifo_is_empty(struct kfifo *fifo);
bool kfifo_is_full(struct kfifo *fifo);
struct kfifo *kfifo_alloc(size_t nmem, size_t size);

#endif
