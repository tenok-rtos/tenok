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
    size_t  size;
    size_t  esize;
    int     flags;

    struct file file;
    struct list_head r_wait_list;
    struct list_head w_wait_list;
};

/**
 * @brief  Initialize a fifo using a preallocated buffer
 * @param  fifo: The fifo object to provide.
 * @param  data: The data space for the fifo.
 * @param  esize: The size of the element managed by the fifo.
 * @param  size: The number of the elements that can fit the fifo.
 * @retval None
 */
void kfifo_init(struct kfifo *fifo, void *data, size_t esize, size_t size);

/**
 * @brief  Dynamically allocate a new fifo buffer
 * @param  esize: The size of the element managed by the fifo.
 * @param  size: The number of the elements that can fit the fifo.
 * @retval kfifo: The dynamically allocated fifo object.
 * @retval None
 */
struct kfifo *kfifo_alloc(size_t nmem, size_t size);

/**
 * @brief  Put data into the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: The pointer of data to put into the fifo.
 * @retval None
 */
#define kfifo_put(fifo, data) \
    kfifo_in((fifo), (data), sizeof(*(data)))

/**
 * @brief  Get data from the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: For returning the retrieved data.
 * @retval None
 */
#define kfifo_get(fifo, data) \
    kfifo_out((fifo), (data), sizeof(*(data)))

/**
 * @brief  Put data into the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: The data to put into the fifo.
 * @param  n: Number of the data to put into the fifo.
 * @retval None
 */
void kfifo_in(struct kfifo *fifo, const void *data, size_t n);

/**
 * @brief  Get data from the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: The memory space for storing retrieved data.
 * @param  n: Number of the data to get from the fifo.
 * @retval None
 */
void kfifo_out(struct kfifo *fifo, void *data, size_t n);

/**
 * @brief  Skip data from the fifo
 * @param  fifo: The fifo object to provide.
 * @retval None
 */
void kfifo_skip(struct kfifo *fifo);

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
 * @brief  Return the record length of the next element to read
 * @param  fifo: The fifo object to provide.
 * @retval size_t: Record length if next element to read.
 */
size_t kfifo_recsize(struct kfifo *fifo);

/**
 * @brief  Return the number of elements can be stored in the fifo
 * @param  fifo: The fifo object to provide.
 * @retval size_t: The number of elements can be stored in the fifo.
 */
size_t kfifo_size(struct kfifo *fifo);

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
