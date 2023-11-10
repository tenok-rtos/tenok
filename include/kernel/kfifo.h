/**
 * @file
 */
#ifndef __KFIFO_H__
#define __KFIFO_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct kfifo {
    int start;
    int end;
    int count;
    void *data;
    size_t size;
    size_t esize;
    size_t header_size;
    size_t payload_size;
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
 * @brief  Free the fifo
 * @param  fifo: The fifo object to provide.
 * @retval None
 */
void kfifo_free(struct kfifo *fifo);

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
 * @brief  Get some data from the fifo without removing
 * @param  fifo: The fifo object to provide.
 * @param  data: For returning the retrieved data.
 * @param  n: Number of the data to get from the fifo.
 * @retval None
 */
void kfifo_out_peek(struct kfifo *fifo, void *data, size_t n);

/**
 * @brief  Read data pointer of the next element to write
 * @param  fifo: The fifo object to provide.
 * @param  data_ptr: For saving the data pointer of the next fifo
 *                   element to write with dma.
 * @retval None
 */
void kfifo_dma_in_prepare(struct kfifo *fifo, char **data_ptr);

/**
 * @brief  Complete data writing of the fifo with dma
 * @param  fifo: The fifo object to provide.
 * @param  n: Size of the data that written to the fifo.
 * @retval None
 */
void kfifo_dma_in_finish(struct kfifo *fifo, size_t n);

/**
 * @brief  Read data pointer of the next element to read
 * @param  fifo: The fifo object to provide.
 * @param  data_ptr: For saving the data pointer of the next fifo
 *                   element to read with dma.
 * @param  n: Size of the data to read from the fifo with dma.
 * @retval None
 */
void kfifo_dma_out_prepare(struct kfifo *fifo, char **data_ptr, size_t *n);

/**
 * @brief  Complete data reading of the fifo with dma
 * @param  fifo: The fifo object to provide.
 * @retval None
 */
void kfifo_dma_out_finish(struct kfifo *fifo);

/**
 * @brief  Put data into the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: The pointer of data to put into the fifo.
 * @retval None
 */
void kfifo_put(struct kfifo *fifo, void *data);

/**
 * @brief  Get data from the fifo
 * @param  fifo: The fifo object to provide.
 * @param  data: For returning the retrieved data.
 * @retval None
 */
void kfifo_get(struct kfifo *fifo, void *data);

/**
 * @brief  Get data from the fifo without removing
 * @param  fifo: The fifo object to provide.
 * @param  data: For returning the retrieved data.
 * @retval None
 */
void kfifo_peek(struct kfifo *fifo, void *data);

/**
 * @brief  Return the record length of the next element to read
 * @param  fifo: The fifo object to provide.
 * @retval size_t: Record length if next element to read.
 */
size_t kfifo_peek_len(struct kfifo *fifo);

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
 * @brief  Return the number of elements can be stored in the fifo
 * @param  fifo: The fifo object to provide.
 * @retval size_t: The number of elements can be stored in the fifo.
 */
size_t kfifo_size(struct kfifo *fifo);

/**
 * @brief  Return the size of the kfifo header
 * @param  None
 * @retval size_t: The size of the kfifo header.
 */
size_t kfifo_header_size(void);

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
