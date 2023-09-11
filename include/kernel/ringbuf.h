#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <fs/fs.h>
#include <kernel/spinlock.h>

struct ringbuf {
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

void ringbuf_init(struct ringbuf *rb, void *data, size_t type_size, size_t ring_size);
struct ringbuf *ringbuf_create(size_t nmem, size_t size);
void ringbuf_put(struct ringbuf *rb, const void *data);
void ringbuf_get(struct ringbuf *rb, void *data);
void ringbuf_in(struct ringbuf *rb, const void *data, size_t n);
void ringbuf_out(struct ringbuf *rb, void *data, size_t n);
size_t ringbuf_get_free_space(struct ringbuf *rb);
size_t ringbuf_get_cnt(struct ringbuf *rb);
size_t ringbuf_get_type_size(struct ringbuf *rb);
bool ringbuf_is_empty(struct ringbuf *rb);
bool ringbuf_is_full(struct ringbuf *rb);
struct ringbuf *ringbuf_create(size_t nmem, size_t size);

#endif
