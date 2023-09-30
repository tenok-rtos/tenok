/**
 * @file
 */
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

/**
 * @brief  Initialize a fifo using a preallocated buffer
 * @param  fifo: The fifo object to provide.
 * @param  data: The data space for the fifo.
 * @param  type_size: The size of the element managed by the fifo.
 * @param  ring_size: The number of the elements that can fit the fifo.
 */
void kfifo_init(struct kfifo *fifo, void *data, size_t type_size, size_t ring_size);

/**
 * @brief  Dynamically allocate a new fifo buffer
 * @param  nmem: The size of the element managed by the fifo.
 * @param  size: The number of the elements that can fit the fifo.
 * @retval kfifo: The dynamically allocated fifo object.
 */
struct kfifo *kfifo_alloc(size_t nmem, size_t size);

/**
 * @brief  Put data into the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: The pointer of data to put into the fifo.
 */
void kfifo_put(struct kfifo *fifo, const void *data);

/**
 * @brief  Get data from the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: For returning the retrieved data.
 */
void kfifo_get(struct kfifo *fifo, void *data);

/**
 * @brief  Put data into the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: The data to put into the fifo.
 * @param  n: Number of the data to put into the fifo.
 */
void kfifo_in(struct kfifo *fifo, const void *data, size_t n);

/**
 * @brief  Get data from the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: The memory space for storing retrieved data.
 * @param  n: Number of the data to get from the fifo.
 */
void kfifo_out(struct kfifo *fifo, void *data, size_t n);

/**
 * @brief  Return the number of unused elements in the fifo
 * @param  fifo: The fifo object to provide.
 * @retval size_t: The number of unused elements in the fifo.
 */
size_t kfifo_avail(struct kfifo *fifo);

/**
 * @brief  Return the number of used elements in the fifo
 * @param  fifo: The fifo object to provide.
 * @retval size_t: The number of used elements in the fifo.
 */
size_t kfifo_len(struct kfifo *fifo);

/**
 * @brief  Return the size of the element managed by the fifo
 * @param  fifo: The fifo object to provide.
 * @retval size_t: The size of the element managed by the fifo.
 */
size_t kfifo_esize(struct kfifo *fifo);

/**
 * @brief  Return true if the fifo is empty
 * @param  fifo: The fifo object to provide.
 * @retval bool: True or false.
 */
bool kfifo_is_empty(struct kfifo *fifo);

/**
 * @brief  Return true if the fifo is full
 * @param  fifo: The fifo object to provide.
 * @retval bool: True or false.
 */
bool kfifo_is_full(struct kfifo *fifo);

#endif
